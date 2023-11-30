#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <EEPROM.h>

#define EEPROM_SIZE 1024
#define BUF_VOLT 100
#define BUF_VOLT_LONG 200
#define BUF_CLICK 60
#define BUF_CLICK_HOUR 60
#define LED_0 0 
#define LED_1 2
#define LED_2 13
#define LED_3 15
#define LED_4 4
#define BUCK_PIN 12
#define BOOTBUTTON 12
#define CLICKER_PWM_CHANNEL 11
#define BUCK_PWM_CHANNEL 0
#define CLICKER 32 //Speaker element
#define REF_PIN 39 //ADC Input
#define PULSEPIN 23 //Geiger pulse input
#define SERVER_IP       "192.168.0.30"
#define AP_IP           "10.0.10.1"
#define NTP_SERVER      "no.pool.ntp.org"
#define LOG_PORT        8080
#define DEVICE_HOSTNAME "EIKGEIGER-WIFI" 


const char *HELP ="|----------------------------------------------------------------|\n"
                  "|   Characters  |   Unit                  |   Value   | DEFAULT  |\n"
                  "|---------------|-------------------------|-----------|----------|\n"
                  "| VO            |  VOLT (value/4096 %)    | 0-2000    | 400      |\n"
                  "| VF            |  FREQUENCY (hz)         | 0-18000   | 1001     |\n"
                  "| SA            |  SAVE TO EPPROM         | 1         | -        |\n"
                  "| RE            |  REBOOT SYSTEM          | 1         | -        |\n"
                  "| RF            |  FACTORYRESET(FULL/PART)| 9999/1111 | -        |\n"
                  "| BE            |  BEEPER (activation)    | 0/1       | 0        |\n"
                  "| BD            |  BEEPER duty (val/255)  | 0-127     | 50       |\n"
                  "| BF            |  BEEPER FREQUENCY (hz)  | 10-40000  | 6000     |\n"
                  "| LE            |  LED (activation)       | 0/1       | 1        |\n"
                  "| LF            |  FLASHTIME (mS)         | 1-500     | 10       |\n"
                  "| LO            |  LOG OUTPUT(Activation) | 0/1       | 1        |\n"
                  "| LS            |  LOG SPEED (ms/log)     | 0-10000   | 1000     |\n"
                  "| EV            |  EVENTSPERCLICK (number)| 0-1000    | 0        |\n"
                  "|----------------------------------------------------------------|\n";


// Helper menu to explain the different values
const char *EXPLAIN ="|--------------------------------------------------------------------------|\n"
                     "|   Variabel  |   Explaination                                             |\n"
                     "|-------------|------------------------------------------------------------|\n"
                     "| raw_volt    |integer between 0 and 2048, mapped to voltage range 0-1300  |\n"
                     "| set_volt    |integer between 0 and 2048. Duty cycle                      |\n"
                     "| cpmM        |floating average 60 seconds clicks per min                  |\n"
                     "| cpmH        |floating average clicks per min over an hour                |\n"
                     "| cpm         |cpm calculated from number of ms since last click           |\n"
                     "|--------------------------------------------------------------------------|\n";

int   buf_volt[BUF_VOLT]   = {0};
int   buf_volt_index       = 0;
float buf_volt_avg         = 0.0;

unsigned long buf_click[BUF_CLICK]  = {0};
int           buf_click_index       =  0;
float         buf_click_avg         =  0.0;

unsigned long buf_click_hour[BUF_CLICK]  = {0};
int           buf_click_hour_index       =  0;
float         buf_click_hour_avg         =  0.0;
float cpm = 0;


volatile unsigned long clicks = 0;
volatile unsigned long click_old=0;
volatile unsigned long ticks = micros();
volatile unsigned long ticks_old = ticks-10;
volatile unsigned long ticks_dt = 0;

unsigned long previousMillis_wifi = 0;
int interval_wifi = 30000;

unsigned long duration = millis();

unsigned long first = millis();
unsigned long last = first+1;
unsigned long now = last+1;

unsigned long last_2 = now+1;
unsigned long now_2 = last_2+1;

int num_radmon_fails = 0;

//server
int statusCode;
const char* ssid = "Default SSID";
const char* passphrase = "Default passord";
String st;
String content;
String esid;
String epass = "";

DNSServer dnsServer;
AsyncWebServer server(80);

bool wifi_conn = false;

typedef struct {
  //HV PWM DUTY CYCLE config_data.BUCK_PWM_SETPOINT/4095
  uint BUCK_PWM_SETPOINT=400;
  //PWM Frequency
  uint BUCK_PWM_FREQ = 1001;
  //CLICK SOUND ONOFF
  bool CLICKER_ACTIVATE = false;
  //Duty cycle for clicker config_data.CLICKER_DUTYCYCLE/255
  uint CLICKER_DUTYCYCLE = 50;
  //CLICKER PWM FREQUENCY
  uint CLICKER_FREQ = 6000;
  //Activate click indication leds
  bool LED_ACTIVATE = true;
  //How long to flash and click
  uint INDICATION_LENGTH = 10;
  //LOG OUTPUT onoff
  bool LOG_OUTPUT = true;
  //LOG OUTPUT WAITTIME
  uint LOG_SPEED = 1000;
  //Number of events per indication
  uint EVENT_DELAY = 0;
  //MCU-chip-UID
  char ssid[23]={};
  //Stored ssid name up to 128 chars
  char WIFI_SSID[128]={};
  //Stored password up to 128 chars
  char WIFI_PASS[128]={};
  //Stored IP
  byte ip[4] ={0,0,0,0};
  //radmon_username
  char RADMON_USER[128]={};
  //Stored password up to 128 chars
  char RADMON_PASS[128]={};
  //NTP Server
  char server_ip[30] = SERVER_IP;
  char ap_ip[30]     = AP_IP;
  char ntp_server[40] = NTP_SERVER;
  int  log_port= LOG_PORT;
  char device_hostname[64] = DEVICE_HOSTNAME;
} Configuration;

Configuration config_data;
Configuration config_data_bcp;

void loadConfig(Configuration& config) {
  EEPROM.get(0, config);
}

void saveConfig(Configuration& config) {
  EEPROM.put(0, config);
  EEPROM.commit();
}

void eepromCheck(){
  
  char ssid[23];
  snprintf(ssid, 23, "MCUDEVICE-%llX", ESP.getEfuseMac());
  Serial.print("ID: ");
  Serial.println(ssid);
  loadConfig(config_data);
  strcpy(config_data_bcp.ssid,ssid);

  if( strcmp(config_data.ssid, config_data_bcp.ssid) == 0){
    Serial.print("Loaded eeprom config successfully");
  }
  else{
    saveConfig(config_data_bcp);
    Serial.print("No eeprom config detected, initialized new config");
  }
}

void erase_eeprom(){
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  delay(500);
}

void IRAM_ATTR ISR() {
  ticks_old = ticks;
  ticks = micros();
  clicks++;
  ticks_dt = ticks-ticks_old;
}

volatile unsigned long reset_clicks = 0;
volatile unsigned long reset_ticks_old = 0;
volatile unsigned long reset_ticks = micros();

void IRAM_ATTR RESET_INT() {

  reset_clicks++;
  reset_ticks = millis();
  if(reset_ticks_old-reset_ticks > 2000){
    reset_clicks = 0;
  }

  if(reset_clicks > 5){
    erase_eeprom();
    saveConfig(config_data_bcp);
    esp_restart();
  }
}

float array_calculate_avg(int * buf, int len){
      int sum = 0;

      for (int i = 0; i < len; i++)
      {
          sum += buf[i];
      }

      return ((float) sum) / ((float) len);
}


float array_minmax_avg(unsigned long * buf, int len){
      unsigned long maxVal = buf[0];
      unsigned long minVal = buf[0];

      for (int i = 0; i < len; i++)
      {
        if (buf[i] > maxVal) {
          maxVal = buf[i];
              }
        if (buf[i] < minVal) {
          minVal = buf[i];
        }
      }

      return ((float) (maxVal-minVal));
}

float adc2volt(unsigned int adc){
  float volt = map(adc, 0, 2047, 150, 2450);
  volt = volt / 1000.;
  return volt;
}

float voltdivider(float volt, int r1, int r2){
  return volt*float((r1+r2)/r2);
}

int revsetpoint(float volt){
  float r1 = 50000.;
  float r2 = 100.;
  volt = 1000*volt/float((r1+r2)/r2);
  int adc = map(volt, 150, 2450, 0, 2047);
  return adc;
}

void printbuf(unsigned int *buf, int len){
    for (int i = 0; i < len; i++){
        Serial.print(buf[i]);
        Serial.print("\t");
    }
    Serial.println();
}

void scanwifi(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print("\t(");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.print("\t");
      Serial.print(WiFi.SSID(i));
      Serial.println();
    }
  }
  Serial.println();
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += "\t(";
    st += WiFi.RSSI(i);
    st += ")";
    //st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
}

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE html><html><head><title>EIKGEIGER CONFIG</title></head><body>");
    response->print( "<h1>EIKGEIGER CONFIG</h1>");
    response->printf("<p>The geiger counter is active, it is configured for %i /4096 duty cycle</p>",config_data.BUCK_PWM_SETPOINT);
    response->print( "<p>To change voltage or params use a serial monitor and connect with  115200 baud.</p>");
    response->print( "<p>Send 'H' over serial to get a help menu<br><br></p>");
    response->printf("<p>Average CPM last minute is  %.2f clicks per minute</p>", array_minmax_avg(buf_click, BUF_CLICK));
    response->print( "<p>Please write which wifi you want to connect and radmon credentials</p>");
    response->printf("<p>Try opening <a href='http://%s'>this link</a> to get ip page</p>", WiFi.softAPIP().toString().c_str());
    response->print( st );
    response->printf( "<form action='./post' method='POST'>"
                  "<p><label for='ssid'>SSID, write name for found wifi</label><br>"
                  "<input type='text' id ='ssid' name='ssid' value='%s'><br><br>",config_data.WIFI_SSID);

    response->print("<label for='pass'>WIFI Password </label><br>"
                      "<input type='text' id ='pass' name='pass'><br><br>");

    response->printf("<label for='radmon_username'>Radmon_username, register user at <a href='https://radmon.org/index.php/register'>Radmon.org</a></label><br>"
                      "<input type='text' id ='radmon_username' name='radmon_username' value='%s'><br><br>",config_data.RADMON_USER);
                    
    response->printf("<label for='radmon_pass'>Radmon device password (NOT login password)</a></label><br>"
                      "<input type='text' id ='radmon_pass' name='radmon_pass' value='%s'><br><br>",config_data.RADMON_PASS);

    response->print(  "<input type ='submit' value ='Submit'></p></form>" );
    response->print( "</body></html>");
    request->send(response);
  }
};


void start_main_page(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print("<!DOCTYPE html><html><head><title>EIKGEIGER CONFIG</title></head><body>");
  response->print( "<h1>EIKGEIGER CONFIG</h1>");
  response->printf("<p>The geiger counter is active, it is configured for %i /4096 duty cycle</p>",config_data.BUCK_PWM_SETPOINT);
  response->print( "<p>To change voltage or params use a serial monitor and connect with  115200 baud.</p>");
  response->print( "<p>Send 'H' over serial to get a help menu<br><br></p>");
  response->printf("<p>Average CPM last minute is  %.2f clicks per minute</p>", array_minmax_avg(buf_click, BUF_CLICK));
  response->print( "<p>Please write which wifi you want to connect and radmon credentials</p>");
  response->printf("<p>Try opening <a href='http://%s/cpm'>this link</a> to get API page</p>", WiFi.localIP().toString().c_str());
  response->printf( "<form action='./post' method='POST'>"
                    "<p><label for='ssid'>SSID, write name for found wifi</label><br>"
                    "<input type='text' id ='ssid' name='ssid' value='%s'><br><br>",config_data.WIFI_SSID);

  response->printf("<label for='pass'>WIFI Password </label><br>"
                    "<input type='text' id ='pass' name='pass'value='%s'><br><br>",config_data.WIFI_PASS);

  response->printf("<label for='radmon_username'>Radmon_username, register user at <a href='https://radmon.org/index.php/register'>Radmon.org</a></label><br>"
                    "<input type='text' id ='radmon_username' name='radmon_username' value='%s'><br><br>",config_data.RADMON_USER);
                  
  response->printf("<label for='radmon_pass'>Radmon device password (NOT login password)</a></label><br>"
                    "<input type='text' id ='radmon_pass' name='radmon_pass' value='%s'><br><br>",config_data.RADMON_PASS);

  response->print(  "<input type ='submit' value ='Submit'></p></form>" );
  response->print( "</body></html>");
  request->send(response);
  });
}

void start_response_server(){
  server.on("/post", HTTP_POST, [](AsyncWebServerRequest * request) 
  {
    int paramsNr = request->params(); // number of params (e.g., 1)
    /*
    0 Param name: ssid                Value: testssid
    1 Param name: pass                Value: testpassord
    2 Param name: ip                  Value: 1.2.3.4
    3 Param name: gateway             Value: 255.255.255.255
    4 Param name: radmon_username     Value: testrad
    5 Param name: radmon_pass         Value: testradpass
    */
    //SSID
    AsyncWebParameter * r = request->getParam(0); // 1st parameter
    String string_val = r->value();
    char data[128] = {};
    strcpy(data,string_val.c_str());
    if (strlen(data) != 0){
      strcpy(config_data.WIFI_SSID,data);
    }
    //SSID_pass
    r = request->getParam(1); // 1st parameter
    string_val = r->value();
    data[0] = '\0';
    strcpy(data,string_val.c_str());
    strcpy(config_data.WIFI_PASS,data);
    
    //RADMON_USER
    r = request->getParam(2); // 1st parameter
    string_val = r->value();
    data[0] = '\0';
    strcpy(data,string_val.c_str());
    strcpy(config_data.RADMON_USER,data);
    
    //RADMON_USER_PASS
    r = request->getParam(3); // 1st parameter
    string_val = r->value();
    data[0] = '\0';
    strcpy(data,string_val.c_str());
    strcpy(config_data.RADMON_PASS,data);

    request->send(200);

    saveConfig(config_data);
    esp_restart();
  });

  server.on("/cpm", HTTP_GET, [](AsyncWebServerRequest * request) 
  {
     AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->printf("{\"data\":{\"cpm\":%.3f,\"cpm_M\":%.2f,\"cpm_H\":%.2f,\"raw_volt\":%i}}",
                      float(60/float(ticks_dt/1e6)),
                      array_minmax_avg(buf_click, BUF_CLICK),
                      array_minmax_avg(buf_click_hour, BUF_CLICK)/60.,
                      analogRead(REF_PIN)
    );

    request->send(response);
  });
}

void ap_init(){
  scanwifi();
  WiFi.softAP("EIKGEIGER-captive");
  start_response_server();
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP
  //more handlers...
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();
}

void serial_handler(){
  if (Serial.available()!=0){
    char inchar = ' ';
      double serial_input;

      inchar = Serial.read();
      switch (inchar){
          case 'H':
            //Help Menu
            Serial.print(HELP);
          break;
          case '?':
            // Variable Explaination
            Serial.print(EXPLAIN);
          case 'V':
            inchar = Serial.read();
            if(inchar == 'O'){
              //PWM Duty cycle
              serial_input= Serial.parseFloat();
              config_data.BUCK_PWM_SETPOINT=serial_input;
              ledcWrite(BUCK_PWM_CHANNEL, config_data.BUCK_PWM_SETPOINT);
              Serial.print("Set HV pwm duty cycle to ");
              Serial.print(config_data.BUCK_PWM_SETPOINT);
              Serial.print("/4096");
              Serial.println();
            }
            if(inchar == 'F'){
              //PWM Frequency
              serial_input= Serial.parseFloat();
              config_data.BUCK_PWM_FREQ = serial_input;
              ledcSetup(BUCK_PWM_CHANNEL, config_data.BUCK_PWM_FREQ, 12);
              ledcWrite(BUCK_PWM_CHANNEL, config_data.BUCK_PWM_SETPOINT);
              Serial.print("Set HV pwm frequency to: ");
              Serial.print(config_data.BUCK_PWM_FREQ);
              Serial.println();
            }
            break;

          case 'S':
            inchar = Serial.read();
            if(inchar == 'A'){
              serial_input= Serial.parseFloat();
              if (serial_input == 1){
                Serial.println("Saving config data to eeprom!");
                saveConfig(config_data);
              }
              //Save all values to eeprom
              
            }
            break;

          case 'R':
            inchar = Serial.read();
            if(inchar == 'E'){
              //Reset device if 1
              serial_input= Serial.parseFloat();
              if (serial_input == 1){
                Serial.println("Restarting ESP now!");
                esp_restart();
              }
            }
            if(inchar == 'F'){
              serial_input= Serial.parseFloat();
              if (serial_input == 1111){
                //Factory reset except credentials
                Serial.println("NOT YET IMPLEMENTED");
              }
              if (serial_input == 9999){
                //Full factory reset
                Serial.println("FULL FACTORY RESET, ERASING EEPROM");
                erase_eeprom();
                Serial.println("WRITING BACKUP DATA");
                saveConfig(config_data_bcp);
                Serial.println("RESTARTING");
                esp_restart();
              }
            }
            break;
          
          case 'B':
            inchar = Serial.read();
            if(inchar == 'E'){
              serial_input= Serial.parseFloat();
              config_data.CLICKER_ACTIVATE = serial_input;
              if(config_data.CLICKER_ACTIVATE){
                Serial.println("BEEPER ACTIVATED ");
              }
              else{
                Serial.println("BEEPER DEACTIVATED ");
              }
            }
            if(inchar == 'D'){
              //Duty cycle for clicker
              serial_input= Serial.parseFloat();
              config_data.CLICKER_DUTYCYCLE = serial_input;
              Serial.print("Clicker duty cycle set to: ");
              Serial.print(config_data.CLICKER_DUTYCYCLE);
              Serial.print("/255");
              Serial.println();
              
            }
            if(inchar == 'F'){
              serial_input= Serial.parseFloat();
              config_data.CLICKER_FREQ = serial_input;
              ledcWrite(CLICKER_PWM_CHANNEL, 0);
              ledcSetup(CLICKER_PWM_CHANNEL, config_data.CLICKER_FREQ, 8);
              Serial.print("Clicker frequency set to: ");
              Serial.print(config_data.CLICKER_FREQ);
              Serial.print(" Hz");
              Serial.println();
            }
            break;

          case 'L':
            inchar = Serial.read();
            if(inchar == 'E'){
              //Activate click indication leds
              serial_input= Serial.parseFloat();
              config_data.LED_ACTIVATE = serial_input;
              if(config_data.LED_ACTIVATE){
                Serial.println("LED ACTIVATED ");
              }
              else{
                Serial.println("LED DEACTIVATED ");
              }
            }
            if(inchar == 'F'){
              //How long to flash and click
              serial_input= Serial.parseFloat();
              config_data.INDICATION_LENGTH = serial_input;
              Serial.print("Indication duration set to: ");
              Serial.print(config_data.INDICATION_LENGTH);
              Serial.print(" mS");
              Serial.println();
            }
            if(inchar == 'O'){
              //LOG OUTPUT BOOL
              serial_input= Serial.parseFloat();
              config_data.LOG_OUTPUT = serial_input;
              if(config_data.LOG_OUTPUT){
                Serial.println("LOGGING ACTIVATED ");
              }
              else{
                Serial.println("LOGGING DEACTIVATED ");
              }
            }
            if(inchar == 'S'){
              //LOG OUTPUT WAITTIME
              serial_input= Serial.parseFloat();
              config_data.LOG_SPEED = serial_input;
              Serial.print("Log interval set to: ");
              Serial.print(config_data.LOG_SPEED);
              Serial.print(" mS");
              Serial.println();
            }
            break;

          case 'E':
            inchar = Serial.read();
            if(inchar == 'V'){
              //Number of events per indication
              serial_input= Serial.parseFloat();
              config_data.EVENT_DELAY = serial_input;
              Serial.print("Counter now needs  ");
              Serial.print(config_data.EVENT_DELAY);
              Serial.print(" events to generate click indication");
              Serial.println();
            }
            break;
      }
  }
}

// upload counts to web server
void upload(int cpm_) {
  const char *cmdFormat = "http://radmon.org/radmon.php?function=submit&user=%s&password=%s&value=%d&unit=CPM";
  char url[256];

  WiFiClient client;
  HTTPClient http;

  // create the request URL
  sprintf(url, cmdFormat, config_data.RADMON_USER, 
                          config_data.RADMON_PASS, 
                          cpm_
  );
  
  Serial.print("[HTTP] begin: ");
  Serial.println(url);

  if (http.begin(client, url)) {
    Serial.print("[HTTP] GET...");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // HTTP header has been sent and server response header has been handled
    Serial.printf(" code: %d\n", httpCode);
    
    // httpCode will be negative on error
    if (httpCode > 0) {
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        Serial.println(payload);
      }
    }
    else {
      Serial.printf("[HTTP] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void counter_loop(){
  now = millis();
  if(now-last > 1000 ){ //do this every second
    last = now;
    //Do this every minute
    if (BUF_CLICK == buf_click_index){
      buf_click_index = 0;
      buf_click_hour[buf_click_hour_index++] = clicks;
      if(strlen(config_data.RADMON_PASS) != 0){
        upload(array_minmax_avg(buf_click,BUF_CLICK));
      }
    }
    //Do this every hour
    if (BUF_CLICK_HOUR == buf_click_hour_index){
      buf_click_hour_index = 0;
    }

    buf_click[buf_click_index++] = clicks;
  }
}

void log_loop(){
  now_2 = millis();
  if(now_2-last_2 > config_data.LOG_SPEED && config_data.LOG_OUTPUT){ //do this every 100ms
    last_2 = now_2;
    buf_volt_avg = array_calculate_avg(buf_volt, BUF_VOLT);
    float buf_cpm_avg_min = array_minmax_avg(buf_click, BUF_CLICK);
    float buf_cpm_avg_hour = array_minmax_avg(buf_click_hour, BUF_CLICK)/60.;
    Serial.print("raw_volt:");
    Serial.print( analogRead(REF_PIN));
    Serial.print(" set_volt:");
    Serial.print( config_data.BUCK_PWM_SETPOINT);
    Serial.print(" cpmM:");
    Serial.print(buf_cpm_avg_min);
    Serial.print(" cpmH:");
    Serial.print(buf_cpm_avg_hour);
    Serial.print(" cpm:");
    Serial.print( float(60/float(ticks_dt/1e6)));
    Serial.println();
  }
}


void clicker_logic(){
  if(clicks > (click_old+config_data.EVENT_DELAY)){
    duration = millis();
    click_old=clicks;
    if (config_data.CLICKER_ACTIVATE){
      ledcWrite(CLICKER_PWM_CHANNEL, config_data.CLICKER_DUTYCYCLE);
    }
    if (config_data.LED_ACTIVATE){
      digitalWrite(LED_0, LOW);
      digitalWrite(LED_1, LOW);
      digitalWrite(LED_2, LOW);
      digitalWrite(LED_3, LOW);
      digitalWrite(LED_4, LOW);
    }
    }
  else {
    if(millis()-duration > config_data.INDICATION_LENGTH){
      ledcWrite(CLICKER_PWM_CHANNEL, 0);
      digitalWrite(LED_0, HIGH);
      digitalWrite(LED_1, HIGH);
      digitalWrite(LED_2, HIGH);
      digitalWrite(LED_3, HIGH);
      digitalWrite(LED_4, HIGH);
    }
  }
}

void led_rise(int waittime){
    digitalWrite(LED_0, LOW);
    delay(waittime);
    digitalWrite(LED_0, HIGH);
    digitalWrite(LED_1, LOW);
    delay(waittime);
    digitalWrite(LED_1, HIGH);
    digitalWrite(LED_2, LOW);
    delay(waittime);
    digitalWrite(LED_2, HIGH);
    digitalWrite(LED_3, LOW);
    delay(waittime);
    digitalWrite(LED_3, HIGH);
    digitalWrite(LED_4, LOW);
    delay(waittime);
    digitalWrite(LED_4, HIGH);
    delay(waittime);
}

void check_and_reconnect(){
  unsigned long currentMillis_wifi = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis_wifi - previousMillis_wifi >= interval_wifi)) {
    Serial.println("Reconnecting to WiFi...");
    led_rise(30);
    led_rise(30);
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis_wifi = currentMillis_wifi;
  }
}

void setup() {
  Serial.begin(115200);

  //Activate eeprom and check if it is programmed
  EEPROM.begin(EEPROM_SIZE);
  eepromCheck();

  //ADC Setup, remove the 1b of noise
  analogSetWidth(11);
  analogSetAttenuation(ADC_11db);

  pinMode (PULSEPIN, INPUT);
  pinMode (BOOTBUTTON, INPUT);

  //Only needed for REV0.2.0
  pinMode (25, INPUT);

  pinMode (LED_0, OUTPUT);
  pinMode (LED_1, OUTPUT);
  pinMode (LED_2, OUTPUT);
  pinMode (LED_3, OUTPUT);
  pinMode (LED_4, OUTPUT);
  
  //Set leds high to not light up
  digitalWrite(LED_0, HIGH);
  digitalWrite(LED_1, HIGH);
  digitalWrite(LED_2, HIGH);
  digitalWrite(LED_3, HIGH);
  digitalWrite(LED_4, HIGH);
  
  //Geiger Pulse Interrupt
  attachInterrupt(  PULSEPIN, ISR, FALLING);
  attachInterrupt(BOOTBUTTON, RESET_INT, FALLING);
  //Attatch output pins to pwm channels
  ledcAttachPin(BUCK_PIN, BUCK_PWM_CHANNEL);
  ledcAttachPin(CLICKER, CLICKER_PWM_CHANNEL);
  
  //Configure pwm channels resolution, buck is 12 bit (4096) clicker is 8 bit (255)
  ledcSetup(BUCK_PWM_CHANNEL, config_data.BUCK_PWM_FREQ, 12);
  ledcSetup(CLICKER_PWM_CHANNEL, config_data.CLICKER_FREQ, 8);
  ledcWrite(BUCK_PWM_CHANNEL, config_data.BUCK_PWM_SETPOINT);
  
  if (strlen(config_data.WIFI_SSID) != 0){

    WiFi.mode(WIFI_STA); //Optional
    WiFi.begin(config_data.WIFI_SSID, config_data.WIFI_PASS);
    Serial.println("\nConnecting");
    int retries = 0;

    while((WiFi.status() != WL_CONNECTED) && (retries < 60)){
        Serial.print(".");
        led_rise(30);
        retries++; 
    }
    if ((WiFi.status() == WL_CONNECTED)){
      Serial.println("Connected to WIFI");
      Serial.print("Local IP: ");
      Serial.println(WiFi.localIP());
      wifi_conn = WiFi.status() == WL_CONNECTED;
      start_main_page();
      start_response_server();
      server.begin();
    }
  }

  if(wifi_conn == false){
    Serial.print("NETWORK ERROR");
    ap_init();
  }
}

void loop() {
  unsigned long start = micros();
  
  //Clicker logic
  clicker_logic();

  //Serial Output
  serial_handler();

  //COUNTER LOOP
  counter_loop();
  
  //SERIAL LOOP
  log_loop();

  //Server Handler
  if ( wifi_conn == false){
    dnsServer.processNextRequest();
  }
  else{
    check_and_reconnect();
  }
}
