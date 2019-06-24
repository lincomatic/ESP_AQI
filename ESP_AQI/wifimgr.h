#include "./WiFiManager.h"          // https://github.com/tzapu/WiFiManager

typedef struct config_parms {
  char ssid[33]; // must have length+1 as w/ strings_xx.h
  char pass[65]; // must have same length+1 as w/ strings_xx.h
  //---- below additions to configuration form
  char apiwritekey[17];
  char staticIP[16]; // optional static IP
  char staticGW[16]; // optional static gateway
  char staticNM[16]; // optional static netmask
} CONFIG_PARMS;

#ifdef API_WRITE_KEY_LEN
extern char g_ApiWriteKey[API_WRITEKEY_LEN+1];
#endif

//flag for saving data

class WifiConfigurator {
  CONFIG_PARMS configParms;
  // a flag for ip setup
  boolean set_static_ip;

  //network stuff
  //default custom static IP, changeable from the webinterface
  void resetConfigParms() { memset(&configParms,0,sizeof(configParms)); }
public:
  static bool shouldSaveConfig;
  WiFiManager wifiManager;

  WifiConfigurator();

  void StartManager(void);
  void handleWebServer() { wifiManager.handleWebServer(); }
  String getUniqueSystemName();
  void printMac();
  void readCfg();
  void resetCfg(int dowifi=0);
  uint8_t getConfigSize() { return sizeof(configParms); }
};


bool WifiConfigurator::shouldSaveConfig; // instantiate

WifiConfigurator::WifiConfigurator()
{
  resetConfigParms();

  set_static_ip = false;
}



//creates the string that shows if the device goes into accces point mode
#define WL_MAC_ADDR_LENGTH 6
String WifiConfigurator::getUniqueSystemName()
{
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);


  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);

  macID.toUpperCase();
  String UniqueSystemName = String(AP_PREFIX) + macID;

  return UniqueSystemName;
}





// displays mac address on serial port
void WifiConfigurator::printMac()
{
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);

  Serial.print("MAC: ");
  for (int i = 0; i < 5; i++){
    Serial.print(mac[i], HEX);
    Serial.print(":");
  }
  Serial.println(mac[5],HEX);
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  WifiConfigurator::shouldSaveConfig = true;
}


void WifiConfigurator::StartManager(void)
{
  Serial.println("StartManager() called");

  shouldSaveConfig = false;

  // add parameter for ThingSpeak Write API key in GUI
  WiFiManagerParameter custom_apiwritekey("apiwritekey", "ThingSpeak Write API Key", configParms.apiwritekey, sizeof(configParms.apiwritekey));

  // add parameters for IP setup in GUI
  WiFiManagerParameter custom_ip("ip", "Static IP (Blank for DHCP)", configParms.staticIP, sizeof(configParms.staticIP));
  WiFiManagerParameter custom_gw("gw", "Static Gateway (Blank for DHCP)", configParms.staticGW, sizeof(configParms.staticGW));
  WiFiManagerParameter custom_nm("nm", "Static Netmask (Blank for DHCP)", configParms.staticNM, sizeof(configParms.staticNM));
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  // this is what is called if the webinterface want to save data, callback is right above this function and just sets a flag.
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // this actually adds the parameters defined above to the GUI
  wifiManager.addParameter(&custom_apiwritekey);

  // if the flag is set we configure a STATIC IP!
  if (set_static_ip) {
    //set static ip
    IPAddress _ip, _gw, _nm;
    _ip.fromString(configParms.staticIP);
    _gw.fromString(configParms.staticGW);
    _nm.fromString(configParms.staticNM);
    
    // this adds 3 fields to the GUI for ip, gw and netmask, but IP needs to be defined for this fields to show up.
    wifiManager.setSTAStaticIPConfig(_ip, _gw, _nm);
    
    Serial.println("Setting IP to:");
    Serial.print("IP: ");
    Serial.println(configParms.staticIP);
    Serial.print("GATEWAY: ");
    Serial.println(configParms.staticIP);
    Serial.print("NETMASK: ");
    Serial.println(configParms.staticNM);
  }
  else {
    // i dont want to fill these fields per default so i had to implement this workaround .. its ugly.. but hey. whatever.  
    wifiManager.addParameter(&custom_ip);
    wifiManager.addParameter(&custom_gw);
    wifiManager.addParameter(&custom_nm);
  }

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep in seconds
  // also really annoying if you just connected and the damn thing resets in the middle of filling in the GUI..
  wifiManager.setConfigPortalTimeout(10*60);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  Serial.print("***");Serial.println(getUniqueSystemName().c_str());
  if (configParms.ssid[0] && configParms.pass[0]) {
    // set stored ssid/pass
    WiFi.begin(configParms.ssid,configParms.pass);
  }
  if (!wifiManager.autoConnect(getUniqueSystemName().c_str())) {
    Serial.println("timed out and failed to connect");
    Serial.println("rebooting...");
    //reset and try again, or maybe put it to deep sleep
    reboot();
    //   we never get here.
  }

  if (*wifiManager.getNewSSID() && *wifiManager.getNewPass()) {
    if (strlen(wifiManager.getNewSSID()) >= sizeof(configParms.ssid)) {
      // shouldn't get here if # chars in form is <= struct size
      Serial.print("SSID too long: ");Serial.println(wifiManager.getNewSSID());
    }
    else if (strlen(wifiManager.getNewPass()) >= sizeof(configParms.pass)) {
      // shouldn't get here if # chars in form is <= struct size
      Serial.print("Pass too long: ");Serial.println(wifiManager.getNewPass());
    }
    else {
      strcpy(configParms.ssid,wifiManager.getNewSSID());
      strcpy(configParms.pass,wifiManager.getNewPass());
      shouldSaveConfig = true;
    }
  }
  //everything below here is only executed once we are connected to a wifi.

  //if you get here you have connected to the WiFi

  Serial.print("CONNECTED to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // this is true if we come from the GUI and clicked "save"
  // the flag is created in the callback above..
  if (shouldSaveConfig) {
    // connection worked so lets save all those parameters to the config file
#ifdef API_WRITE_KEY_LEN
    strcpy(configParms.apiwritekey, custom_apiwritekey.getValue());
    if (*configParms.apiwritekey) {
      strcpy(g_ApiWriteKey,configParms.apiwritekey);
    }
    else {
      g_ApiWriteKey[0] = 0;
    }
#endif // API_WRITE_KEY_LEN
    strcpy(configParms.staticIP, custom_ip.getValue());
    strcpy(configParms.staticGW, custom_gw.getValue());
    strcpy(configParms.staticNM, custom_nm.getValue());
    
    // if we defined something in the gui before that does not work we might to get rid of previous settings in the config file
    // so if the form is transmitted empty, delete the entries.
    //if (strlen(ip) < 8) {
    //      resetConfigParms();
    //    }
    
    
    Serial.println("saving config");
    
    int eepidx = 0;
    int i;
    EEPROM.write(eepidx++,strlen(configParms.apiwritekey));
    for (i=0;i < strlen(configParms.apiwritekey);i++) {
      EEPROM.write(eepidx++,configParms.apiwritekey[i]);
    }

    EEPROM.write(eepidx++,strlen(configParms.staticIP));
    for (i=0;i < (int)strlen(configParms.staticIP);i++) {
      EEPROM.write(eepidx++,configParms.staticIP[i]);
    }

    EEPROM.write(eepidx++,strlen(configParms.staticGW));
    for (i=0;i < (int)strlen(configParms.staticGW);i++) {
      EEPROM.write(eepidx++,configParms.staticGW[i]);
    }

    EEPROM.write(eepidx++,strlen(configParms.staticNM));
    for (i=0;i < (int)strlen(configParms.staticNM);i++) {
      EEPROM.write(eepidx++,configParms.staticNM[i]);
    }

    EEPROM.commit();

    Serial.print("TS write API key: ");
    Serial.println(*configParms.apiwritekey ? configParms.apiwritekey : "(empty)");
    Serial.print("IP: ");
    Serial.println(*configParms.staticNM ? configParms.staticNM : "(DHCP)");
    Serial.print("GW: ");
    Serial.println(*configParms.staticGW ? configParms.staticGW : "(DHCP)");
    Serial.print("NM: ");
    Serial.println(*configParms.staticNM ? configParms.staticNM : "(DHCP)");
    //end save
  }
}


void WifiConfigurator::resetCfg(int dowifi)
{
  Serial.println("resetCfg()");

  // reset config parameters
  resetConfigParms();

  // clear EEPROM
  EEPROM.write(0,0);
  EEPROM.commit();

  if (dowifi) {
    // erase WiFi settings (SSID/passphrase/etc
    WiFiManager wifiManager;
    wifiManager.resetSettings();
  }
}

void WifiConfigurator::readCfg()
{
  Serial.println("readCfg()");
  int resetit = 0;

  int eepidx = 0;
  int i;

  resetConfigParms();

  uint8_t len = EEPROM.read(eepidx++);
  if ((len > 0) && (len < sizeof(configParms.ssid))) {
    for (i=0;i < len;i++) {
      configParms.ssid[i] = EEPROM.read(eepidx++);
    }
    configParms.ssid[i] = 0;
    Serial.print("ssid: ");Serial.println(configParms.ssid);
  }
  else configParms.ssid[0] = 0;

  len = EEPROM.read(eepidx++);
  if ((len > 0) && (len < sizeof(configParms.pass))) {
    for (i=0;i < len;i++) {
      configParms.pass[i] = EEPROM.read(eepidx++);
    }
    configParms.pass[i] = 0;
    Serial.print("pass: ");Serial.println(configParms.pass);
  }
  else configParms.pass[0] = 0;

  len = EEPROM.read(eepidx++);
  if ((len == 0) || (len >= sizeof(configParms.apiwritekey))) {
    // assume uninitialized
    resetit = 1;
  }
  else {
    for (i=0;i < len;i++) {
      configParms.apiwritekey[i] = EEPROM.read(eepidx++);
    }
    configParms.apiwritekey[i] = 0;
#ifdef API_WRITE_KEY_LEN
    strcpy(g_ApiWriteKey,configParms.apiwritekey);
    Serial.print("apiwritekey: ");Serial.println(g_ApiWriteKey);
#endif // API_WRITE_KEY_LEN

    len = EEPROM.read(eepidx++);
    if ((len > 0) && (len < sizeof(configParms.staticIP))) {
      for (i=0;i < len;i++) {
	configParms.staticIP[i] = EEPROM.read(eepidx++);
      }
      configParms.staticIP[i] = 0;
      Serial.print("staticIP: ");Serial.println(configParms.staticIP);
    }
    else {
      resetit = 1;
    }

    if (!resetit) {
      len = EEPROM.read(eepidx++);
      if ((len > 0) && (len < sizeof(configParms.staticGW))) {
	for (i=0;i < len;i++) {
	  configParms.staticGW[i] = EEPROM.read(eepidx++);
	}
	configParms.staticGW[i] = 0;
	Serial.print("staticGW: ");Serial.println(configParms.staticGW);
      }
      else {
	resetit = 1;
      }
    }

    if (!resetit) {
      len = EEPROM.read(eepidx++);
      if ((len > 0) && (len < sizeof(configParms.staticNM))) {
	for (i=0;i < len;i++) {
	  configParms.staticNM[i] = EEPROM.read(eepidx++);
	}
	configParms.staticNM[i] = 0;
      }
      else {
	resetit = 1;
      }
    }
  }

  if (!resetit) { // valid config
    if (*configParms.staticIP) {
      // lets use the IP settings from the config file for network config.
      Serial.print("staticIP: ");Serial.println(configParms.staticIP);
      set_static_ip = 1;
    }
    else {
      Serial.println("using DHCP");
    }
  }
  else {
    resetCfg(0);
  }
}

WifiConfigurator g_wfCfg;

