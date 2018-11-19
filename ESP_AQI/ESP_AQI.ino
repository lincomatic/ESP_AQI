// -*- C++ -*-
// compile with WeMos D1 R1 profile
//
#include <eagle_soc.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "./pms.h"
#include "./AM2320.h"

#define WIFI_MGR // use AP mode to configure via captive portal
#define OTA_UPDATE // OTA firmware update
// ON LOLIN DON'T USE GPIO5/4 -> I2C!!!
//#define PIN_RX 5// LoLin/WeMos D1/GPIO5
//#define PIN_TX 4 // LoLin/WeMos D2/GPIO4
#define PIN_RX  14 // LoLin/WeMos D5/GPIO14
#define PIN_TX  15 // LoLin/WeMos D8/GPIO15 - dummy-not hooked up
//#define PIN_TX  12 // LoLin/WeMos D6/GPIO12
#define PIN_SET 13  // LoLin/WeMos D7/GPIO13
//#define PIN_LED 2 //0
#define PIN_FACTORY_RESET 12 // LoLin D6/GPIO12 ground this pin to wipe out EEPROM & WiFi settings

#define AP_PREFIX "ESPAQI_"
#define TEMPERATURE_FAHRENHEIT
//#define THINGSPEAK
#define EMONCMS






#define UPDATE_INTERVAL_MS 300000UL
// delay after wake up before taking a reading
#define PMS_SLEEP_WAKEUP_WAIT 30000UL

#ifdef THINGSPEAK
#define API_WRITEKEY_LEN 16
//#define THINGSPEAK_WRITE_KEY "thingspeak-write-key"
#endif

#ifdef EMONCMS
#define EMONCMS_NODE "aqi0"
//#define EMONCMS_WRITE_KEY "emoncms-write-key"
//#define EMONCMS_BASE_URI "http://data.openevse.com/emoncms/input/post.json?node="
#define EMONCMS
#define EMONCMS_BASE_URI "http://www.lincomatic.com/emoncms/input/post.json?node="
#define API_WRITEKEY_LEN 32
#endif

// OTA_UPDATE
#define OTA_HOST "ESPAQI"
#define OTA_PASS "espaqi"

#ifdef OTA_UPDATE
#include "./ArduinoOTAMgr.h"
ArduinoOTAMgr AOTAMgr;
#endif


Pmsx003 pms(PIN_RX, PIN_TX);
AM2320 am2320;
char g_ApiWriteKey[API_WRITEKEY_LEN+1] = { 0 };

void reboot()
{
  WiFi.disconnect();
  delay(1000);
  ESP.reset();
}


#ifdef WIFI_MGR
#include "./wifimgr.h"
#endif // WIFI_MGR
#include "./btn.h"


Btn btnReset(PIN_FACTORY_RESET);

////////////////////////////////////////


void factoryReset()
{
  Serial.println("Factory Reset");
#ifdef WIFI_MGR
  wfCfg.resetCfg(1);
#endif // WIFI_MGR
  reboot();
}


// n.b. sleep command is flaky, so use SET pin instead for sleep/wake
void pmsx003sleep()
{
  digitalWrite(PIN_SET,LOW);
}

void pmsx003wake()
{
  digitalWrite(PIN_SET,HIGH);
}


void backgroundTasks()
{
  btnReset.read();
  if (btnReset.longPress()) {
    Serial.println("long press");
    factoryReset();
  }

#ifdef OTA_UPDATE
  AOTAMgr.handle();
#endif

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
  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModeActive);
  pinMode(PIN_SET, OUTPUT); // sleep/wake pin
  pmsx003sleep();

  // onboard leds also outputs
#ifdef PIN_LED
  pinMode(PIN_LED, OUTPUT); // onboard LED
  digitalWrite(PIN_LED,HIGH); // turn off onboard LED
#endif // PIN_LED

  Serial.begin(115200);
  while (!Serial) {};

  btnReset.init();
  
#ifdef WIFI_MGR
  EEPROM.begin(wfCfg.getConfigSize());  

  // display the MAC on Serial
  wfCfg.printMac();
  
  wfCfg.readCfg();
  Serial.println("readCfg() done.");
  
  wfCfg.StartManager();
  Serial.println("StartManager() done.");
  EEPROM.end();
#endif // WIFI_MGR
  
#ifdef OTA_UPDATE
  AOTAMgr.boot(OTA_HOST,OTA_PASS);
#endif
  
 
  am2320.begin();

#ifdef THINGSPEAK_WRITE_KEY
  strcpy(g_ApiWriteKey,THINGSPEAK_WRITE_KEY);
#elif defined(EMONCMS_WRITE_KEY)
  strcpy(g_ApiWriteKey,EMONCMS_WRITE_KEY);
#endif

  Serial.println("setup done");
}

////////////////////////////////////////

unsigned long lastUpdateMs = 0UL;
unsigned long updateWaitMs = 0UL;
char g_sTmp[256];

void loop(void)
{
  if (1)   backgroundTasks();

  unsigned long curms = millis();
  if ((curms-lastUpdateMs) >= updateWaitMs) {
#ifdef PIN_LED
    digitalWrite(PIN_LED,LOW); // onboard LED on
#endif // PIN_LED
    pmsx003wake();
    unsigned long waitms = millis();
    Serial.println("WAKEUP");
    mydelay(PMS_SLEEP_WAKEUP_WAIT);
    pms.flushInput();
    Serial.println("READ");
    const auto n = Pmsx003::Reserved;
    Pmsx003::pmsData data[n];

    Pmsx003::PmsStatus status = pms.read(data, n);
    do {
      status = pms.read(data, n);
  
      switch (status) {
      case Pmsx003::OK:
	{
	  Serial.println("_________________");
	  Serial.print("Wait time ");
	  waitms = millis() - waitms;
	  Serial.println(waitms);
	  
	  // For loop starts from 3
	  // Skip the first three data (PM1dot0CF1, PM2dot5CF1, PM10CF1)
	  for (size_t i = 0; i < n; ++i) { 
	    Serial.print(data[i]);
	    Serial.print("\t");
	    Serial.print(Pmsx003::dataNames[i]);
	    Serial.print(" [");
	    Serial.print(Pmsx003::metrics[i]);
	    Serial.print("]");
	    Serial.println();
	  }
	  
	  Serial.print("---\nam2320: ");
	  bool arc = am2320.measure();
	  float humidity,temperature;
	  if (arc == true) {
	    temperature =  am2320.getTemperature();
#ifdef TEMPERATURE_FAHRENHEIT
	    temperature = temperature * (9.0/5.0) + 32.0;
#endif // TEMPERATURE_FAHRENHEIT
	    humidity = am2320.getHumidity();
	    sprintf(g_sTmp,"temp=%0.0f humidity=%0.0f",temperature,humidity);
	  }
	  else { // error
	    //	    temperature = -99;
	    //	    humidity = -1;
	    sprintf(g_sTmp,"error %d",am2320.getErrorCode());
	  }
	  Serial.println(g_sTmp);
	  
	  if (*g_ApiWriteKey) {
	    *g_sTmp = 0;

#ifdef THINGSPEAK
	    sprintf(g_sTmp,"http://api.thingspeak.com/update?api_key=%s&field1=%d&field2=%d&field3=%d",g_ApiWriteKey,data[Pmsx003::PM1dot0],data[Pmsx003::PM2dot5],data[Pmsx003::PM10dot0]);
	    if (1) { //(arc == true) {
	      sprintf(g_sTmp+strlen(g_sTmp),"&field4=%0.0f&field5=%0.0f",temperature,humidity);
	    }
#elif defined(EMONCMS)
	    const char *baseuri = EMONCMS_BASE_URI;
	    const char *node = EMONCMS_NODE;
	    sprintf(g_sTmp,"%s%s&apikey=%s&json={pm1:%d,pm25:%d,pm10:%d,pm1cf1:%d,pm25cf1:%d,pm10cf1:%d,ppd03:%d,ppd05:%d,ppd1:%d,ppd25:%d,ppd50:%d,ppd10:%d",baseuri,node,EMONCMS_WRITE_KEY,data[Pmsx003::PM1dot0],data[Pmsx003::PM2dot5],data[Pmsx003::PM10dot0],data[Pmsx003::PM1dot0CF1],data[Pmsx003::PM2dot5CF1],data[Pmsx003::PM10dot0CF1],data[Pmsx003::Particles0dot3],data[Pmsx003::Particles0dot5],data[Pmsx003::Particles1dot0],data[Pmsx003::Particles2dot5],data[Pmsx003::Particles5dot0],data[Pmsx003::Particles10]);
	    if (arc == true) {
	      sprintf(g_sTmp+strlen(g_sTmp),",temp:%0.0f,rh:%0.0f",temperature,humidity);
	    }
	    strcat(g_sTmp,"}");
#endif // EMONCMS
	    if (*g_sTmp) {
	      Serial.println(g_sTmp);
	      HTTPClient http;
	      http.setUserAgent("ESP_AQI/1.0");
	      http.begin(g_sTmp);
	      int hrc = http.GET(); // send request
	      String hresp = http.getString(); // get payload
	      Serial.print("return code: ");Serial.println(hrc);
	      Serial.print("response data: ");Serial.println(hresp);
	      http.end();
	    }
	  }
	  else {
	    Serial.println("API write key not set");
	  }
	  curms = millis();
          Serial.print("updatems: ");Serial.println(curms-lastUpdateMs);
	  updateWaitMs = UPDATE_INTERVAL_MS - waitms;
	  lastUpdateMs = curms;
	  pmsx003sleep();
	}
	break;
      default:
	Serial.println("_________________");
	Serial.println(Pmsx003::errorMsg[status]);
        if (status == Pmsx003::noData) mydelay(500);
	break;
      }
	  backgroundTasks();
    } while(status != Pmsx003::OK);
#ifdef PIN_LED
    digitalWrite(PIN_LED,HIGH); // onboard LED off
#endif // PIN_LED
  }
}
