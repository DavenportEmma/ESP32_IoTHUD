# ESP32_IoTHUD

## This repo is just for projects, to set up the toolchain and IDF follow this link: https://docs.espressif.com/projects/esp-idf/en/latest/get-started/

## If flashing to the ESP32 fails
It is probably a driver issue. Follow these steps:
1. Download the Linux-3-x-x... folder from this repo
2. Navigate to it in terminal
3. >make
4. >sudo cp cp210x.ko /lib/modules/"$(uname -r)"/kernel/drivers/usb/serial/
5. >sudo modprobe usbserial
6. >sudo modprobe cp210x
If step 6 fails, saying something about a required key, you need to disable secure boot using the mokutil program
7. >sudo apt-get install mokutil
8. >sudo mokutil --disable-validation
REBOOT
9. >sudo modprobe cp210x
It should work now

