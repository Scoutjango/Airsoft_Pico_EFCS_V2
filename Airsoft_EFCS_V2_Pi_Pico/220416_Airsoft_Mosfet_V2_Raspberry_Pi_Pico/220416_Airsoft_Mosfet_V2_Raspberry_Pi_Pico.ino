#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

//Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3C = 128x32; 0x3D = 128x64;  // we use 128x64 but have to use 128x32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Pins
const int TRIGGER = 16;
const int IRLED = 28; //LOW=ON
const int LIGHTGATE = 22;
const int MOSFET = 19;
const int FIREMODESWITCH = 18;
const int POTI = 27;
const int BATTERYVOLTAGE = 26;
const int MAGAZINE = 17;

//const
const int IRLEDOFFDELAY = 300; //Delay in ms
const int FIREMODEDELAY = 300; //Delay in ms
const int SINGLESHOTDELAY = 1; //Delay in ms
const int MINBURST = 2;
const int MAXBURST = 4;
const int MINAUTODELAY = 0;
const int MAXAUTODELAY = 84;
const float R1 = 100200.0;    //R1 for Battery 
const float R2 = 47700.0;      //R2 for Battery
const float MINV = 6.0;       //Minimum Volltage for Battery
const int POTIMIN = 0;       //Minimum Value of Poti
const int POTIMAX = 1023;     //Max Value of Poti
const int IRLEDRESPONSETIME = 1; //Time to wait for LED ON

//Global
unsigned long triggerStartTime;
bool active = false;
int fireMode = 0;
int shotCounter = 0;
int bursted = 0;
bool release = false;
int displayCounter = 0;
int displayVoltage = 0;
int displayFireMode = 0;
float volt = 0.0;
bool resetOnce = false;

void updateDisplay(int counter,int mode,float voltage){
  displayCounter = counter;
  displayFireMode = mode;
  displayVoltage = voltage;
  display.setTextSize(5);
  display.setTextColor(SSD1306_WHITE);
  display.fillRect(0,0,display.width()-1,display.height()-1,SSD1306_BLACK);
  display.setCursor(20,0);
  display.println(counter);
  display.setTextSize(2);
  display.setCursor(0,50);
  switch(mode){
    case 0: display.println("-");break;
    case 1: display.println("- -");break;
    case 2: display.println("- - -");break;
    default : display.println("ERROR"); break;
  }
  display.setCursor(80,50);
  display.println(((float)((int)(voltage*10)))*0.1);
  display.display();
}

void reset(){
  bursted = 0;
  active = false;
  digitalWrite(MOSFET,LOW);
  digitalWrite(IRLED,LOW);
}

int getPoti(int min, int max){
  int value = analogRead(POTI);
  value =map(value,POTIMIN,POTIMAX,min,max);
  return abs(value);
}

float checkBattery(){
  int valueVoltmeter = analogRead(BATTERYVOLTAGE);
  float vOut = (valueVoltmeter*3.3)/1024.0;
  return (vOut/(R2/(R1+R2)));
}

void singleShot(){
  while (digitalRead(TRIGGER)){
    ;
  }
  triggerStartTime = millis();
  delay(SINGLESHOTDELAY);
}

void releaseShot(){
  singleShot();
  if(release){
    active = true;
    digitalWrite(MOSFET,HIGH);
    release = false;
  }
}

void burstShot(){
  int burstShotCount = getPoti(MINBURST,MAXBURST);
  if(burstShotCount == 2){
    releaseShot();
  }else{
    bursted++;
    if(bursted>=burstShotCount){
      bursted = 0;
      singleShot();
    }else{
      active = true;
      triggerStartTime = millis();
      digitalWrite(MOSFET,HIGH);
    }
  }
}

void autoShot(){
  delay(getPoti(MINAUTODELAY,MAXAUTODELAY));
}

void checkFireMode(){
  digitalWrite(MOSFET,LOW);
  active = false;
  unsigned long testtime = millis();
  if(displayCounter!=shotCounter||displayFireMode!=fireMode||displayVoltage!=volt){
    updateDisplay(shotCounter,fireMode,volt);
  }
  unsigned long tTime = millis();
  Serial.println(tTime-testtime);
  switch (fireMode){
  case 0: singleShot(); break;
  case 1: burstShot(); break;
  case 2: autoShot(); break;
  default: 
    reset();
    fireMode = 0;
    break;
  }
}

void checkShot(){
  delay(IRLEDRESPONSETIME);
  while (digitalRead(LIGHTGATE) && active){ 
    if(!digitalRead(TRIGGER)){
      if(millis()-triggerStartTime>IRLEDOFFDELAY){
        reset();
      }
    }
  }
  if(active){
    shotCounter++;
    checkFireMode();
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(IRLED,OUTPUT);
  pinMode(MOSFET,OUTPUT);
  pinMode(TRIGGER,INPUT);
  pinMode(LIGHTGATE,INPUT);
  pinMode(FIREMODESWITCH,INPUT);
  pinMode(POTI,INPUT);
  pinMode(BATTERYVOLTAGE,INPUT);
  pinMode(MAGAZINE,INPUT);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();
  volt = checkBattery();
}

void loop() {
  // put your main code here, to run repeatedly:
  if(digitalRead(TRIGGER)){
    triggerStartTime = millis();
    digitalWrite(IRLED,HIGH);
    digitalWrite(MOSFET,HIGH);
    active = true;
    release = true;
  }else{
    if(millis()-triggerStartTime>IRLEDOFFDELAY){
      reset();
    }
  }
  if(active){
    checkShot();
  }
  if(digitalRead(FIREMODESWITCH)){
    if(fireMode<2){
      fireMode++;
    }else{
      fireMode = 0;
    }
    delay(FIREMODEDELAY);
  }
  if(digitalRead(MAGAZINE)){
    resetOnce = true;
  }
  if(!digitalRead(MAGAZINE) && resetOnce){
    resetOnce = false;
    shotCounter = 0;
    volt = checkBattery();
    reset();
  }
  if(checkBattery()<MINV){
    display.clearDisplay();
    display.setTextSize(7);
    display.setCursor(0,0);
    display.println("LOW");
    display.display();
  }else if (displayCounter!=shotCounter||displayFireMode!=fireMode||((int)(displayVoltage*10)) != ((int)(volt*10))){
    volt = checkBattery();
    updateDisplay(shotCounter,fireMode,volt);
  }
}
