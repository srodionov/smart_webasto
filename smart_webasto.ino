// initialization
// save webasto config

  /*
  [WiFi name][Logo Conn State]
  [Webasto Name]
  [Temp][webasto.Temp][webasto.Volt]
  [timer][flame logo]
  */
  
#include "FS.h" 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <Adafruit_BME280.h>
#include <Adafruit_BMP085.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ESP8266httpUpdate.h>
#include "wbus.h"
#include "lib/logos.h"

#define SCREEN_WIDTH    128 // OLED display width, in pixels
#define SCREEN_HEIGHT   32  // OLED display height, in pixels
#define OLED_RESET      -1  // Reset pin # (or -1 if sharing Arduino reset pin)

#define rxPin           14 //
#define txPin           12 //

#define THING_TYPE      "smartWebasto"
#define FW_VERSION      "1.0.20"

#define cert_file       "/cert.der"
#define key_file        "/private.der"

#define SEALEVELPRESSURE_HPA (1013.25)

String                  mqttServer, wifiSSID, wifiPassword, chipIdHex;
int                     mqttPort        = 8883;
char                    buffer[255]     = {0};            
unsigned long           lastMsg         = 0;
unsigned long           lastMsr         = 0;
unsigned long           lastOTA         = 0;
unsigned long           wifiLastConn    = 0;
float                   temperature     = 0;
float                   pressure        = 0;
float                   altitude        = 0;
float                   humidity        = 0;
char                    displayTxt[64]  = "";
uint8_t                 logLevel        = 1;

Adafruit_BMP085         bmeSensor;
Adafruit_SSD1306        display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiClientSecure        espClient;
SoftwareSerial          SoftSerial(rxPin,txPin);
wbus                    webasto(&SoftSerial);  // pass a pointer
PubSubClient            mqttClient(espClient);

void reboot(char* reason){
    if (logLevel >= 1) { SoftSerial.println("reboot"); }
    if (logLevel >= 2) { SoftSerial.println(reason); }
    
    SoftSerial.println(reason);
    
    display.clearDisplay();
    display.setTextSize(1);             
    display.setTextColor(WHITE);        
    display.setCursor(0,0);
    display.println("Error: ");
    display.println(reason);
    display.println("<<< Reboot >>>");
    display.display();
      
    delay(5000);
    ESP.restart();
}

void mqttPublish(PubSubClient *client, String topic, const JsonObject& src)
{
  if (logLevel >= 1) { SoftSerial.println("mqttPublish"); }
      
  String outTopic = chipIdHex + "/" + topic; 
  if (logLevel >= 2) { SoftSerial.print("outTopic: "); SoftSerial.print(outTopic); }
  
  //DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(14) + 230);
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  if (topic == "system") {
    root["type"] = THING_TYPE;
    root["firmware"] = FW_VERSION;
    root["chipID"] = chipIdHex;
    //root["flashChipId"] = ESP.getFlashChipId();
    root["flashChipSize"] = ESP.getFlashChipSize();
    root["coreVersion"] = ESP.getCoreVersion();
    root["sdkVersion"] = ESP.getSdkVersion();
    root["sketchSize"] = ESP.getSketchSize();
    root["freeSketchSpace"] = ESP.getFreeSketchSpace();
    root["freeHeap"] = ESP.getFreeHeap();
  } else if (topic == "connection") {    
    root["wifi"] = wifiSSID;
    root["rssi"] = WiFi.RSSI();
    root["mac"] = WiFi.macAddress();
    IPAddress ip = WiFi.localIP();
    sprintf(displayTxt,"%d.%d.%d.%d", ip[0],ip[1],ip[2],ip[3]);
    root["ip"] = displayTxt;
  } else if (topic == "sensor") {
    root["temperature"] = temperature;
    //root["humidity"] = humidity;
    root["pressure"] = pressure;
    root["altitude"] = altitude;
  } else if (topic == "heater") {
    root["name"] = webasto.name();
    root["state"] = webasto.state();
    root["heatCountdown"] = webasto.heatCountdown();    
    root["logLevel"] = webasto.logLevel;
    root["cmdRefreshSec"] = webasto.cmdRefreshSec;
    root["cmdTimeout"] = webasto.cmdTimeout;
    root["readDelay"] = webasto.readDelay;
    root["maxRetryCount"] = webasto.maxRetryCount;
    if (webasto.state() == "ON"){
      root["voltage"] = webasto.voltage();
      root["temperature"] = webasto.temperature();
    }
    if (webasto.faultCount() > 0) {
      JsonArray& faults = root.createNestedArray("faults");
      for (int i=0; i<webasto.faultCount(); i++) faults.add(webasto.faultCode(i));
    }
  } else {
    for (auto kvp : src) {
      root[kvp.key] = kvp.value;
    }
  }
  root.printTo((char*)buffer, root.measureLength() + 1);
  client->publish(outTopic.c_str(), buffer);
  client->loop();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (logLevel >= 1) { SoftSerial.println("mqttCallback"); }
    
  int result = 0;
  String inTopic = topic, outTopic = chipIdHex + "/command";
  if (logLevel >= 2) { SoftSerial.print("inTopic: "); SoftSerial.print(inTopic); }
          
  if (inTopic != outTopic) return;
    
  const size_t bufferSize = JSON_OBJECT_SIZE(3) + 30;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success()) result = -1;
  
  String cmd = root["cmd"]; cmd.toUpperCase();
  String param = root["param"]; param.toUpperCase();  
    
  if (cmd.compareTo("ON") == 0) {
    result = webasto.turnOn();
  } else if (cmd.compareTo("OFF") == 0) {
    result = webasto.turnOff();
  } else if (cmd.compareTo("ERRLST") == 0) {
    result = webasto.readFaultCodes();
  } else if (cmd.compareTo("ERRCLR") == 0) {
    result = webasto.clearFaultCodes(); 
  } else if (cmd.compareTo("CMDQRY") == 0) {
    result = webasto.getSensorsInfo();
  } else if (cmd.compareTo("CMDCHK") == 0) {
    result = webasto.checkCmd(param.toInt());         
  } else if (cmd.compareTo("CMDRFH") == 0) {
    webasto.cmdRefreshSec = param.toInt();
  } else if (cmd.compareTo("LOGLVL") == 0) {
    webasto.logLevel=(uint8_t)param.toInt(); 
  } else if (cmd.compareTo("CMDTMO") == 0) {
    webasto.cmdTimeout=((uint8_t)param.toInt())*1000;
  } else if (cmd.compareTo("READDL") == 0) {
    webasto.readDelay=((uint8_t)param.toInt());
  } else if (cmd.compareTo("MAXRTY") == 0) {
    webasto.maxRetryCount = ((uint8_t)param.toInt());
  } else {
    result = -1;  
  } 
  root["cmdResult"] = result;

  mqttPublish(&mqttClient, "command/result", root);
}

void setup() {
  //software serial pins mode
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);

  Wire.pins(4, 5);
  Wire.begin(4, 5);
  
  SoftSerial.begin(115200);
  delay(100);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    reboot("Screen error");
  } else {
    display.clearDisplay();
    display.setTextSize(1);             
    display.setTextColor(WHITE);        
    display.setCursor(0,0);
    sprintf(displayTxt, "%s-%s", THING_TYPE, FW_VERSION);
    display.println(displayTxt);
    display.display();
  }
  
  if (!SPIFFS.begin()) {
    reboot("Mount file system");
  } else {
    
    File cert = SPIFFS.open(cert_file, "r");
    if (!cert)
      reboot("Open certificate file");
    else
      if (logLevel >= 1) { SoftSerial.println("Success to open certificate file"); }
     
    delay(1000);
    
    if (espClient.loadCertificate(cert))
      if (logLevel >= 1) { SoftSerial.println("Certificate loaded"); }
    else
      reboot("Certificate not loaded"); 
    
    File private_key = SPIFFS.open(key_file, "r");
    if (!private_key)
      reboot("Open private cert file");
    else
      if (logLevel >= 1) { SoftSerial.println("Success to open private cert file"); }

    delay(1000);
  
    if (espClient.loadPrivateKey(private_key))
      if (logLevel >= 1) { SoftSerial.println("Private Key loaded"); }
    else
      reboot("Private Key not loaded");
  }

  sprintf(displayTxt, "%06X", ESP.getChipId());
  chipIdHex.concat(displayTxt);
}

void loop() {
  long now = millis();
  
  display.clearDisplay();      
  display.setCursor(0,0);
  display.setTextSize(1); 
  
  if ((WiFi.status() != WL_CONNECTED) || (wifiSSID == "")) {
    display.println("Scanning WiFi ... ");
    if (now > (wifiLastConn + 60*1000) || (wifiLastConn == 0)) {  
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      delay(100);

      boolean settingsFound = false;
      File jsonFile = SPIFFS.open("/setting.json", "r");        
      if (jsonFile) {
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(jsonFile, jsonFile.size());
        if (root.success()) {
          mqttServer = root["mqtt"]["server"].as<String>();
          mqttPort = root["mqtt"]["port"];

          if (logLevel >= 2) { SoftSerial.print("MQTT Server: "); SoftSerial.println(mqttServer); }
          
          int32_t wifiRSSI = -1000;
          JsonArray& wifiList = root["wifi"];
          if (logLevel >= 2) { SoftSerial.println("Scanning WiFi networks ... "); }     
          int n = WiFi.scanNetworks();
          for (int i = 0; i < n; i++) {
            sprintf(displayTxt, "%s %d", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
            if (logLevel >= 2) { SoftSerial.println(displayTxt); }
            for (auto& wifi : wifiList){
              if (wifi["SSID"] == WiFi.SSID(i)) {                
                if (WiFi.RSSI(i) > wifiRSSI) {
                  wifiSSID = wifi["SSID"].as<String>();
                  wifiPassword = wifi["password"].as<String>();                                  
                  wifiRSSI = WiFi.RSSI(i);
                  settingsFound = true;
                }
              }
            }
          }
        }   
      }
      jsonFile.close();

      if (settingsFound){
        if (logLevel >= 2) { SoftSerial.println("WiFi begin"); }
                  
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        mqttClient.setServer(mqttServer.c_str(),mqttPort);
        mqttClient.setCallback(mqttCallback);
        
        if (logLevel >= 2) { SoftSerial.print("SSID: "); SoftSerial.println(wifiSSID); }
        
        wifiLastConn = millis();
      }
    }
  } else {
    display.println(wifiSSID.c_str());
    if (!mqttClient.connected()){
      if (logLevel >= 1) { SoftSerial.println("Connecting MQTT ..."); }
      if (mqttClient.connect(chipIdHex.c_str())) { 
        if (logLevel >= 2) { SoftSerial.println("MQTT connected"); }
        String outTopic = chipIdHex + "/command"; 
        mqttClient.subscribe(outTopic.c_str());   
        if (logLevel >= 2) { SoftSerial.print("Subscribed to MQTT topic: "); SoftSerial.println(outTopic); }
      } else {
        if (logLevel >= 2) { SoftSerial.println("MQTT connection failure"); }
        delay(1000);
      }
    } else {
      mqttClient.loop();
    }
  }

  //OTA
  if (WiFi.status() == WL_CONNECTED && wifiSSID != "" && webasto.state() != "ON" && (now > (lastOTA + 60*1000) || (lastOTA == 0))){
    if (logLevel >= 1) { SoftSerial.println("OTA update ..."); }
    display.println("OTA update ... ");
    display.display();

    sprintf(displayTxt, "%s-%s", THING_TYPE, FW_VERSION); 
    t_httpUpdate_return ret = ESPhttpUpdate.update("update.atomigy.com", 80, "/update.php", displayTxt);
    
    switch(ret) {
    case HTTP_UPDATE_FAILED:
        sprintf(displayTxt, "[update] Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str()); 
        break;
    case HTTP_UPDATE_NO_UPDATES:
        sprintf(displayTxt, "[update] no Update"); 
        break;
    case HTTP_UPDATE_OK:
        sprintf(displayTxt, "[update] Update ok");
        break;
    }
    if (logLevel >= 2) { SoftSerial.println(displayTxt); }
    
    display.println(displayTxt);
    display.display();
    delay(1000);
        
    lastOTA = now;         
  } else {
    
    // measure Temp
    if ((lastMsr == 0) || (now - lastMsr) > 10000) {
      lastMsr = now;
          
      if (bmeSensor.begin()) {
        temperature = bmeSensor.readTemperature();
        //humidity = bmeSensor.readHumidity();
        pressure = bmeSensor.readPressure() / 100.0F;
        altitude = bmeSensor.readAltitude(SEALEVELPRESSURE_HPA);
      }
    }
           
    display.println(webasto.name()); 
  
    sprintf(displayTxt, "%2.0fC ", temperature );  
    display.print(displayTxt); 

    if (webasto.state() == "ON") {
      sprintf(displayTxt, "%2.0f C %2.2f V", webasto.temperature(), webasto.voltage());  
      display.println(displayTxt); 
    
      uint16_t countDownSecTotal = webasto.heatCountdown();
      if (countDownSecTotal > 0) {
        uint8_t countDownMin = (int)(countDownSecTotal/60);
        uint8_t countDownSec = countDownSecTotal - countDownMin*60;
        sprintf(displayTxt, "%02d:%02d", countDownMin, countDownSec);
        display.println(displayTxt); 
      }
    }
    
    //flame logo
    if (webasto.flame()) {    
      display.drawBitmap(
        (display.width() - LOGO_WIDTH),
        (display.height() - LOGO_HEIGHT),
        flame_logo, LOGO_WIDTH, LOGO_HEIGHT, 1);
    }
  
    if (!mqttClient.connected()) {
      //offline
      display.drawBitmap(
        (display.width() - LOGO_WIDTH),
        0,
        nocloud_logo, LOGO_WIDTH, LOGO_HEIGHT, 1);                   
    } else {
      if ((lastMsg == 0) || (now - lastMsg) > 60000) {
        StaticJsonBuffer<0> jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        mqttPublish(&mqttClient, "system", root);
        mqttPublish(&mqttClient, "connection", root);
        mqttPublish(&mqttClient, "sensor", root);
        mqttPublish(&mqttClient, "heater", root);
         
        lastMsg = millis();
      }
      
      //online  
      display.drawBitmap(
        (display.width() - LOGO_WIDTH),
        0,
        cloud_logo, LOGO_WIDTH, LOGO_HEIGHT, 1);   
    }
    
    display.display();
   
    //update webasto state
    webasto.loop();
  }
}
