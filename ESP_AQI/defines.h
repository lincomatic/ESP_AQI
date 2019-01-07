#ifndef _DEFINES_H_


//#define AQI0 // dual channel no OLED
#define AQI1 // single channel w/ OLED


#ifdef AQI0
#define USE_BME280
#define USE_AM2320
#define PIN_RX1 12 // D6/GPIO12 PMS7003
#define PIN_RX2 13 // D7GPIO13 PMS5003
#define PIN_TX  15 // LoLin/WeMos D8/GPIO1pin_fac5 - dummy-not hooked up
#define PIN_SET 14 // D5/GPIO14
#define EMONCMS
#define EMONCMS_NODE "aqi0"
#endif // AQI0

#ifdef AQI1
//#define USE_AM2320
#define USE_BME280
#define OLED128X64 // I2C OLED needs library https://github.com/greiman/SSD1306Ascii
#define OLED_BME280_TEMP_CORRECTION (0.0F)
#define OLED_BME280_RH_CORRECTION (0.0F)
#define OLED_AM2320_TEMP_CORRECTION (0.0F)
#define OLED_AM2320_RH_CORRECTION (0.0F)
#define PIN_RX1 12 // D6/GPIO12 PMS7003
#define PIN_TX  15 // LoLin/WeMos D8/GPIO1pin_fac5 - dummy-not hooked up
#define PIN_SET 14 // D5/GPIO14
#define EMONCMS
#define EMONCMS_NODE "aqi1"
#endif // AQI1

#ifdef MCP9808FEED
//#define USE_AM2320
#define USE_MCP9808
//#define USE_BME280
//#define OLED128X64 // I2C OLED needs library https://github.com/greiman/SSD1306Ascii
#define UPDATE_INTERVAL_MS (5UL * 1000UL)
// delay after wake up before taking a reading - to give PMS time to stabilize
#define PMS_SLEEP_WAKEUP_WAIT 0000UL
#define OLED_BME280_TEMP_CORRECTION (0.0F)
#define OLED_BME280_RH_CORRECTION (0.0F)
#define OLED_AM2320_TEMP_CORRECTION (0.0F)
#define OLED_AM2320_RH_CORRECTION (0.0F)
//#define PIN_RX1 12 // D6/GPIO12 PMS7003
#define PIN_TX  15 // LoLin/WeMos D8/GPIO1pin_fac5 - dummy-not hooked up
#define PIN_SET 14 // D5/GPIO14
#define EMONCMS
#define EMONCMS_NODE "aqi1"
#endif // MCP9808FEED

#define WIFI_MGR // use AP mode to configure via captive portal
#define OTA_UPDATE // OTA firmware update
//#define OLED128X64 // I2C OLED needs library https://github.com/greiman/SSD1306Ascii
#define OLED_I2C_ADDR 0x3C


//
// aux sensor selection
//
//#define testaux // testing aux sensors - infinite loop in setup()
//#define USE_MCP9808
//#define USE_AM2320
//#define USE_BME280
#define BME280_LOW_POWER // put BME280 to sleep between readings
#define BME280_I2C_ADDR 0x76
#define BME280_TEMP_CORRECTION (0.0F) // moved to EMONCMS(-2.6)
#define BME280_RH_CORRECTION (0.0F) // moved to EMONCMS(8.0)

//
// PMS sensor(s)
//
//#define PIN_RX1 12 // D6/GPIO12 PMS7003
//#define PIN_RX2 13 // D7GPIO13 PMS5003
//#define PIN_TX  15 // LoLin/WeMos D8/GPIO1pin_fac5 - dummy-not hooked up
//#define PIN_TX  12 // LoLin/WeMos D6/GPIO12
//#define PIN_SET 13  // LoLin/WeMos D7/GPIO13
//#define PIN_SET 14 // D5/GPIO14
//#define PIN_LED 2 //0
//#define PIN_FACTORY_RESET 12 // LoLin D6/GPIO12 ground this pin to wipe out EEPROM & WiFi settings

#define AP_PREFIX "ESPAQI_"
#define TEMPERATURE_FAHRENHEIT
//#define THINGSPEAK
//#define EMONCMS

#ifndef UPDATE_INTERVAL_MS
#define UPDATE_INTERVAL_MS (5UL * 60UL * 1000UL)
#endif //UPDATE_INTERVAL_MS


// delay after wake up before taking a reading - to give PMS time to stabilize
#ifndef PMS_SLEEP_WAKEUP_WAIT
#define PMS_SLEEP_WAKEUP_WAIT 32000UL
#endif // PMS_SLEEP_WAKEUP_WAIT

#ifdef THINGSPEAK
#define API_WRITEKEY_LEN 16
//#define THINGSPEAK_WRITE_KEY "thingspeak-write-key"
#endif

#ifdef EMONCMS
//#define EMONCMS_NODE "aqi1"
//#define EMONCMS_BASE_URI "http://data.openevse.com/emoncms/input/post?node="
//#define EMONCMS_WRITE_KEY "---emoncms-write-key------------"
#define API_WRITEKEY_LEN 32
#endif

// OTA_UPDATE
#define OTA_HOST "ESPAQI"
#define OTA_PASS "espaqi"

#define TEMPERATURE_NOT_INSTALLED -2560 // fake temp to return when hardware not installed

#endif // _DEFINES_H_
