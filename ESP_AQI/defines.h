#ifndef _DEFINES_H_

#define WIFI_MGR // use AP mode to configure via captive portal
#define OTA_UPDATE // OTA firmware update

//
// aux sensor selection
//
//#define testaux // testing aux sensors - infinite loop in setup()
//#define USE_MCP9808
#define USE_AM2320
#define USE_BME280
#define BME280_LOW_POWER // put BME280 to sleep between readings
#define BME280_I2C_ADDR 0x76
#define BME280_TEMP_CORRECTION (0.0) // moved to EMONCMS(-2.6)
#define BME280_RH_CORRECTION (0.0) // moved to EMONCMS(8.0)

//
// PMS sensor(s)
//
#define PIN_RX1 12 // D6/GPIO12 PMS7003
#define PIN_RX2 13 // D7GPIO13 PMS5003
#define PIN_TX  15 // LoLin/WeMos D8/GPIO1pin_fac5 - dummy-not hooked up
//#define PIN_TX  12 // LoLin/WeMos D6/GPIO12
//#define PIN_SET 13  // LoLin/WeMos D7/GPIO13
#define PIN_SET 14 // D5/GPIO14
//#define PIN_LED 2 //0
//#define PIN_FACTORY_RESET 12 // LoLin D6/GPIO12 ground this pin to wipe out EEPROM & WiFi settings

#define AP_PREFIX "ESPAQI_"
#define TEMPERATURE_FAHRENHEIT
//#define THINGSPEAK
#define EMONCMS

#define UPDATE_INTERVAL_MS (5UL * 60UL * 1000UL)
// delay after wake up before taking a reading - to give PMS time to stabilize
#define PMS_SLEEP_WAKEUP_WAIT 32000UL

#ifdef THINGSPEAK
#define API_WRITEKEY_LEN 16
#define THINGSPEAK_WRITE_KEY "thingspeak-write-key"
#endif

#ifdef EMONCMS
#define EMONCMS_NODE "aqi0"
#define EMONCMS_BASE_URI "http://data.openevse.com/emoncms/input/post?node="
#define EMONCMS_WRITE_KEY "---emoncms-write-key------------"
#define API_WRITEKEY_LEN 32
#endif

// OTA_UPDATE
#define OTA_HOST "ESPAQI"
#define OTA_PASS "espaqi"

#define TEMPERATURE_NOT_INSTALLED -2560 // fake temp to return when hardware not installed

#endif // _DEFINES_H_
