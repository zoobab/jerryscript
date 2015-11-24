### About

Files in this folder (embedding/esp8266) are copied from
examples/project_template of esp_iot_rtos_sdk and modified for JerryScript.
You can view online from [this](https://github.com/espressif/esp_iot_rtos_sdk/tree/master/examples/project_template) page.


### How to build JerryScript for ESP8266

#### 1. SDK

Follow [this](https://github.com/seanshpark/esp8266-docs/wiki/Preparation-and-setting-build-environment)
page to setup build environment


#### 2. Building

Follow [this](https://github.com/seanshpark/esp8266-docs/wiki/Building-JerryScript)
page to patch for JerryScript building.
Below is a summary after SDK patch is applied.


```
cd ~/harmony/jerryscript
make -f ./embedding/esp8266/Makefile.esp8266 clean
make -f ./embedding/esp8266/Makefile.esp8266
```

Output files should be placed at $BIN_PATH

#### 3. Flashing for ESP8266 ESP-01 board (WiFi Module)

Steps are for ESP8266 ESP-01(WiFi) board. Others may vary.
Refer http://www.esp8266.com/wiki/doku.php?id=esp8266-module-family page.

##### 3.1 GPIO0 and GPIO2

Before flashing you need to follow the steps.

1. Power off ESP8266
2. Connect GPIO0 to GND and GPIO2 to VCC
3. Power on ESP8266
4. Flash

##### 3.2 Flashing

```
make -f ./embedding/esp8266/Makefile.esp8266 flash
```

Default USB device is `/dev/ttyUSB0`. If you have different one, give with `USBDEVICE`, like;

```
USBDEVICE=/dev/ttyUSB1 make -f ./embedding/esp8266/Makefile.esp8266 flash
```


### Running

1. Power off
2. Disonnect(float) both GPIO0 and GPIO2
3. Power on

Sample program here works with LED and a SW with below connection.
* Connect GPIO2 to a LED > 4K resistor > GND
* Connect GPIO0 between VCC > 4K resistor and GND

If GPIO0 is H then turn on time of the LED is long. If L, it will be short.
