// -*- C++ -*-
// compile with WeMos D1 R1 profile
//
#include <eagle_soc.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "./pms.h"

//
// aux sensor selection
//
//#define USE_AM2320
#define USE_BME280
#define BME280_LOW_POWER // put BME280 to sleep between readings
#define BME280_I2C_ADDR 0x76

#define WIFI_MGR // use AP mode to configure via captive portal
#define OTA_UPDATE // OTA firmware update
// ON LOLIN DON'T USE GPIO5/4 -> I2C!!!
//#define PIN_RX 5// LoLin/WeMos D1/GPIO5
//#define PIN_TX 4 // LoLin/WeMos D2/GPIO4
//#define PIN_RX  14 // LoLin/WeMos D5/GPIO14
#define PIN_RX1 13 // D7GPIO13 PMS5003
#define PIN_RX2 12 // D5/GPIO12 PMS7003
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

#define UPDATE_INTERVAL_MS 300000UL
// delay after wake up before taking a reading - to give PMS time to stabilize
#define PMS_SLEEP_WAKEUP_WAIT 32000UL

#ifdef THINGSPEAK
#define API_WRITEKEY_LEN 16
#define THINGSPEAK_WRITE_KEY "thingspeak-write-key"
#endif

#ifdef EMONCMS
#define EMONCMS_NODE "aqi0"
#define EMONCMS_WRITE_KEY "emoncms-write-key"
#define EMONCMS_BASE_URI "http://data.openevse.com/emoncms/input/post.json?node="
#define API_WRITEKEY_LEN 32
#endif

// OTA_UPDATE
#define OTA_HOST "ESPAQI"
#define OTA_PASS "espaqi"

#ifdef OTA_UPDATE
#include "./ArduinoOTAMgr.h"
ArduinoOTAMgr AOTAMgr;
#endif

Pmsx003 pms1(PIN_RX1, PIN_TX);
#ifdef PIN_RX2
Pmsx003 pms2(PIN_RX2, PIN_TX);
#endif // PIN_RX2

//
// aux sensor
//
#ifdef USE_AM2320
#include "./AM2320.h"
AM2320 am2320;
#endif //USE_AM2320

#ifdef USE_BME280
#include "./SparkFunBME280.h"
BME280 bme280;
bool bme280Present;
#endif // USE_BME280

typedef struct auxdata {
  float temperature;
  float humidity;
#ifdef USE_BME280
  float airPressure;
#endif // USE_BME280
} AUX_DATA;


#ifdef API_WRITEKEY_LEN
#ifdef THINGSPEAK
const char *g_ApiWriteKey = THINGSPEAK_WRITE_KEY;
#elif defined(EMONCMS)
const char *g_ApiWriteKey = EMONCMS_WRITE_KEY;
#endif // THINGSPEK
#endif // API_WRITEKEY_LEN


void reboot()
{
  Serial.println("rebooting...");
  WiFi.disconnect();
  delay(1000);
  ESP.reset();
}


#ifdef WIFI_MGR
#include "./wifimgr.h"
#endif // WIFI_MGR
#include "./btn.h"


#ifdef PIN_FACTORY_RESET
Btn btnReset(PIN_FACTORY_RESET);
#endif

////////////////////////////////////////


void factoryReset()
{
  Serial.println("Factory Reset");
#ifdef WIFI_MGR
  g_wfCfg.resetCfg(1);
#endif // WIFI_MGR
  reboot();
}


// n.b. sleep command is flaky, so use SET pin instead for sleep/wake
void pmsx003sleep()
{
#ifdef PIN_SET
  digitalWrite(PIN_SET,LOW);
#endif
}

void pmsx003wake()
{
#ifdef PIN_SET
  digitalWrite(PIN_SET,HIGH);
#endif
}


void backgroundTasks()
{
#ifdef PIN_FACTORY_RESET
  btnReset.read();
  if (btnReset.longPress()) {
    Serial.println("long press");
    factoryReset();
  }
#endif

#ifdef OTA_UPDATE
  AOTAMgr.handle();
#endif

  g_wfCfg.handleWebServer();

  yield();
}

void mydelay(unsigned long ms)
{
  unsigned long startms = millis();
  while ((millis() - startms) < ms) {
    backgroundTasks();
  }
}

void setup(void)
{
  pms1.begin();
  pms1.waitForData(Pmsx003::wakeupTime);
  pms1.write(Pmsx003::cmdModeActive);
#ifdef PIN_RX2
  pms2.begin();
  pms2.waitForData(Pmsx003::wakeupTime);
  pms2.write(Pmsx003::cmdModeActive);
#endif // PIN_RX2
#ifdef PIN_SET
  pinMode(PIN_SET, OUTPUT); // sleep/wake pin
 #ifndef PMS_SLEEP_WAKEUP_WAIT
  pmsx003wake();
 #endif
#endif
#ifdef PMS_SLEEP_WAKEUP_WAIT
  pmsx003sleep();
#endif

  // onboard leds also outputs
#ifdef PIN_LED
  pinMode(PIN_LED, OUTPUT); // onboard LED
  digitalWrite(PIN_LED,HIGH); // turn off onboard LED
#endif // PIN_LED

  Serial.begin(115200);
  while (!Serial) {};

#ifdef PIN_FACTORY_RESET
  btnReset.init();
#endif
  
#ifdef WIFI_MGR
  EEPROM.begin(g_wfCfg.getConfigSize());  

  // display the MAC on Serial
  g_wfCfg.printMac();
  
  g_wfCfg.readCfg();
  Serial.println("readCfg() done.");
  
  g_wfCfg.StartManager();
  Serial.println("StartManager() done.");
  EEPROM.end();
#endif // WIFI_MGR
  
#ifdef OTA_UPDATE
  AOTAMgr.boot(OTA_HOST,OTA_PASS);
#endif
  

#ifdef USE_AM2320 
  am2320.begin();
#endif //USE_AM2320

#ifdef USE_BME280
  Wire.begin();
#ifdef BME280_LOW_POWER
  Wire.setClock(400000); //Increase to fast I2C speed!
#endif 

  bme280.setI2CAddress(BME280_I2C_ADDR);
  if (!bme280.beginI2C()) {
    Serial.println("BME280 connect failed");
    bme280Present = false;
  }
  else {
    bme280Present = true;
#ifdef BME280_LOW_POWER
    bme280.setMode(MODE_SLEEP); //Sleep for now
#endif
  }

#endif // USE_BME280

  Serial.println("setup done");
}

////////////////////////////////////////

//
// global variables
//
AUX_DATA g_auxData;
unsigned long lastUpdateMs = 0UL;
unsigned long updateWaitMs = 0UL;
char g_sTmp[512];


const auto n = Pmsx003::Reserved;
Pmsx003::pmsData data[n];
#ifdef PIN_RX2
Pmsx003::pmsData data2[n];
#endif // PIN_RX2

int readPms(Pmsx003 *pms,Pmsx003::pmsData *data)
{
  Serial.println("readPms");
  pms->flushInput();

  int retry = 0;
  Pmsx003::PmsStatus status;
  do {
    Serial.println("_________________");
    status = pms->read(data, n);
    switch(status) {
    case Pmsx003::OK:
      {
	for (size_t i = 0; i < n; ++i) { 
	  Serial.print(data[i]);
	  Serial.print("\t");
	  Serial.print(Pmsx003::dataNames[i]);
	  Serial.print(" [");
	  Serial.print(Pmsx003::metrics[i]);
	  Serial.print("]");
	  Serial.println();
	}
      }
      break;
    default:
      backgroundTasks();
      ++retry;
      Serial.println(Pmsx003::errorMsg[status]);
      /*if (status == Pmsx003::noData)*/ mydelay(500);
    } // switch(status)

  } while ((status != Pmsx003::OK) && (retry < 10));    

  return (status == Pmsx003::OK) ? 0 : 1;
}

void loop(void)
{
  backgroundTasks();
  unsigned long startms = millis();
  Serial.println("WAKEUP");

#ifdef PIN_LED
  digitalWrite(PIN_LED,LOW); // onboard LED on
#endif // PIN_LED

#ifdef PMS_SLEEP_WAKEUP_WAIT
  pmsx003wake();
  // wait for PMS to stabilize
  mydelay(PMS_SLEEP_WAKEUP_WAIT);
#endif // PMS_SLEEP_WAKEUP_WAIT

  Serial.println("READ");
  backgroundTasks();
  int rc1 = readPms(&pms1,data);
  backgroundTasks();
  int rc2 = readPms(&pms2,data2);
  backgroundTasks();

#ifdef PMS_SLEEP_WAKEUP_WAIT
  pmsx003sleep();
#endif // PMS_SLEEP_WAKEUP_WAIT


#ifdef USE_AM2320	  
  Serial.print("---\nam2320: ");
  bool arc = am2320.measure();
  if (arc == true) {
    g_auxData.temperature =  am2320.getTemperature();
#ifdef TEMPERATURE_FAHRENHEIT
    g_auxData.temperature = (g_auxData.temperature * (9.0F/5.0F)) + 32.0F;
#endif // TEMPERATURE_FAHRENHEIT
    g_auxData.humidity = am2320.getHumidity();
    sprintf(g_sTmp,"temp=%0.0f humidity=%0.0f",g_auxData.temperature,g_auxData.humidity);
  }
  else { // error
    //	    temperature = -99;
    //	    humidity = -1;
    sprintf(g_sTmp,"error %d",am2320.getErrorCode());
  }
  Serial.println(g_sTmp);
#endif // USE_AM2320

#ifdef USE_BME280
  if (bme280Present) {
#ifdef BME280_LOW_POWER
  bme280.setMode(MODE_FORCED); //Wake up sensor and take reading
  //  long startTime = millis();
  while(bme280.isMeasuring() == false) ; //Wait for sensor to start measurment
  while(bme280.isMeasuring() == true) ; //Hang out while sensor completes the reading    
  //  long endTime = millis();
  //  Serial.print(" Measure time(ms): ");
  //  Serial.print(endTime - startTime);
#endif //BME280_LOW_POWER

    // N.B. *MUST* read temperature before pressure to load t_fine variable
#ifdef TEMPERATURE_FAHRENHEIT
    g_auxData.temperature = bme280.readTempF();
#else
    g_auxData.temperature = bme280.readTempC();
#endif //TEMPERATURE_FAHRENHEIT
    g_auxData.humidity = bme280.readFloatHumidity();
    g_auxData.airPressure = bme280.readFloatPressure();
  }
#endif // USE_BME280
    
#ifdef API_WRITEKEY_LEN
  if (*g_ApiWriteKey) {
    *g_sTmp = 0;
      
#ifdef THINGSPEAK
    sprintf(g_sTmp,"http://api.thingspeak.com/update?api_key=%s&field1=%d&field2=%d&field3=%d",g_ApiWriteKey,data[Pmsx003::PM1dot0],data[Pmsx003::PM2dot5],data[Pmsx003::PM10dot0]);
    if (arc == true) {
      sprintf(g_sTmp+strlen(g_sTmp),"&field4=%0.0f&field5=%0.0f",g_auxData.temperature,g_auxData.humidity);
    }
#elif defined(EMONCMS)
    const char *baseuri = EMONCMS_BASE_URI;
    const char *node = EMONCMS_NODE;
    sprintf(g_sTmp,"%s%s&json={",baseuri,node);
    if (!rc1) {
      sprintf(g_sTmp+strlen(g_sTmp),"pm1:%d,pm25:%d,pm10:%d,pm1cf1:%d,pm25cf1:%d,pm10cf1:%d,ppd03:%d,ppd05:%d,ppd1:%d,ppd25:%d,ppd50:%d,ppd10:%d",data[Pmsx003::PM1dot0],data[Pmsx003::PM2dot5],data[Pmsx003::PM10dot0],data[Pmsx003::PM1dot0CF1],data[Pmsx003::PM2dot5CF1],data[Pmsx003::PM10dot0CF1],data[Pmsx003::Particles0dot3],data[Pmsx003::Particles0dot5],data[Pmsx003::Particles1dot0],data[Pmsx003::Particles2dot5],data[Pmsx003::Particles5dot0],data[Pmsx003::Particles10]);
    }

    if (!rc2) {
      if (!rc1) strcat(g_sTmp,",");
      sprintf(g_sTmp+strlen(g_sTmp),"pm1_2:%d,pm25_2:%d,pm10_2:%d,pm1cf1_2:%d,pm25cf1_1:%d,pm10cf1_2:%d,ppd03_2:%d,ppd05_2:%d,ppd1_2:%d,ppd25_2:%d,ppd50_2:%d,ppd10_2:%d",data2[Pmsx003::PM1dot0],data2[Pmsx003::PM2dot5],data2[Pmsx003::PM10dot0],data2[Pmsx003::PM1dot0CF1],data2[Pmsx003::PM2dot5CF1],data2[Pmsx003::PM10dot0CF1],data2[Pmsx003::Particles0dot3],data2[Pmsx003::Particles0dot5],data2[Pmsx003::Particles1dot0],data2[Pmsx003::Particles2dot5],data2[Pmsx003::Particles5dot0],data2[Pmsx003::Particles10]);
    }

#ifdef USE_AM2320
    if (arc == true) {
      if (!rc1 || !rc2) strcat(g_sTmp,",");
      sprintf(g_sTmp+strlen(g_sTmp),"temp:%0.0f,rh:%0.0f",g_auxData.temperature,g_auxData.humidity);
    }
#endif // USE_AM2320

#ifdef USE_BME280
    bool arc;
    if (bme280Present) {
      arc = true;
      if (!rc1 || !rc2) strcat(g_sTmp,",");
      sprintf(g_sTmp+strlen(g_sTmp),"temp:%0.0f,rh:%0.0f,airprs:%0.0f",g_auxData.temperature,g_auxData.humidity,g_auxData.airPressure);
    }
    else {
      arc = false;
    }
#endif //USE_BME280

#if defined(USE_BME280) || defined(USE_AM2320)
	if (!rc1 || !rc2 || arc) strcat(g_sTmp, ",");
#else
	if (!rc1 || !rc2) strcat(g_sTmp, ",");
#endif
	sprintf(g_sTmp+strlen(g_sTmp),"rssi:%d}&apikey=%s",WiFi.RSSI(),EMONCMS_WRITE_KEY);

#endif // EMONCMS
    if (*g_sTmp) {
      Serial.println(g_sTmp);
      for (int i=0;i < 2;i++) {
	backgroundTasks();
	HTTPClient http;
	http.setUserAgent("ESP_AQI/1.0");
	http.begin(g_sTmp);
	//	http.begin("http://www.google.com/gg");
	int hrc = http.GET(); // send request
	Serial.print("return code: ");Serial.println(hrc);
	String hresp = http.getString(); // get payload
	Serial.print("response data: ");Serial.println(hresp);
	http.end();
	mydelay(250);
	if (hrc > 0) break;
	if (i==4) while (1) backgroundTasks();
      }
    }
  }
  else {
    Serial.println("API write key not set");
  }
#endif // API_WRITEKEY_LEN

#ifdef PIN_LED
  digitalWrite(PIN_LED,HIGH); // onboard LED off
#endif // PIN_LED

  unsigned long curms = millis();
  unsigned long waitms = curms-startms;
  Serial.print("waitms: ");Serial.println(waitms);
  Serial.print("last update interval: ");Serial.println(curms - lastUpdateMs);
  lastUpdateMs = curms;
  updateWaitMs = UPDATE_INTERVAL_MS - waitms;
  Serial.print("waiting updateWaitMs: ");Serial.println(updateWaitMs);
  mydelay(updateWaitMs);
}
