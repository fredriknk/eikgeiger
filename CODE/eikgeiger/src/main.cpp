#include <Arduino.h>

#define BUF_VOLT 30
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

unsigned long runtime_ = 0;
unsigned long runtime = 0;

//PID variables:
double I=0, I_=0, p=0, p_=0, setpoint=0, input=0, dp=0, output=0;


uint32_t Freq;
int LED_0= 0; 
int LED_1= 2;
int LED_2= 13;
int LED_3= 15;
int LED_4= 4;
int CLICKER = 35; //Speaker element
const int REFpin = 25; //ADC Input
const int Pulsepin = 23; //Geiger pulse input
float cpm = 0;
int BUCK_PIN= 12; 

volatile unsigned long clicks = 0;
volatile unsigned long click_old=0;
volatile unsigned long ticks = micros();
volatile unsigned long ticks_old = ticks-10;
volatile unsigned long ticks_dt = 0;


int TIMEDELAY= 10;
int pwm_int = 340;
int potValue = 0;

unsigned long first = millis();
unsigned long last = first+1;
unsigned long now = last+1;

unsigned long last_2 = now+1;
unsigned long now_2 = last_2+1;

unsigned long last_3 = micros();
unsigned long now_3 = last_3+1;

int BUCK_PWM = 0;
int CLICKER_PWM = 12;


void IRAM_ATTR ISR() {
    ticks_old = ticks;
    ticks = micros();
    clicks++;
    ticks_dt = ticks-ticks_old;
}

void setup() {
  Serial.begin(115200);

  analogSetWidth(11);
  analogSetAttenuation(ADC_11db);

  pinMode (Pulsepin, INPUT);
  
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

  //ledcSetup(CLICKER_PWM, 4000, 8);
  ledcSetup(BUCK_PWM, 1000, 12);

  ledcSetup(CLICKER_PWM, 6000, 8);
  
  ledcWrite(CLICKER_PWM, 0);
  ledcWrite(BUCK_PWM, pwm_int);
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

void printbuf(unsigned int *buf, int len){
    for (int i = 0; i < len; i++){
        Serial.print(buf[i]);
        Serial.print("\t");
    }
    Serial.println();
}

void loop() {
  unsigned long start = micros();

  if(clicks > click_old){
    //cpm = float(60e6/ticks_dt);
    click_old=clicks;
    ledcWrite(CLICKER_PWM, 60);
    digitalWrite(LED_0, LOW);
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_4, LOW);
  }
  else {
    ledcWrite(CLICKER_PWM, 0);
    digitalWrite(LED_0, HIGH);
    digitalWrite(LED_1, HIGH);
    digitalWrite(LED_2, HIGH);
    digitalWrite(LED_3, HIGH);
    digitalWrite(LED_4, HIGH);
  }

  if (Serial.available()!=0){
    pwm_int=Serial.parseInt();
    Serial.parseInt();
  }

  potValue = analogRead(REFpin);
  if (BUF_VOLT == buf_volt_index){
      buf_volt_index = 0;
  }
  buf_volt[buf_volt_index++] = potValue;

  now = millis();
  if(millis()-last > 1000 ){ //do this every second
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

  now_2 = millis();
  if(millis()-last_2 > 10 ){ //Do this every 100 ms
    last_2 = now_2;
    buf_volt_avg = array_calculate_avg(buf_volt, BUF_VOLT);
    float buf_cpm_avg_min = array_minmax_avg(buf_click, BUF_CLICK);
    float buf_cpm_avg_hour = array_minmax_avg(buf_click_hour, BUF_CLICK)/60.;
    /*Serial.print(voltdivider(adc2volt(buf_volt_avg),50100,100));
    Serial.print("\t");
    Serial.print(float((60*1e6)/ticks_dt),2);
    Serial.print("\t");
    Serial.print(buf_cpm_avg_hour,2);
    Serial.print("\t");
    Serial.print(buf_cpm_avg_min,2);
    Serial.print("\t");
    Serial.print(buf_cpm_avg_min*1000./(100*1320.),6);// usivert/hour
    Serial.print("\t");
    Serial.print(pwm_int);
    Serial.print("\t");
    Serial.print(clicks);
    Serial.print("\t");
    Serial.print(float(clicks/float((now-first)/60000.)));
    Serial.print("\t");
    Serial.print(float((now-first)/60000.));
    Serial.print("\t");
    Serial.print(ticks_dt%8);
    Serial.print("\t");
    Serial.print(runtime_);
    Serial.print("\t");
    Serial.print(runtime);
    Serial.print("\t");
    Serial.print(micros()-start);
    Serial.println();*/
    Serial.print(input);
    Serial.print("\t");
    Serial.print(p);
    Serial.print("\t");
    Serial.print(I);
    Serial.print("\t");
    Serial.print(dp);
    Serial.print("\t");
    Serial.print(output);
    Serial.println();
  }

  //PID LOOP
  now_3 = micros();

  if(now_3-last_3 > 2000 ){ //Do this every 2 ms
    input = array_calculate_avg(buf_volt, BUF_VOLT);
    setpoint = double(pwm_int);

    float dt = now_3-last_3;
    last_3 = now_3;
    double Kp = 0., Ki = 0.0, Kd = 0.;

    p_ = p;
    
    p=setpoint-input;

    dp = double((p-p_)/dt);
    
    I_ = I;
    I = I + dt*((p+p_)/2.);
    
    output = p*Kp+I*Ki+dp*Kd;

    ledcWrite(BUCK_PWM, 0);
  }
  
  runtime_ = runtime;
  runtime =  micros()-start;
}