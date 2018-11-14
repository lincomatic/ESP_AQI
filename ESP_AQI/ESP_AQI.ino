#include <eagle_soc.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "./pms.h"
#include "./AM2320.h"

#define WIFI_MGR // use AP mode to configure via captive portal
#define OTA_UPDATE // OTA firmware update

//#define PIN_RX 5// LoLin/WeMos D1/GPIO5
//#define PIN_TX 4 // LoLin/WeMos D2/GPIO4
#define PIN_RX 14 // LoLin/WeMos D5/GPIO14
#define PIN_TX 12 // LoLin/WeMos D6/GPIO12
#define PIN_LED    2 //0
#define PIN_FACTORY_RESET 12 // LoLin D6/GPIO12 ground this pin to wipe out EEPROM & WiFi settings

#define AP_PREFIX "ESPAQI_"

//#define THINGSPEAK
#define EMONCMS

#define EMONCMS_BASE_URI "http://data.openevse.com/emoncms/input/post.json?node="
#define EMONCMS_NODE "aqi0"
#define EMONCMS_WRITE_KEY "emoncms-write-key"

#define READ_INTERVAL_MS 30000UL
#define UPDATE_INTERVAL_MS 30000UL

#ifdef THINGSPEAK
#define API_WRITEKEY_LEN 16
#endif
#ifdef EMONCMS
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


void setup(void)
{
  // onboard leds also outputs
  pinMode(PIN_LED, OUTPUT); // onboard LED
  digitalWrite(PIN_LED,HIGH); // turn off onboard LED
  
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
  
  Serial.println("Pmsx003");
  
  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModeActive);

  am2320.begin();
}

////////////////////////////////////////

unsigned long lastRead = 0UL;
unsigned long lastUpdateMs = 0UL;
char g_sTmp[256];

void loop(void)
{
  btnReset.read();
  if (btnReset.longPress()) {
    Serial.println("long press");
    factoryReset();
  }

#ifdef OTA_UPDATE
  AOTAMgr.handle();
#endif


  unsigned long curms = millis();
  if ((curms-lastRead) >= READ_INTERVAL_MS) {
    digitalWrite(PIN_LED,LOW); // onboard LED on
    lastRead = curms;
    const auto n = Pmsx003::Reserved;
    Pmsx003::pmsData data[n];

    Pmsx003::PmsStatus status = pms.read(data, n);
  
    switch (status) {
    case Pmsx003::OK:
      //    digitalWrite(PIN_LED,LOW); // onboard LED on
      {
	Serial.println("_________________");
	auto newRead = millis();
	Serial.print("Wait time ");
	Serial.println(newRead - lastRead);
	lastRead = newRead;
      
	// For loop starts from 3
	// Skip the first three data (PM1dot0CF1, PM2dot5CF1, PM10CF1)
	for (size_t i = Pmsx003::PM1dot0; i < n; ++i) { 
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
	  humidity = am2320.getHumidity();
	  sprintf(g_sTmp,"temp=%f humidity=%f",temperature,humidity);
	}
	else { // error
	  sprintf(g_sTmp,"error %d",am2320.getErrorCode());
	}
	Serial.println(g_sTmp);

	if ((curms-lastUpdateMs) >= UPDATE_INTERVAL_MS) {
	  if (1) {//	  if (*g_ApiWriteKey) {
*g_sTmp = 0;
#ifdef THINGSPEAK
	    sprintf(g_sTmp,"http://api.thingspeak.com/update?api_key=%s&field1=%d&field2=%d&field3=%d",g_ApiWriteKey,data[Pmsx003::PM1dot0],data[Pmsx003::PM2dot5],data[Pmsx003::PM10dot0]);
	    if (arc == true) {
	      sprintf(g_sTmp+strlen(g_sTmp),"&field4=%f&field5=%f",temperature,humidity);
	    }
#elif defined(EMONCMS)
      	    const char *baseuri = EMONCMS_BASE_URI;
	    const char *node = EMONCMS_NODE;
	    const char *apikey = EMONCMS_WRITE_KEY;
	    sprintf(g_sTmp,"%s%s&apikey=%s&json={pm1:%d,pm25:%d,pm10:%d",baseuri,node,apikey,data[Pmsx003::PM1dot0],data[Pmsx003::PM2dot5],data[Pmsx003::PM10dot0]);
	    if (arc == true) {
	      sprintf(g_sTmp+strlen(g_sTmp),",temp:%f,rh:%f",temperature,humidity);
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
	  lastUpdateMs = curms;
	}
	break;
      }
    case Pmsx003::noData:
      break;
    default:
      Serial.println("_________________");
      Serial.println(Pmsx003::errorMsg[status]);
    }
    digitalWrite(PIN_LED,HIGH); // onboard LED off
  }
}
