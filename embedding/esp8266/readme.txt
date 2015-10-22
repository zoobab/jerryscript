About

Files in this folder (embedding/esp8266) are copied from
examples/project_template of esp_iot_rtos_sdk and modified for JerryScript.
You can view online from
https://github.com/espressif/esp_iot_rtos_sdk/tree/master/examples/project_template
page.


How to build JerryScript for ESP8266

1. SDK

Follow
https://github.com/seanshpark/esp8266-docs/wiki/Preparation-and-setting-build-environment
page to setup build environment


2. Building

Follow
https://github.com/seanshpark/esp8266-docs/wiki/Building-JerryScript
page to patch for JerryScript building.
Below is a summary after SDK patch is applied.

2-1. JerryScript

-------------------------------------------------------------------------------
cd ~/harmony/jerryscript
make -f Makefile.esp8266 clean
make -f Makefile.esp8266
-------------------------------------------------------------------------------

2-2. ES9266 user application

If only build for files changed only in js/ and /user then you can do as below.

-------------------------------------------------------------------------------
cd ~/harmony/jerryscript/embedding/esp8266
./js2c.py; BOOT=new APP=0 SPI_SPEED=80 SPI_MODE=QIO SPI_SIZE_MAP=1 make
-------------------------------------------------------------------------------

./jsc2c.py:
  Must run oncee at the first time to generate the header file.
  Converts all JS files in js for embedding to esp8266_js.h file.

BOOT=new APP=0 SPI_SPEED=80 SPI_MODE=QIO SPI_SIZE_MAP=1 make:
  Build the user binary.


2-3. Output files

Output files should be placed at $BIN_PATH



3. Flashing for ESP8266 ESP-01 board (WiFi Module)

Steps are for ESP8266 ESP-01, WiFi, board. Others may vary.
Refer http://www.esp8266.com/wiki/doku.php?id=esp8266-module-family page.

3.1 GPIO0 and GPIO2

Before flashing you need to follow the steps.

1) Power off ESP8266
2) Connect GPIO0 to GND and GPIO2 to VCC
3) Power on ESP8266
4) Flash

3.2 Flash
-------------------------------------------------------------------------------
cd $BIN_PATH
/opt/Espressif/esptool-py/esptool.py --port /dev/ttyUSB0 \
write_flash \
0x00000 eagle.flash.bin 0x40000 eagle.irom0text.bin
-------------------------------------------------------------------------------

/dev/ttyUSB0:
  You may need to change the device name for your Serial port


4. Run

1) Power off
2) Disonnect(float) both GPIO0 and GPIO2
3) Power on

Sample program here works with LED and a SW with below connection.
 - Connect GPIO2 to a LED > 4K resistor > GND
 - Connect GPIO0 between VCC > 4K resistor and GND
If GPIO0 is H then turn on time of the LED is longer. If L will be shorter.

