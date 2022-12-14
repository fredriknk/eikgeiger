#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
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
#define CLICKER_PWM_CHANNEL 12
#define BUCK_PWM_CHANNEL 0
#define CLICKER 32 //Speaker element
#define REF_PIN 39 //ADC Input
#define PULSEPIN 23 //Geiger pulse input
#define FIRST_RUN_NUMBER 1065098293


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

unsigned long duration = millis();

unsigned long first = millis();
unsigned long last = first+1;
unsigned long now = last+1;

unsigned long last_2 = now+1;
unsigned long now_2 = last_2+1;



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


typedef struct 
{ 
  //128bit random ID
  char ssid[23]={};
  //Stored ssid name up to 128 chars
  char WIFI_SSID[128]={};
  //Stored password up to 128 chars
  char WIFI_PASS[128]={};
  //Stored IP
  byte ip[4] ={0,0,0,0};
  //Stupid check to see if eeprom is set
  unsigned long FIRST_RUN =  FIRST_RUN_NUMBER;
  //HV PWM DUTY CYCLE config_data.BUCK_PWM_SETPOINT/4095
  uint BUCK_PWM_SETPOINT=350;
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
    st += " (";
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
    response->printf("<p>Geigertelleren er aktiv, den er innstilt til %i </p>",config_data.BUCK_PWM_SETPOINT);
    response->print( "<p>Hvis du vil endre spenning brukes seriellport gjennom usb med buadrate på 115200.</p>");
    response->print( "<p>Bruk kommando VO### , der ### er et heltall mellom 0-2000. Start lavt og gå sakte oppover til du får CPM verdi fra geigeren</p>");
    response->printf("<p>Gjennomsnittlig CPM siste minutt er  %.2f klikk per minutt</p>", array_minmax_avg(buf_click, BUF_CLICK));
    response->print( "<p>Vennligst velg hvilket wifi du vil koble deg til, og skriv inn passord</p>");
    response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
    response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
    response->print( st );
    response->print( "<form action='/' method='POST'>"
                     "<p><label for='ssid'>SSID, write name or number for found wifi</label><br>"
                     "<input type='text' id ='ssid' name='ssid'><br><br>"

                     "<label for='pass'>Password </label><br>"
                     "<input type='text' id ='pass' name='pass'><br><br>"

                     "<label for='ip'>IP Address (Leave blank for DHCP) </label><br>"
                     "<input type='text' id ='ip' name='ip' value=''><br><br>"

                     "<label for='gateway'>Gateway Address (leave blank for auto)</label><br>"
                     "<input type='text' id ='gateway' name='gateway' value=''><br><br>"

                     "<label for='radmon_username'>Radmon_username, register user at <a href='https://radmon.org/index.php/register'>Radmon.org</a></label><br>"
                     "<input type='text' id ='radmon_username' name='radmon_username' value=''><br><br>"
                    
                     "<label for='radmon_pass'>Radmon device password (NOT login password)</a></label><br>"
                     "<input type='text' id ='radmon_pass' name='radmon_pass' value=''><br><br>"

                     "<input type ='submit' value ='Submit'></p></form>");
    response->print( "</body></html>");
    request->send(response);
  }
};

void ap_init(){
  scanwifi();
  WiFi.softAP("esp-captive");
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP
  //more handlers...
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  eepromCheck();

  //ADC Setup
  analogSetWidth(11);
  analogSetAttenuation(ADC_11db);

  pinMode (PULSEPIN, INPUT);
  pinMode (25, INPUT);
  pinMode (LED_0, OUTPUT);
  pinMode (LED_1, OUTPUT);
  pinMode (LED_2, OUTPUT);
  pinMode (LED_3, OUTPUT);
  pinMode (LED_4, OUTPUT);

  digitalWrite(LED_0, HIGH);
  digitalWrite(LED_1, HIGH);
  digitalWrite(LED_2, HIGH);
  digitalWrite(LED_3, HIGH);
  digitalWrite(LED_4, HIGH);

  ledcAttachPin(BUCK_PIN, BUCK_PWM_CHANNEL);
  ledcAttachPin(CLICKER, CLICKER_PWM_CHANNEL);

  attachInterrupt(PULSEPIN, ISR, FALLING);

  ledcSetup(BUCK_PWM_CHANNEL, config_data.BUCK_PWM_FREQ, 12);

  ledcSetup(CLICKER_PWM_CHANNEL, config_data.CLICKER_FREQ, 8);
  
  ledcWrite(BUCK_PWM_CHANNEL, config_data.BUCK_PWM_SETPOINT);


  ap_init();
}

void loop() {
  unsigned long start = micros();
  
  //Clicker logic
  if(clicks > (click_old+config_data.EVENT_DELAY)){
    duration = millis();
    click_old=clicks;
    if (config_data.CLICKER_ACTIVATE){ledcWrite(CLICKER_PWM_CHANNEL, config_data.CLICKER_DUTYCYCLE);}
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

  if (Serial.available()!=0){
    char inchar = ' ';
    double serial_input;

    inchar = Serial.read();
    switch (inchar){
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
  //COUNTER LOOP
  now = millis();
  if(now-last > 1000 ){ //do this every second
    last = now;
    if (BUF_CLICK == buf_click_index){
      buf_click_index = 0;
      buf_click_hour[buf_click_hour_index++] = clicks;
    }

    if (BUF_CLICK_HOUR == buf_click_hour_index){
      buf_click_hour_index = 0;
    }

    buf_click[buf_click_index++] = clicks;
  }
  
  //SERIAL LOOP
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

  dnsServer.processNextRequest();
}