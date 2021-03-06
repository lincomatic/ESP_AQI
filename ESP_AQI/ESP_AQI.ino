// -*- C++ -*-
// compile with WeMos D1 R1 profile
//
#include <eagle_soc.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "Wire.h"
#include "./pms.h"
#include "./defines.h"

#ifdef OTA_UPDATE
#include "./ArduinoOTAMgr.h"
ArduinoOTAMgr AOTAMgr;
#endif

#ifdef PIN_RX1
Pmsx003 pms1(PIN_RX1, PIN_TX);
#endif // PIN_RX1
#ifdef PIN_RX2
Pmsx003 pms2(PIN_RX2, PIN_TX);
#endif // PIN_RX2

#ifdef OLED128X64
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#endif //OLED128X64

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

#ifdef USE_MCP9808
#include "./MCP9808.h"
MCP9808 mcp9808;
#endif //USE_MCP9808

typedef struct auxdata {
#ifdef USE_AM2320
  float atemp;
  float arh;
#endif // USE_AM2320
#ifdef USE_BME280
  float btemp;
  float brh;
  float airPressure;
#endif // USE_BME280
#ifdef USE_MCP9808
  float mtemp;
#endif // USE_MCP9808
} AUX_DATA;


//
// global variables
//
AUX_DATA g_auxData;
unsigned long lastUpdateMs = 0UL;
unsigned long updateWaitMs = 0UL;
char g_sTmp[512];
char g_sHttpStr[512];
#ifdef OLED128X64
SSD1306AsciiWire g_oled;
#endif //OLED128X64


const auto n = Pmsx003::Reserved;
#ifdef PIN_RX1
Pmsx003::pmsData data[n];
#endif // PIN_RX1
#ifdef PIN_RX2
Pmsx003::pmsData data2[n];
#endif // PIN_RX2
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
#ifdef USE_BME280
  bme280.reset();
#endif
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


void ReadAux()
{
#ifdef USE_AM2320	  
  Serial.print("---\nam2320: ");
  bool arc = am2320.measure();
  if (arc == true) {
    g_auxData.atemp = am2320.getTemperature();
#ifdef TEMPERATURE_FAHRENHEIT
    g_auxData.atemp = (g_auxData.atemp * (9.0F / 5.0F)) + 32.0F;
#endif // TEMPERATURE_FAHRENHEIT
    g_auxData.arh = am2320.getHumidity();
    sprintf(g_sTmp, "temp=%0.1f humidity=%0.1f", g_auxData.atemp, g_auxData.arh);
  }
  else { // error
    //	    temperature = -99;
    //	    humidity = -1;
    sprintf(g_sTmp, "error %d", am2320.getErrorCode());
    g_auxData.atemp = TEMPERATURE_NOT_INSTALLED;
  }
  Serial.println(g_sTmp);
  backgroundTasks();
#endif // USE_AM2320
  
#ifdef USE_BME280
  if (bme280Present) {
#ifdef BME280_LOW_POWER
    bme280.setMode(MODE_FORCED); //Wake up sensor and take reading
    unsigned long st = millis();
    while ((bme280.isMeasuring() == false) && ((millis()-st) < 250UL)); //Wait for sensor to start measurment
    while ((bme280.isMeasuring() == true) && ((millis()-st) < 500UL)) delay(1); //Hang out while sensor completes the reading    
    //      Serial.print(" Measure time(ms): ");
    //      Serial.println(millis() - st);
#endif //BME280_LOW_POWER
    
    // N.B. *MUST* read temperature before pressure to load t_fine variable
#ifdef TEMPERATURE_FAHRENHEIT
    g_auxData.btemp = bme280.readTempF();
#else
    g_auxData.btemp = bme280.readTempC();
#endif //TEMPERATURE_FAHRENHEIT
    g_auxData.btemp += BME280_TEMP_CORRECTION;
    g_auxData.brh = bme280.readFloatHumidity() + BME280_RH_CORRECTION;
    g_auxData.airPressure = bme280.readFloatPressure();
    sprintf(g_sTmp, "BME280: temp %f F, rh: %f %%, pressure: %f", g_auxData.btemp, g_auxData.brh, g_auxData.airPressure);
    Serial.println(g_sTmp);
#ifdef OLED128X64
    g_oled.clear();
    sprintf(g_sTmp,"T: %0.1fF",g_auxData.btemp);
    g_oled.println(g_sTmp);
    float rh = g_auxData.brh;
    if (rh < 0.0F) rh = 0.0F;
    else if (rh > 100.0F) rh = 100.0F;
    sprintf(g_sTmp,"RH: %0.0f%%",rh);
    g_oled.println(g_sTmp);
    sprintf(g_sTmp,"AP: %0.2f in",g_auxData.airPressure*0.0002952998751);
    g_oled.println(g_sTmp);
#endif // OLED128X64
  }
  else {
    g_auxData.btemp = TEMPERATURE_NOT_INSTALLED;
  }
  backgroundTasks();
#endif // USE_BME280

#ifdef USE_MCP9808
  int16_t c10 = mcp9808.readAmbient(); // celcius * 10
  if (c10 != TEMPERATURE_NOT_INSTALLED) {
#ifdef TEMPERATURE_FAHRENHEIT
    g_auxData.mtemp = (((float)c10) * (9.0F / 50.0F)) + 32.0F;
#else
    g_auxData.mtemp = ((float)c10)/10.0F;
#endif //TEMPERATURE_FAHRENHEIT
    sprintf(g_sTmp, "MCP9808 temp=%0.1f", g_auxData.mtemp);
  }
  else {
    g_auxData.mtemp = TEMPERATURE_NOT_INSTALLED;
    strcpy(g_sTmp,"error reading MCP9808");
  }
  Serial.println(g_sTmp);
#endif // USE_MCP9808

  mydelay(1000);
}


void setup(void)
{
#ifdef PIN_RX1
  pms1.begin();
  pms1.waitForData(Pmsx003::wakeupTime);
  pms1.write(Pmsx003::cmdModeActive);
#endif // PIN_RX1
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

  Wire.begin();
  Wire.setClock(400000L); //Increase to fast I2C speed!

#ifdef OLED128X64
  g_oled.begin(&Adafruit128x64, OLED_I2C_ADDR);
  g_oled.setFont(Arial14);
  g_oled.clear();
#endif // OLED128X64

#ifdef PIN_FACTORY_RESET
  btnReset.init();
#endif
  
#ifdef WIFI_MGR
#ifdef OLED128X64
  g_oled.print("configuring\nWiFi...");
#endif // OLED128X64

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
  

#ifdef OLED128X64
  g_oled.clear();

  g_oled.println("SSID:");
  g_oled.println(WiFi.SSID());
  g_oled.print("IP: "); g_oled.println(WiFi.localIP());
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);

  g_oled.print("MA: ");
  for (int i = 0; i < 5; i++){
    g_oled.print(mac[i], HEX);
    g_oled.print(":");
  }
  g_oled.println(mac[5],HEX);
#endif //OLED128X64

#ifdef USE_AM2320 
  am2320.begin();
#endif //USE_AM2320

#ifdef USE_BME280
  bme280.setI2CAddress(BME280_I2C_ADDR);
  if (!bme280.beginI2C()) {
    Serial.println("BME280 connect failed");
    bme280Present = false;
  }
  else {
    bme280Present = true;
	Serial.println("BME280 connected");
#ifdef BME280_LOW_POWER
    bme280.setMode(MODE_SLEEP); //Sleep for now
#endif
  }

#endif // USE_BME280

#ifdef USE_MCP9808
  mcp9808.begin();
#endif //USE_MCP9808


#ifdef testaux
  while (1) ReadAux();
#endif 

  Serial.println("setup done");
}

////////////////////////////////////////



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

#if (PMS_SLEEP_WAKEUP_WAIT != 0)
  pmsx003wake();
  // wait for PMS to stabilize
  mydelay(PMS_SLEEP_WAKEUP_WAIT);
#endif // PMS_SLEEP_WAKEUP_WAIT

  Serial.println("READ");
  backgroundTasks();
#ifdef PIN_RX1
  int rc1 = readPms(&pms1,data);
  backgroundTasks();
#endif // PIN_RX1
#ifdef PIN_RX2
  int rc2 = readPms(&pms2,data2);
  backgroundTasks();
#endif // PIN_RX2

#if (PMS_SLEEP_WAKEUP_WAIT != 0)
  pmsx003sleep();
#endif // PMS_SLEEP_WAKEUP_WAIT
  ReadAux();

#ifdef OLED128X64
  g_oled.clear();
  sprintf(g_sTmp,"PM2.5: %d %d",data[Pmsx003::PM2dot5],data[Pmsx003::PM2dot5CF1]);
  g_oled.println(g_sTmp);
#ifdef USE_BME280
  if (g_auxData.btemp != TEMPERATURE_NOT_INSTALLED) {
    sprintf(g_sTmp,"T: %0.1fF",g_auxData.btemp+OLED_BME280_TEMP_CORRECTION);
    g_oled.println(g_sTmp);
    float rh = g_auxData.brh+OLED_BME280_RH_CORRECTION;
    if (rh < 0.0F) rh = 0.0F;
    else if (rh > 100.0F) rh = 100.0F;
    sprintf(g_sTmp,"RH: %0.0f%%",rh);
    g_oled.println(g_sTmp);
    sprintf(g_sTmp,"AP: %0.2f in",g_auxData.airPressure*0.0002952998751);
    g_oled.println(g_sTmp);
  }
#elif defined(USE_AM2320)
  if (g_auxData.atemp != TEMPERATURE_NOT_INSTALLED) {
    sprintf(g_sTmp,"T: %0.1fF",g_auxData.atemp+OLED_AM2320_TEMP_CORRECTION);
    g_oled.println(g_sTmp);
    float rh = g_auxData.arh+OLED_AM2320_RH_CORRECTION;
    if (rh < 0.0F) rh = 0.0F;
    else if (rh > 100.0F) rh = 100.0F;
    sprintf(g_sTmp,"RH: %0.0f%%",rh);
    g_oled.println(g_sTmp);
  }
#endif //USE_AM2320
#endif // OLED128X64
    
#ifdef API_WRITEKEY_LEN
  if (*g_ApiWriteKey) {
    *g_sTmp = 0;
    *g_sHttpStr = 0;
      
#ifdef THINGSPEAK
    sprintf(g_sTmp,"http://api.thingspeak.com/update?api_key=%s&field1=%d&field2=%d&field3=%d",g_ApiWriteKey,data[Pmsx003::PM1dot0],data[Pmsx003::PM2dot5],data[Pmsx003::PM10dot0]);
    if (g_auxData.arh >= 0) {
      sprintf(g_sTmp+strlen(g_sTmp),"&field4=%0.1f&field5=%0.1f",g_auxData.atemp,g_auxData.arh);
    }
#elif defined(EMONCMS)
    const char *baseuri = EMONCMS_BASE_URI;
    const char *node = EMONCMS_NODE;
    sprintf(g_sTmp,"%s%s&json={",baseuri,node);
    int baselen = strlen(g_sTmp);
#ifdef PIN_RX1
    if (!rc1) {
      sprintf(g_sTmp+strlen(g_sTmp),"pm1:%d,pm25:%d,pm10:%d,pm1cf1:%d,pm25cf1:%d,pm10cf1:%d,ppd03:%d,ppd05:%d,ppd1:%d,ppd25:%d,ppd50:%d,ppd10:%d",data[Pmsx003::PM1dot0],data[Pmsx003::PM2dot5],data[Pmsx003::PM10dot0],data[Pmsx003::PM1dot0CF1],data[Pmsx003::PM2dot5CF1],data[Pmsx003::PM10dot0CF1],data[Pmsx003::Particles0dot3],data[Pmsx003::Particles0dot5],data[Pmsx003::Particles1dot0],data[Pmsx003::Particles2dot5],data[Pmsx003::Particles5dot0],data[Pmsx003::Particles10]);
    }
#endif // PIN_RX1

#ifdef PIN_RX2
    if (!rc2) {
      if (strlen(g_sTmp) > baselen) strcat(g_sTmp,",");
      sprintf(g_sTmp+strlen(g_sTmp),"pm1_2:%d,pm25_2:%d,pm10_2:%d,pm1cf1_2:%d,pm25cf1_2:%d,pm10cf1_2:%d,ppd03_2:%d,ppd05_2:%d,ppd1_2:%d,ppd25_2:%d,ppd50_2:%d,ppd10_2:%d",data2[Pmsx003::PM1dot0],data2[Pmsx003::PM2dot5],data2[Pmsx003::PM10dot0],data2[Pmsx003::PM1dot0CF1],data2[Pmsx003::PM2dot5CF1],data2[Pmsx003::PM10dot0CF1],data2[Pmsx003::Particles0dot3],data2[Pmsx003::Particles0dot5],data2[Pmsx003::Particles1dot0],data2[Pmsx003::Particles2dot5],data2[Pmsx003::Particles5dot0],data2[Pmsx003::Particles10]);
    }
#endif // PIN_RX2

#ifdef USE_AM2320
    if (g_auxData.atemp != TEMPERATURE_NOT_INSTALLED) {
      if (strlen(g_sTmp) > baselen) strcat(g_sTmp,",");
      sprintf(g_sTmp+strlen(g_sTmp),"tempa:%0.1f,rha:%0.1f",g_auxData.atemp,g_auxData.arh);
    }
#endif // USE_AM2320

#ifdef USE_BME280
    if (g_auxData.btemp != TEMPERATURE_NOT_INSTALLED) {
      if (strlen(g_sTmp) > baselen) strcat(g_sTmp,",");
      sprintf(g_sTmp+strlen(g_sTmp),"temp:%0.1f,rh:%0.1f,airprs:%0.0f",g_auxData.btemp,g_auxData.brh,g_auxData.airPressure);
    }
#endif //USE_BME280
    
#ifdef USE_MCP9808
    if (g_auxData.mtemp != TEMPERATURE_NOT_INSTALLED) {
      if (strlen(g_sTmp) > baselen) strcat(g_sTmp,",");
      sprintf(g_sTmp+strlen(g_sTmp),"tempm:%0.1f",g_auxData.mtemp);
    }
#endif // USE_MCP9808

#ifndef NORSSI
      if (strlen(g_sTmp) > baselen) strcat(g_sTmp,",");
      sprintf(g_sTmp+strlen(g_sTmp),"rssi:%d",WiFi.RSSI());
#endif // NORSSI

      sprintf(g_sTmp+strlen(g_sTmp),"}&apikey=%s",EMONCMS_WRITE_KEY);

#endif // EMONCMS
    if (*g_sTmp) {
      Serial.println(g_sTmp);
      for (int i=0;i < 2;i++) {
	backgroundTasks();
	HTTPClient http;
	http.setUserAgent("ESP_AQI/1.0");
	//	strcpy(g_sTmp,"http://www.google.com/gg");
	http.begin(g_sTmp);
	strcpy(g_sHttpStr,g_sTmp);
	int hrc = http.GET(); // send request
	Serial.print("return code: ");Serial.println(hrc);
	String hresp = http.getString(); // get payload
	Serial.print("response data: ");Serial.println(hresp);
	sprintf(g_sHttpStr+strlen(g_sHttpStr),"<br>rc: %d",hrc);
	http.end();
	mydelay(250);
	if (hrc > 0) break;
	//dbg	if (i==4) while (1) backgroundTasks();
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
  if (updateWaitMs > UPDATE_INTERVAL_MS) updateWaitMs = 0;
  Serial.print("waiting updateWaitMs: ");Serial.println(updateWaitMs);
  mydelay(updateWaitMs);
}
