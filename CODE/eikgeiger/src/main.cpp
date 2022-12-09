#include <Arduino.h>
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

bool ss_flag = false;
bool enable_psup = true;

unsigned long runtime_ = 0;
unsigned long runtime = 0;

//PID variables:
double I=0, I_=0, p=0, p_=0, setpoint=0, input=0, dp=0, output=0;
double Kp = 1., Ki = 2.0, Kd = 0.00;
double pid = 0;

//Filter:
float x[] = {0,0,0};
float y[] = {0,0,0};
int raw_read = 0;

uint32_t Freq;
int LED_0= 0; 
int LED_1= 2;
int LED_2= 13;
int LED_3= 15;
int LED_4= 4;
int CLICKER = 32; //Speaker element
const int REFpin = 25; //ADC Input
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

unsigned long last_3 = micros();
unsigned long now_3 = last_3+1;

unsigned long last_4 = micros();
unsigned long now_4 = last_4+1;

int BUCK_PWM = 0;
int CLICKER_PWM = 12;


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


void setup() {
  Serial.begin(115200);

  setpoint = revsetpoint(370);

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

  ledcSetup(BUCK_PWM, 1000, 12);

  ledcSetup(CLICKER_PWM, 6000, 8);
  
  //ledcWrite(CLICKER_PWM, 0);
  ledcWrite(BUCK_PWM, pwm_int);

  //Deep sleep
  esp_sleep_enable_timer_wakeup(5 * 1e6);
}

void loop() {
  delay(1);
  unsigned long start = micros();
  
  //Clicker logic

  if(clicks > click_old){
    duration = millis();
    //cpm = float(60e6/ticks_dt);
    click_old=clicks;
    ledcWrite(CLICKER_PWM, 10);
    digitalWrite(LED_0, LOW);
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_4, LOW);
  }
  else {
    if(millis()-duration > 3){
      ledcWrite(CLICKER_PWM, 0);
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
        case 'p':
          serial_input= Serial.parseFloat();
          Kp=serial_input;
          break;
        case 'i':
          serial_input= Serial.parseFloat();
          I=0;
          Ki=serial_input;
          break;
        case 'd':
          serial_input= Serial.parseFloat();
          Kd=serial_input;
          break;
        case 's':
          serial_input= Serial.parseFloat();
          setpoint=revsetpoint(serial_input);
          break;
        case 'r':
          Ki=0;
          Kd=0;
          Kp=0;
          setpoint=400.0;
          break;
        case 'k':
          Serial.print("Kp:");
          Serial.print(Kp);
          Serial.print(" Ki:");
          Serial.print(Ki);
          Serial.print(" Kd:");
          Serial.print(Kd);
          Serial.print(" setpoint:");
          Serial.print(setpoint);
          Serial.println();
          break;
        case 'o':
          output = 0;
          I=0;
          ledcWrite(BUCK_PWM,0);
          enable_psup = Serial.parseInt();
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
  now_4 = millis();
  if(now_4-last_4 > 1000 ){ //do this every second
    last_4 = now_4;

    buf_volt_avg = array_calculate_avg(buf_volt, BUF_VOLT);
    float buf_cpm_avg_min = array_minmax_avg(buf_click, BUF_CLICK);
    float buf_cpm_avg_hour = array_minmax_avg(buf_click_hour, BUF_CLICK)/60.;
    Serial.print("raw_volt:");
    Serial.print( voltdivider(adc2volt(analogRead(REFpin)),50000,100));
    Serial.print(" volt:");
    Serial.print( voltdivider(adc2volt(input),50000,100));
    Serial.print(" set_volt:");
    Serial.print( voltdivider(adc2volt(setpoint),50000,100));
    Serial.print(" output");
    Serial.print(output);
    Serial.print(" cpmM:");
    Serial.print(buf_cpm_avg_min);
    Serial.print(" cpmH:");
    Serial.print(buf_cpm_avg_hour);
    Serial.print(" cpm:");
    Serial.print( float(60/float(ticks_dt/1e6)));
    Serial.println();
  }
  
  //ADC LOOP
  now_2 = micros();
  if(now_2-last_2 > 1000 && enable_psup){ //Do this every 10 000 us
    last_2 = now_2;
    raw_read = analogRead(REFpin);
    
    x[0] = raw_read;

    float b[] = {0.06396438485558797, 0.12792876971117606, 0.06396438485558781};
    float a[] = {1.1682606671932643, -0.4241182066156163};

    y[0] = a[0]*y[1] + a[1]*y[2] +
           b[0]*x[0] + b[1]*x[1] + b[2]*x[2];
    
    for(int i = 1; i >= 0; i--){
      x[i+1] = x[i]; // store xi
      y[i+1] = y[i]; // store yi
    }

    if (BUF_VOLT == buf_volt_index){
      buf_volt_index = 0;
    }

    buf_volt[buf_volt_index++] = y[0];
  }
  
  //PID LOOP
  now_3 = micros();
  if(now_3-last_3 > 100000 && enable_psup){ //Do this every 10 ms

    double dt = (now_3-last_3)/1e6;
    last_3 = now_3;

    input = array_calculate_avg(buf_volt, BUF_VOLT);
    p_ = p;
    
    p=setpoint-input;

    dp = double((p-p_)/dt);

    I_= I;
    I = I + dt*((p+p_)/2.);

    if ( I < 0){
      I = I_;
    }

    if ( abs(I)*Ki > 3000){
      I = I_;
    }
    double output_ = output;
    output = p*Kp+I*Ki+dp*Kd;

    if ( output < 0){
      output = 0;
    }

    if ( output > 1500){
      output = 1500;
      ss_flag = true;
    }

    else{
      ss_flag = false;
    }

    ledcWrite(BUCK_PWM, int(output));
  }
  
  runtime_ = runtime;
  runtime =  micros()-start;
}