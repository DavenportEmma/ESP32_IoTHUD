#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "driver/i2c.h"
#include "font8px.h"
#include "font16px.h"
#include "sdkconfig.h"
#include "mqtt_client.h"
#include "../parson/parson.h"
#include "../parson/parson.c"

static const char *TAG = "MQTTWS_I2C_DISPLAY";

static EventGroupHandle_t wifi_event_group;
static SemaphoreHandle_t xSemaphore = NULL;
static int CONNECTED_BIT = BIT0;

static char jsonString[1024 * 4] = "";

int patientIndex = 0; 
struct data {
    bool valid;
    char loinc[32];
    char name[256];
    float value;
    char units[256];
    char reference[256];
    char description[256];
} patient[3];

#define _I2C_NUMBER(num) I2C_NUM_##num
#define I2C_NUMBER(num) _I2C_NUMBER(num)

#define DATA_LENGTH 512                  /*!< Data buffer length of test buffer */
#define RW_TEST_LENGTH 128               /*!< Data length for r/w test, [0,DATA_LENGTH] */
#define DELAY_TIME_BETWEEN_ITEMS_MS 1000 /*!< delay time between different test items */

#define I2C_MASTER_SCL_IO CONFIG_I2C_MASTER_SCL               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO CONFIG_I2C_MASTER_SDA               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUMBER(CONFIG_I2C_MASTER_PORT_NUM) /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ CONFIG_I2C_MASTER_FREQUENCY        /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */
#define WIRE_MAX 32
#define WRITE_BIT 0
#define ACK_CHECK_EN 0x1                        /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0 

#define ssd1306_swap(a, b) \
  (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b))) ///< No-temp-var swap operation

#define SSD1306_ADDR 0x3C	// i2c address of display module
#define HEIGHT 64			// 128 x 64 px display
#define WIDTH 128

#define BLACK                          0 ///< Draw 'off' pixels
#define WHITE                          1 ///< Draw 'on' pixels
#define INVERSE                        2 ///< Invert pixels

#define SSD1306_MEMORYMODE          0x20 ///< See datasheet
#define SSD1306_COLUMNADDR          0x21 ///< See datasheet
#define SSD1306_PAGEADDR            0x22 ///< See datasheet
#define SSD1306_SETCONTRAST         0x81 ///< See datasheet
#define SSD1306_CHARGEPUMP          0x8D ///< See datasheet
#define SSD1306_SEGREMAP            0xA0 ///< See datasheet
#define SSD1306_DISPLAYALLON_RESUME 0xA4 ///< See datasheet
#define SSD1306_DISPLAYALLON        0xA5 ///< Not currently used
#define SSD1306_NORMALDISPLAY       0xA6 ///< See datasheet
#define SSD1306_INVERTDISPLAY       0xA7 ///< See datasheet
#define SSD1306_SETMULTIPLEX        0xA8 ///< See datasheet
#define SSD1306_DISPLAYOFF          0xAE ///< See datasheet
#define SSD1306_DISPLAYON           0xAF ///< See datasheet
#define SSD1306_COMSCANINC          0xC0 ///< Not currently used
#define SSD1306_COMSCANDEC          0xC8 ///< See datasheet
#define SSD1306_SETDISPLAYOFFSET    0xD3 ///< See datasheet
#define SSD1306_SETDISPLAYCLOCKDIV  0xD5 ///< See datasheet
#define SSD1306_SETPRECHARGE        0xD9 ///< See datasheet
#define SSD1306_SETCOMPINS          0xDA ///< See datasheet
#define SSD1306_SETVCOMDETECT       0xDB ///< See datasheet

#define SSD1306_SETLOWCOLUMN        0x00 ///< Not currently used
#define SSD1306_SETHIGHCOLUMN       0x10 ///< Not currently used
#define SSD1306_SETSTARTLINE        0x40 ///< See datasheet

#define SSD1306_EXTERNALVCC         0x01 ///< External display voltage source
#define SSD1306_SWITCHCAPVCC        0x02 ///< Gen. display voltage from 3.3V

#define SSD1306_RIGHT_HORIZONTAL_SCROLL              0x26 ///< Init rt scroll
#define SSD1306_LEFT_HORIZONTAL_SCROLL               0x27 ///< Init left scroll
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< Init diag scroll
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A ///< Init diag scroll
#define SSD1306_DEACTIVATE_SCROLL                    0x2E ///< Stop scroll
#define SSD1306_ACTIVATE_SCROLL                      0x2F ///< Start scroll
#define SSD1306_SET_VERTICAL_SCROLL_AREA             0xA3 ///< Set scroll range

uint8_t *buffer;
int8_t vccstate;
uint8_t rotation = 0;

// writes to slave device
// i2c_num - i2c port number
// *data_wr - pointer to array of bytes
// size - size of array
esp_err_t i2c_master_write_slave(i2c_port_t i2c_num, uint8_t *data_wr, size_t size)
{
	// create and initialise i2c command link
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();	
    // queue command for i2c master to generate a start signal
    i2c_master_start(cmd);	
    // queue command for master to write 1 byte to i2c bus
    // this line selects the slave device
    // ACK_CHECK_EN - enable ACK check for master
    i2c_master_write_byte(cmd, (SSD1306_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    // queue command for master to write buffer to i2c bus
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    // generate stop signal
    i2c_master_stop(cmd);
    // master sends queued commands
    // i2c_num - i2c port number
    // portTICK_RATE_MS - maximum wait ticks. If the queue is full for that amount of ticks, the call aborts instead of waiting longer
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    // free i2c command link
    i2c_cmd_link_delete(cmd);
    return ret;
}

// initiates master
esp_err_t i2c_master_init()
{
	// set master port
    int i2c_master_port = I2C_MASTER_NUM;
    // create i2c_config_t object
    i2c_config_t conf;
    // set i2c mode to master
    conf.mode = I2C_MODE_MASTER;
    // gpio number for master data
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    // conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    // ssd1306 has 3.3v pull ups
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;  
    // gpio number for master clock 
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    //conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    // set master clock frequency
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    // i2c parameter initialisation
    i2c_param_config(i2c_master_port, &conf);
    // i2c driver install
    // I2C_MASTER_RX_BUF_DISABLE - disable receive buffer for master
    // I2C_MASTER_TX_BUF_DISABLE - disable transmit buffer for master
    // intr_alloc_flags - allocate interrupts
    return i2c_driver_install(i2c_master_port, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

// send a single command to the display
void command(uint8_t c)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();	
    // queue command for i2c master to generate a start signal
    i2c_master_start(cmd);	
    // queue command for master to write 1 byte to i2c bus
    // this line selects the slave device
    // ACK_CHECK_EN - enable ACK check for master
    i2c_master_write_byte(cmd, (SSD1306_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    // queue command for master to write buffer to i2c bus
    // 0x00 - after the slave address is written to the bus the control byte must follow
    /*
	| 7 bit			| 1 bit	||				|			|
	| slave address | R/W#	|| control byte | data byte | ...
	|				|		|| Co | D/C#	|			|
    */
    // Co - continuation bit. 0 = transmission of the following information
    // will contain data bytes only
    // D/C# - data/command selection bit. determines the next data byte is
    // acted as a command or a data. 0 = the following byte is a command.
    // 1 = the following byte is data which will be stored at GDDRAM. GDDRAM
    // col. addr pointer is increased by one automatically after each data write
    i2c_master_write_byte(cmd, 0x00, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, c, ACK_CHECK_EN);
    // generate stop signal
    i2c_master_stop(cmd);
    // master sends queued commands
    // i2c_num - i2c port number
    // portTICK_RATE_MS - maximum wait ticks. If the queue is full for that amount of ticks, the call aborts instead of waiting longer
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    if(ret != 0)
        return;
    // free i2c command link
    i2c_cmd_link_delete(cmd);
}

// clears the display
void clearDisplay()
{
    // sets the entire buffer to 0
    memset(buffer, 0, WIDTH * ((HEIGHT + 7) / 8));
}

// fills the display with white
void fillDisplay()
{
    // sets the entire buffer to 0xFF
    memset(buffer, 0xFF, WIDTH * ((HEIGHT + 7) / 8));
}

// clears a rectangle of the screen to black
// param y in pages
void clearBox(int x1, int y1, int x2, int y2)
{
    int i,j;
    for(i = y1; i < y2; i++)
    {
        for(j = x1; j < x2; j++)
        {
            buffer[j + (i * 128)] = 0;
        }
    }
}

void begin(uint8_t vcs, uint8_t addr) 
{
    // allocate memory to buffer
    buffer = (uint8_t*) pvPortMalloc(WIDTH * ((HEIGHT + 7) / 8));

	clearDisplay();
	vccstate = vcs;

	// display initialisation sequence
	command(SSD1306_DISPLAYOFF);			// 0xAE
	command(SSD1306_SETDISPLAYCLOCKDIV);	// 0xD5
	command(0x80);							// the suggested ratio
	command(SSD1306_SETMULTIPLEX);			// 0xA8
	command(HEIGHT - 1);

	command(SSD1306_SETDISPLAYOFFSET);		// 0xD3
	command(0x0);							// no offset
	command(SSD1306_SETSTARTLINE | 0x0);	// line #0
	command(SSD1306_CHARGEPUMP);			// 0x8D
              
	command((vccstate == SSD1306_EXTERNALVCC) ? 0x10 : 0x14);

	command(SSD1306_MEMORYMODE);			// 0x20
	command(0x00);							// horizontal addressing mode
	command(SSD1306_SEGREMAP | 0x1);		// col. addr 0 is mapped to SEG0
	command(SSD1306_COMSCANDEC);			// 0xC8scan from COM[N-1] to COM0

	command(SSD1306_SETCOMPINS);			//0xDA
	command(0x12);							// alternative COM pin config
											// disable COM Left/Right remap
	command(SSD1306_SETCONTRAST);			// 0x81
	command((vccstate == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF);

  	command(SSD1306_SETPRECHARGE); // 0xd9
  	command((vccstate == SSD1306_EXTERNALVCC) ? 0x22 : 0xF1);

  	command(SSD1306_SETVCOMDETECT);			// 0xDB
  	command(0x40);
  	command(SSD1306_DISPLAYALLON_RESUME);	// 0xA4
  	command(SSD1306_NORMALDISPLAY);			// 0xA6
  	command(SSD1306_DEACTIVATE_SCROLL);		
  	command(SSD1306_DISPLAYON);				// / Main screen turn on
}

// sends the buffer contents to the display
void display() 
{
    // initialises the display RAM
	command(SSD1306_PAGEADDR);
	command(0);
	command(0xFF);
	command(SSD1306_COLUMNADDR);
	command(0);
	command(WIDTH - 1);
	uint16_t count = WIDTH * ((HEIGHT + 7) / 8);
	uint8_t *ptr   = buffer;
    // create i2c link
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // begin tx
    i2c_master_start(cmd);
 
    // configure display to receive data
    i2c_master_write_byte(cmd, (SSD1306_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN);
    uint8_t bytesOut = 1;
    esp_err_t ret;
    while(count--)
    {
        // if outgoing buffer is full
    	if(bytesOut >= WIRE_MAX)
    	{
            // stop bit
    		i2c_master_stop(cmd);
            // begin the tx
    		ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
            if(ret != 0)
                return;
            // delete link
            i2c_cmd_link_delete(cmd);
    		cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
    		i2c_master_write_byte(cmd, (SSD1306_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    		i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN);
    		bytesOut = 1;
    	}
        // write bytes from buffer to outgoing buffer
    	i2c_master_write_byte(cmd, *ptr++, ACK_CHECK_EN);
        bytesOut++;
    }
    // finish tx
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    if(ret != 0)
        return;
    i2c_cmd_link_delete(cmd);
}

void drawPixel(int16_t x, int16_t y, uint16_t colour)
{
	if((x >= 0) && (x < WIDTH && (y >= 0) && (y < HEIGHT))) 
	{
	    // Pixel is in-bounds. Rotate coordinates if needed.
	    switch(rotation) 
	    {
	    	case 1:
	    		ssd1306_swap(x, y);
	    		x = WIDTH - x - 1;
	    		break;
	    	case 2:
	    		x = WIDTH  - x - 1;
	    		y = HEIGHT - y - 1;
	    		break;
	    	case 3:
	    		ssd1306_swap(x, y);
	    		y = HEIGHT - y - 1;
	    		break;
	    }
	    switch(colour) 
	    {
	    	case WHITE:   buffer[x + (y/8)*WIDTH] |=  (1 << (y&7)); break;
	    	case BLACK:   buffer[x + (y/8)*WIDTH] &= ~(1 << (y&7)); break;
	    	case INVERSE: buffer[x + (y/8)*WIDTH] ^=  (1 << (y&7)); break;
        }
	}
}

// draws horizontal line
void HLine(int16_t x, int16_t y, int16_t w, uint16_t colour) 
{
    if((y >= 0) && (y < HEIGHT)) 
    { // Y coord in bounds?
        if(x < 0) 
        {   // Clip left
            w += x;
            x  = 0;
        }
        if((x + w) > WIDTH) 
        {   // Clip right
            w = (WIDTH - x);
        }
        if(w > 0) 
        {   // Proceed only if width is positive
            uint8_t *pBuf = &buffer[(y / 8) * WIDTH + x], mask = 1 << (y & 7);
            switch(colour) 
            {
                case WHITE:               while(w--) { *pBuf++ |= mask; }; break;
                case BLACK: mask = ~mask; while(w--) { *pBuf++ &= mask; }; break;
                case INVERSE:             while(w--) { *pBuf++ ^= mask; }; break;
            }
        }
    } 
}

// draws vertical line
// x = start x position
// y = start y position in bits
// h = length
void VLine(int16_t x, int16_t y, int16_t h, uint16_t colour)
{
    int i;
    // param y is starting bit number, not page number
    // y = 0 -> 63
    // startPageNumber finds which page the line starts on
    int startPageNumber = (y - (y % 8))/8;
    // startBitNumber finds which bit the line starts on
    int startBitNumber = y - (startPageNumber * 8);
    // firstByteMask masks the bits in the first byte that are above the line
    int firstByteMask = 0xFF << startBitNumber;

    // if the line starts and ends in the same byte it will just display
    // a full byte. This is because of the interaction between the firstByteMask
    // and the lastByteMask
    if((startBitNumber + h) > 8)
    {
        // remaining length in bits
        int remainingLength = h - (8 - (y % 8));
        // how many full 0xFF bytes are left
        int remainingFullPages = (remainingLength - (remainingLength % 8)) / 8;
        // last byte might not be 0xFF
        int lastByte = h - ((remainingFullPages * 8 ) + ( 8 - startBitNumber));
        // masks the bits in the last byte that are below the end of the line
        int lastByteMask = 0xFF >> (8 - lastByte);
        // fill the first byte
        buffer[x + (128 * startPageNumber)] = (0xFF & firstByteMask);

        // fill the full pages in the middle
        for(i = 1; i <= remainingFullPages; i++)
        {
            buffer[x + ((startPageNumber + i) * 128)] = 0xFF;
        }

        buffer[x + ((startPageNumber + remainingFullPages + 1) * 128)] = 0xFF & lastByteMask;
    }
    else
    {
        int endMask = 0xFF >> (8 - h);
        int mask = firstByteMask & endMask;
        buffer[x + (startPageNumber * 128)] = mask;
    }

}

// draw a character from the ascii.h file
// characters in this file are 5x8b
void drawChar8(unsigned char c[5], int16_t x, int16_t y)
{
    int i;
    for(i = 0; i < 5; i++)
    {
        if(((i + x) + (128 * y)) > 1023)
            break;
        buffer[(i + x) + (128 * y)] = c[i];
    }
}

// draw a character from the font16px.h file
void drawChar16(unsigned char c[10][2], int16_t x, int16_t y)
{
    int i, j;
    for(i = 0; i < 2; i++)
    {
        for(j = 0; j < 10; j++)
        {
            if((j + x) + (128 * (i + y)) > 1023)
                break;
            buffer[(j + x) + (128 * (i + y))] = c[j][i];
        }
    }
}

// param t: type of font to be used, 8 or 16 px
void drawCharFromString(char c, int x, int y, int t)
{
    if(t == 16)
    {
        switch(c)
        {
            case '0': drawChar16(zero16,x,y);       break;
            case '1': drawChar16(one16,x,y);        break;
            case '2': drawChar16(two16,x,y);        break;
            case '3': drawChar16(three16,x,y);      break;
            case '4': drawChar16(four16,x,y);       break;
            case '5': drawChar16(five16,x,y);       break;
            case '6': drawChar16(six16,x,y);        break;
            case '7': drawChar16(seven16,x,y);      break;
            case '8': drawChar16(eight16,x,y);      break;
            case '9': drawChar16(nine16,x,y);       break;
            case 'a': case 'A': drawChar16(A16,x,y);  break;    
            case 'b': case 'B': drawChar16(B16,x,y);  break;    
            case 'c': case 'C': drawChar16(C16,x,y);  break;    
            case 'd': case 'D': drawChar16(D16,x,y);  break;    
            case 'e': case 'E': drawChar16(E16,x,y);  break;    
            case 'f': case 'F': drawChar16(F16,x,y);  break;    
            case 'g': case 'G': drawChar16(G16,x,y);  break;    
            case 'h': case 'H': drawChar16(H16,x,y);  break;    
            case 'i': case 'I': drawChar16(I16,x,y);  break;    
            case 'j': case 'J': drawChar16(J16,x,y);  break;    
            case 'k': case 'K': drawChar16(K16,x,y);  break;    
            case 'l': case 'L': drawChar16(L16,x,y);  break;    
            case 'm': case 'M': drawChar16(M16,x,y);  break;    
            case 'n': case 'N': drawChar16(N16,x,y);  break;    
            case 'o': case 'O': drawChar16(O16,x,y);  break;    
            case 'p': case 'P': drawChar16(P16,x,y);  break;    
            case 'q': case 'Q': drawChar16(Q16,x,y);  break;    
            case 'r': case 'R': drawChar16(R16,x,y);  break;    
            case 's': case 'S': drawChar16(S16,x,y);  break;    
            case 't': case 'T': drawChar16(T16,x,y);  break;    
            case 'u': case 'U': drawChar16(U16,x,y);  break;    
            case 'v': case 'V': drawChar16(V16,x,y);  break;    
            case 'w': case 'W': drawChar16(W16,x,y);  break;    
            case 'x': case 'X': drawChar16(X16,x,y);  break;    
            case 'y': case 'Y': drawChar16(Y16,x,y);  break;    
            case 'z': case 'Z': drawChar16(Z16,x,y);  break;
            case '.': drawChar16(point16,x,y);    break;
            case '/': drawChar16(slash16,x,y);    break;      
            default: break;
        }
    }
    else if(t == 8)
    {
        switch(c)
        {
            case '0': drawChar8(zero8,x,y);       break;
            case '1': drawChar8(one8,x,y);        break;
            case '2': drawChar8(two8,x,y);        break;
            case '3': drawChar8(three8,x,y);      break;
            case '4': drawChar8(four8,x,y);       break;
            case '5': drawChar8(five8,x,y);       break;
            case '6': drawChar8(six8,x,y);        break;
            case '7': drawChar8(seven8,x,y);      break;
            case '8': drawChar8(eight8,x,y);      break;
            case '9': drawChar8(nine8,x,y);       break;
            case 'a': case 'A': drawChar8(A8,x,y);  break;    
            case 'b': case 'B': drawChar8(B8,x,y);  break;    
            case 'c': case 'C': drawChar8(C8,x,y);  break;    
            case 'd': case 'D': drawChar8(D8,x,y);  break;    
            case 'e': case 'E': drawChar8(E8,x,y);  break;    
            case 'f': case 'F': drawChar8(F8,x,y);  break;    
            case 'g': case 'G': drawChar8(G8,x,y);  break;    
            case 'h': case 'H': drawChar8(H8,x,y);  break;    
            case 'i': case 'I': drawChar8(I8,x,y);  break;    
            case 'j': case 'J': drawChar8(J8,x,y);  break;    
            case 'k': case 'K': drawChar8(K8,x,y);  break;    
            case 'l': case 'L': drawChar8(L8,x,y);  break;    
            case 'm': case 'M': drawChar8(M8,x,y);  break;    
            case 'n': case 'N': drawChar8(N8,x,y);  break;    
            case 'o': case 'O': drawChar8(O8,x,y);  break;    
            case 'p': case 'P': drawChar8(P8,x,y);  break;    
            case 'q': case 'Q': drawChar8(Q8,x,y);  break;    
            case 'r': case 'R': drawChar8(R8,x,y);  break;    
            case 's': case 'S': drawChar8(S8,x,y);  break;    
            case 't': case 'T': drawChar8(T8,x,y);  break;    
            case 'u': case 'U': drawChar8(U8,x,y);  break;    
            case 'v': case 'V': drawChar8(V8,x,y);  break;    
            case 'w': case 'W': drawChar8(W8,x,y);  break;    
            case 'x': case 'X': drawChar8(X8,x,y);  break;    
            case 'y': case 'Y': drawChar8(Y8,x,y);  break;    
            case 'z': case 'Z': drawChar8(Z8,x,y);  break; 
            case '[': drawChar8(openSquare8,x,y);   break;
            case ']': drawChar8(closeSquare8,x,y);  break;
            case '/': drawChar8(slash8,x,y);        break;
            case '.': drawChar8(point8,x,y);        break;
            default: break;
        }
    }
}

void drawInteger(int n, int x, int y, int t)
{
    int i;
    int j = 0;
	char digit[3];
	sprintf(digit,"%i",n);
    for(i = 0; i < 3; i++)
    {
        drawCharFromString(digit[i],x+j,y,t);
        j += 12;
    }
}

void drawFloat(float n, int x, int y, int t)
{
    int i;
    int j = 0;
    char digit[9];
    sprintf(digit,"%f",n);
    //remove trailing zeros
    for(i = 8; i > 0; i--)
    {
        if(digit[i] == '0' || digit[i] == '\0')
			digit[i] = '\0';

		else
			break;
    }
    for(i = 0; i < 9; i++)
    {
        drawCharFromString(digit[i],x+j,y,t);
        j += 12;
    }
}

// param w: wrap text
void drawString(char s[], int x, int y, int t, bool w)
{
    int i;
    int l = strlen(s);
    
    if(w == 0)
    {
        if(t == 16 && l > 10)
        {
            l = 10;
        }
        else if(t == 8 && l > 18)
        {
            l = 18;
        }
    }
    for(i = 0; i < l; i++)
    {
        drawCharFromString(s[i],x,y,t);
        if(t == 16)
        {
            x += 12;
            if(x > 118)
            {
                y++;
                x = 0;
            }
        }
        else if(t == 8)
        {
            x += 6;
            if(x > 123)
            {
                y++;
                x = 0;
            }
        }
    }
}

/*
    PAGE|COL 0 |COL 1 | ...  |COL 126|COL 127|
    0   |      |      | ...  |       |       |
    1   |      |      | ...  |       |       |
    2   |      |      | ...  |       |       |
    3   |      |      | ...  |       |       |
    4   |      |      | ...  |       |       |
    5   |      |      | ...  |       |       |
    6   |      |      | ...  |       |       |
    7   |      |      | ...  |       |       |


    PAGE 0  | COL 0
    ----------------
            |   LSB
            |   
            |
            |
            |
            |
            |
            |   MSB
*/

void initData()
{
    int i;
    xSemaphoreTake(xSemaphore,portMAX_DELAY);
    for(i = 0; i < 3; i++)
    {
        patient[i].valid = 0;
    }
    xSemaphoreGive(xSemaphore);
}

void displayPatientDataTask(void *arg)
{
    int i;
    while(1)
    {   
        for(i = 0; i < 3; i++)
        {
            xSemaphoreTake(xSemaphore,portMAX_DELAY);
            if(patient[i].valid == 1)
            {
                clearDisplay();
                drawString(patient[i].name,0,0,8,0);
                drawFloat(patient[i].value,0,1,16);
                // clear box for high or low value warning
                clearBox(115,1,127,2);
                drawString(patient[i].reference,115,1,16,0);
                drawString(patient[i].units,0,3,16,0);
                drawString(patient[i].description,0,6,8,1);
                display();
                xSemaphoreGive(xSemaphore);
            }
            else
            {
                xSemaphoreGive(xSemaphore);
                break;
            }
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}
/*
    in  index
    lc  loinc code
    c   loinc code interpretation
    v   value
    u   units
    n   name
    r   reference
*/
void copyDataToPatient(int in, char* lc, char* c, float v, char* u, char* n, char* r)
{
    strcpy(patient[in].name,n);
    strcpy(patient[in].loinc,lc);
    patient[in].value = v;
    strcpy(patient[in].units,u);
    strcpy(patient[in].reference,r);
    strcpy(patient[in].description, c);
    patient[in].valid = 1;
    patientIndex++;
}

void parseJSONTask(char *js)
{
    JSON_Value *root_value;
    JSON_Object *data, *codeObj, *system, *subject, *valueQuantity, *interpCodingObj, *interpSystem;
    JSON_Array *codingArray, *interp, *interpCodingArray;
    
    char msgLoincCode[32];
    char msgLoincCodeInterp[256];
    float msgValue;
    char msgUnits[32];
    char msgName[128];
    char msgRef[8];

    root_value = json_parse_string(js);
	data = json_value_get_object(root_value);
    if(data == NULL)
        vTaskDelete(NULL);

    // loinc code
    codeObj = json_object_get_object(data,"code");
    codingArray = json_object_get_array(codeObj,"coding");
    system = json_array_get_object(codingArray,0);
    strcpy(msgLoincCode,json_object_get_string(system,"code"));
    // interpretation of loinc code
    strcpy(msgLoincCodeInterp,json_object_get_string(system,"display"));

    // measured value
    valueQuantity = json_object_get_object(data,"valueQuantity");
    msgValue = json_object_get_number(valueQuantity,"value");
    // units of measured value
    strcpy(msgUnits,json_object_get_string(valueQuantity,"code"));

    // patient name
    subject = json_object_get_object(data,"subject");
    strcpy(msgName,json_object_get_string(subject,"display"));

    // high or low value warning
    interp = json_object_get_array(data,"interpretation");
    interpCodingObj = json_array_get_object(interp,0);
    interpCodingArray = json_object_get_array(interpCodingObj,"coding");
    interpSystem = json_array_get_object(interpCodingArray,0);
    strcpy(msgRef,json_object_get_string(interpSystem,"code"));

    xSemaphoreTake(xSemaphore,portMAX_DELAY);
        // if the name in the new message is equal to
        // the name of the current data ie. new data
        // is data of the current patient
        if(strcmp(patient[0].name,msgName) == 0)
        {

            // if the patient data structure is full
            if(patientIndex >= 3)
            {
                // overwrite the first element as it is the oldest
                // fifo
                patientIndex = 0;
            }
            copyDataToPatient(patientIndex, msgLoincCode, msgLoincCodeInterp, msgValue, msgUnits, msgName, msgRef);
        }
        // if the message contains data of a
        // new patient
        else
        {
            // overwrite first element as this is a new patient
            copyDataToPatient(0, msgLoincCode, msgLoincCodeInterp, msgValue, msgUnits, msgName, msgRef);
            // invalidate the other two elements
            patient[1].valid = 0;
            patient[2].valid = 0;
        }
        
    xSemaphoreGive(xSemaphore);

    vTaskDelete(NULL);
}

// this function deals with mqtt events
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    // get the client associated with the event
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "/topic/conor0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/conor1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/conor1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            //msg_id = esp_mqtt_client_publish(client, "/topic/conor0", "data", 0, 0, 0);
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
        // data event
        /*  msg_id                  message id
            topic                   pointer to the received topic
            topic_len               length of the topic
            data                    pointer to the received data
            data_len                length of the data for this event
            current_data_offset     offset of the current data for this event
            total_data_len  total   length of the data received
        */
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            strncpy(jsonString, event->data, event->data_len);
            jsonString[event->data_len] = '\0';
            xTaskCreate(&parseJSONTask, "parse JSON task", (1024 * 8),  (void *)jsonString, 5, NULL);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    
    return ESP_OK;
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_init(void)
{
    // initialises the underlying TCP/IP stack
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    // creates system event task and initialises event callback
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // initialise wifi resources for wifi driver;
    // control structure, buffers
    // start wifi task
    // use default wifi configuration
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // set wifi api configuration storage type
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "VM0196906",
            .password = "8w2knZfjpdyb",
            //.ssid = "Conor's phone",
            //.password = "password12345",
        },
    };
    // set operating mode set to station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // set configuration of the STA
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s]", "VM0196906");
    // start wifi according to current config mode
    // if STA it creates a station control block and starts the station
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    /*  xEventGroupWaitBits(event_group, 
                            bits_to_wait_for,
                            clear_on_exit,
                            wait_for_all_bits,
                            ticks_to_wait)
        this will wait for the connected bit in the wifi event group
        to be asserted. It will not be cleared when the function returns*/
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

static void mqtt_app_start(void)
{
    // set configuration for mqtt
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://broker.mqttdashboard.com",
        // mqtt event handler
        .event_handle = mqtt_event_handler,
        // .user_context = (void *)your_context
        .buffer_size = (1024 * 4)
    };

    #if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

#endif /* CONFIG_BROKER_URL_FROM_STDIN */
    // initialise mqtt client with set config
    // returns client handle
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    // start mqtt client
    esp_mqtt_client_start(client);
}

/*void emptyTask(void *arg)
{

    while(1)
    {
        vTaskDelay( 2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}*/

void app_main() // vTaskStartScheduler is created here
{
    // create mutex, returns handle
    xSemaphore = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_WS", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(i2c_master_init());
    begin(SSD1306_SWITCHCAPVCC, 0x3C);

    clearDisplay();
    display();

    nvs_flash_init();
    wifi_init();
    mqtt_app_start();

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    xTaskCreate(&displayPatientDataTask, "display patient data task", (1024 * 4), NULL, 5, NULL);

    while(1)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}