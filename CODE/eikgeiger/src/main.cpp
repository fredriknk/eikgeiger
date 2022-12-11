#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <EEPROM.h>

#define BUF_VOLT 100
#define BUF_VOLT_LONG 200
#define BUF_CLICK 60
#define BUF_CLICK_HOUR 60

int   buf_volt[BUF_VOLT]   = {0};
int   buf_volt_index       = 0;
float buf_volt_avg         = 0.0;

unsigned long buf_click[BUF_CLICK]  = {0};
int           buf_click_index       =  0;
float         buf_click_avg         =  0.0;

unsigned long buf_click_hour[BUF_CLICK]  = {0};
int           buf_click_hour_index       =  0;
float         buf_click_hour_avg         =  0.0;

uint setpoint=340;



uint32_t Freq;
int LED_0= 0; 
int LED_1= 2;
int LED_2= 13;
int LED_3= 15;
int LED_4= 4;
int CLICKER = 32; //Speaker element
const int REFpin = 39; //ADC Input
const int Pulsepin = 23; //Geiger pulse input
float cpm = 0;
int BUCK_PIN= 12; 

volatile unsigned long clicks = 0;
volatile unsigned long click_old=0;
volatile unsigned long ticks = micros();
volatile unsigned long ticks_old = ticks-10;
volatile unsigned long ticks_dt = 0;

unsigned long duration = millis();

int TIMEDELAY= 10;
int pwm_int = 340;
int potValue = 0;

unsigned long first = millis();
unsigned long last = first+1;
unsigned long now = last+1;

unsigned long last_2 = now+1;
unsigned long now_2 = last_2+1;
int BUCK_PWM = 0;
int CLICKER_PWM = 12;
bool click_sound = false;

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


struct data_layout
{
  char id[16];
  char ssid[128];
  char pwd[128];
  char ip[128];
  bool first_run;
  int number1;
  byte number2;
  byte number3;
  byte number4;
  byte number5;

};

data_layout persistant_data;
data_layout temp_data;

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
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      //Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
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
    response->print("<!DOCTYPE html><html><head><title>EIKGEIGER WIFI LOGIN</title></head><body>");
    response->print( "<h1>EIKGEIGER</h1>");
    response->printf("<p>Geigertelleren er aktiv, den er innstilt til %i </p>",setpoint);
    response->print( "<p>Hvis du vil endre spenning brukes seriellport gjennom PC.</p>");
    response->print( "<p>Bruk kommando s### , der ### er et heltall mellom 0-2000.</p>");
    response->printf("<p>Gjennomsnittlig CPM siste minutt er  %.2f klikk per minutt</p>", array_minmax_avg(buf_click, BUF_CLICK));
    response->print( "<p>Vennligst velg hvilket wifi du vil koble deg til, og skriv inn passord</p>");
    response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
    response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
    response->print( st );
    response->print( "<form action='/' method='POST'>"
                     "<p><label for='ssid'>SSID, write name or number for found wifi</label>"
                     "<input type='text' id ='ssid' name='ssid'>"

                     "<br><label for='pass'>Password \t\t</label>"
                     "<input type='text' id ='pass' name='pass'>"

                     "<br><label for='ip'>IP Address (Leave blank for DHCP) </label>"
                     "<input type='text' id ='ip' name='ip' value=''>"

                     "<br><label for='gateway'>Gateway Address (leave blank for auto)</label>"
                     "<input type='text' id ='gateway' name='gateway' value=''><br>"

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

  analogSetWidth(11);
  analogSetAttenuation(ADC_11db);

  pinMode (Pulsepin, INPUT);
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

  ledcAttachPin(BUCK_PIN, BUCK_PWM);
  ledcAttachPin(CLICKER, CLICKER_PWM);

  attachInterrupt(Pulsepin, ISR, FALLING);

  ledcSetup(BUCK_PWM, 1000, 12);

  ledcSetup(CLICKER_PWM, 6000, 8);
  
  //ledcWrite(CLICKER_PWM, 0);
  ledcWrite(BUCK_PWM, setpoint);


  ap_init();
}

void loop() {
  unsigned long start = micros();
  
  //Clicker logic

  if(clicks > click_old){
    duration = millis();
    //cpm = float(60e6/ticks_dt);
    
    click_old=clicks;
    if (click_sound){ledcWrite(CLICKER_PWM, 80);}
    digitalWrite(LED_0, LOW);
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_4, LOW);
  }
  else {
    if(millis()-duration > 3){
      if (click_sound){ledcWrite(CLICKER_PWM, 0);};
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
        case 'v':
          serial_input= Serial.parseFloat();
          setpoint=serial_input;
          ledcWrite(BUCK_PWM, setpoint);
          break;
        case 's':

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
  if(now_2-last_2 > 1000 ){ //do this every 100ms
    last_2 = now_2;

    buf_volt_avg = array_calculate_avg(buf_volt, BUF_VOLT);
    float buf_cpm_avg_min = array_minmax_avg(buf_click, BUF_CLICK);
    float buf_cpm_avg_hour = array_minmax_avg(buf_click_hour, BUF_CLICK)/60.;
    Serial.print("raw_volt:");
    Serial.print( analogRead(REFpin));
    Serial.print(" set_volt:");
    Serial.print( setpoint);
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