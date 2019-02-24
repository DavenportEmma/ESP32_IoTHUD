#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"

static const char *TAG = "i2c-example";

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
    // free i2c command link
    i2c_cmd_link_delete(cmd);
    return ret;
}

void clearDisplay()
{
    memset(buffer, 0, WIDTH * ((HEIGHT + 7) / 8));
}

void begin(uint8_t vcs, uint8_t addr) 
{
	//if((!buffer) && !(buffer = (uint8_t *)malloc(WIDTH * ((HEIGHT + 7) / 8))))
    	//return false;

    buffer = (uint8_t*) malloc(WIDTH * ((HEIGHT + 7) / 8));

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

void VLine(int16_t x, int16_t y, int16_t h, uint16_t colour)
{
    
}

/*void aVLine(int16_t x, int16_t __y, int16_t __h, uint16_t colour) 
{
    if((x >= 0) && (x < WIDTH)) 
    { // X coord in bounds?
        if(__y < 0) 
        { // Clip top
            __h += __y;
            __y = 0;
        }   
        if((__y + __h) > HEIGHT) 
        { // Clip bottom
            __h = (HEIGHT - __y);
        }
        if(__h > 0) 
        {   // Proceed only if height is now positive
            // this display doesn't need ints for coordinates,
            // use local byte registers for faster juggling
            uint8_t  y = __y, h = __h;
            uint8_t *pBuf = &buffer[(y / 8) * WIDTH + x];

            // do the first partial byte, if necessary - this requires some masking
            uint8_t mod = (y & 7);
            if(mod) 
            {
                // mask off the high n bits we want to set
                mod = 8 - mod;
                // note - lookup table results in a nearly 10% performance
                // improvement in fill* functions
                // uint8_t mask = ~(0xFF >> mod);
                static const uint8_t PROGMEM premask[8] =
                { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
                uint8_t mask = pgm_read_byte(&premask[mod]);
                // adjust the mask if we're not going to reach the end of this byte
                if(h < mod) mask &= (0XFF >> (mod - h));

                switch(colour) 
                {
                    case WHITE:   *pBuf |=  mask; break;
                    case BLACK:   *pBuf &= ~mask; break;
                    case INVERSE: *pBuf ^=  mask; break;
                }
                pBuf += WIDTH;
            }

            if(h >= mod)   
            {   // More to go?
                h -= mod;
                // Write solid bytes while we can - effectively 8 rows at a time
                if(h >= 8) 
                {
                    if(colour == INVERSE) 
                    {
                        // separate copy of the code so we don't impact performance of
                        // black/white write version with an extra comparison per loop
                        do 
                        {
                            *pBuf ^= 0xFF;  // Invert byte
                            pBuf  += WIDTH; // Advance pointer 8 rows
                            h     -= 8;     // Subtract 8 rows from height
                        } 
                        while(h >= 8);
                    } 
                    else 
                    {
                        // store a local value to work with
                        uint8_t val = (colour != BLACK) ? 255 : 0;
                        do 
                        {
                            *pBuf = val;    // Set byte
                            pBuf += WIDTH;  // Advance pointer 8 rows
                            h    -= 8;      // Subtract 8 rows from height
                        } 
                        while(h >= 8);
                    }
                }

                if(h) 
                {   // Do the final partial byte, if necessary
                    mod = h & 7;
                    // this time we want to mask the low bits of the byte,
                    // vs the high bits we did above
                    // uint8_t mask = (1 << mod) - 1;
                    // note - lookup table results in a nearly 10% performance
                    // improvement in fill* functions
                    static const uint8_t PROGMEM postmask[8] =
                    { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };
                    uint8_t mask = pgm_read_byte(&postmask[mod]);
                    switch(colour) 
                    {
                        case WHITE:   *pBuf |=  mask; break;
                        case BLACK:   *pBuf &= ~mask; break;
                        case INVERSE: *pBuf ^=  mask; break;
                    }
                }
            }
        } // endif positive height
    } // endif x in bounds
}*/

void display() 
{
	command(SSD1306_PAGEADDR);
	command(0);
	command(0xFF);
	command(SSD1306_COLUMNADDR);
	command(0);
	command(WIDTH - 1);


	uint16_t count = WIDTH * ((HEIGHT + 7) / 8);
	uint8_t *ptr   = buffer;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SSD1306_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN);
    uint8_t bytesOut = 1;
    while(count--)
    {
    	if(bytesOut >= WIRE_MAX)
    	{
    		i2c_master_stop(cmd);
    		esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    		i2c_cmd_link_delete(cmd);

    		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    		i2c_master_start(cmd);
    		i2c_master_write_byte(cmd, (SSD1306_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    		i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN);
    		uint8_t bytesOut = 1;
    	}
    	i2c_master_write_byte(cmd, *ptr++, ACK_CHECK_EN);
    	bytesOut++;
    }
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
}

static void i2c_test_task(void *arg)
{
    while(1)
    {
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    ESP_ERROR_CHECK(i2c_master_init());
    begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display();
    clearDisplay();
    drawPixel(10,60,WHITE);
    display();
    xTaskCreate(i2c_test_task, "i2c_test_task_0", 1024 * 2, (void *)0, 10, NULL);
}