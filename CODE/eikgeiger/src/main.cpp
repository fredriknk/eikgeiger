#include <Arduino.h>

uint32_t Freq;
int LED_0= 0;
int LED_1= 2;
int LED_2= 13;
int LED_3= 15;
int LED_4= 4;
int CLICKER = 35;
const int REFpin = 25;
const int Pulsepin = 23;

int BUCK_PIN= 12; 


uint32_t clicks = 0;
uint32_t click_old=0;


int TIMEDELAY= 10;
int pwm_int = 0;
int potValue = 0;


int BUCK_PWM = 0;
int CLICKER_PWM = 12;

void IRAM_ATTR ISR() {
    clicks++;
}

void setup() {
  Serial.begin(115200);

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
  ledcSetup(BUCK_PWM, 1000, 11);

  ledcSetup(CLICKER_PWM, 6000, 8);
  
  ledcWrite(CLICKER_PWM, 0);
  ledcWrite(BUCK_PWM, 250);
}

void loop() {
  /*
  Serial.println("Input pwm max 8192");        //Prompt User for input
  while (Serial.available()==0){}             // wait for user input
    pwm_int = Serial.parseInt();                    //Read user input and hold it in a variable
 
  // Print well formatted output
  Serial.print("YOU SET PPWM ");                 
  Serial.print(pwm_int);
  //ledcSetup(BUCK_PWM, pwm_int, 2);
  ledcWrite(BUCK_PWM, pwm_int);

  while (Serial.available()==0){}             // wait for user input
    pwm_int=Serial.parseInt();                    //Read user input and hold it in a variable

  
  */
  delay(10);
  if(clicks > click_old){
    click_old=clicks;
    ledcWrite(CLICKER_PWM, 60);
    digitalWrite(LED_0, LOW);
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_4, LOW);
    delay(10);
    digitalWrite(LED_0, HIGH);
    digitalWrite(LED_1, HIGH);
    digitalWrite(LED_2, HIGH);
    digitalWrite(LED_3, HIGH);
    digitalWrite(LED_4, HIGH);
    ledcWrite(CLICKER_PWM, 0);
  }

  potValue = analogRead(REFpin);
  Serial.println(potValue);
  if (Serial.available()!=0){ 
    pwm_int=Serial.parseInt();
    ledcWrite(BUCK_PWM, pwm_int);
    Serial.parseInt();
  }

}