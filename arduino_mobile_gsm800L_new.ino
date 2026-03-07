/*
Project: DIY GSM Phone using Arduino Nano + SIM800L
Channel: @electronicsideaa  (Electronics Idea)
Board: Arduino Nano (ATmega328P - 16MHz)
Baud Rate: 9600

Connections:
SIM800L TX -> D11
SIM800L RX -> D10 (via voltage divider)
Power: External 4V stable supply (do NOT power from Arduino 5V)

Library Used:
SoftwareSerial.h
Adafruit_GFX.h
Adafruit_SSD1306.h
Note:
Use proper power supply for SIM800L (2A peak current required).

If you modify this code, please credit the channel.
*/


#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ================= OLED ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ================= GSM ================= */
SoftwareSerial sim800(11, 10);   // RX, TX
int signalStrength = 0;

/* ================= BATTERY ================= */
#define BATTERY_PIN A0
int batteryPercent = 0;

/* ================= KEYPAD ================= */
const int keyPins[]   = {2,3,4,5,6,7,8,9,12,13};
const char keyChars[] = {'1','2','3','4','5','6','7','8','9','0'};
bool lastKey[10];
String dialNumber = "";

/* ================= BUTTONS ================= */
#define CALL_BUTTON A2
#define END_BUTTON  A3
#define BACK_BUTTON A1
bool lastCall=HIGH, lastEnd=HIGH, lastBack=HIGH;

/* ================= CALL STATES ================= */
bool dialing=false;
bool inCall=false;
bool incomingCall=false;
String incomingNumber="";

/* ================= CALL TIMER ================= */
unsigned long dialingStart=0;
unsigned long callStartTime=0;
unsigned long callDurationSec=0;

/* ================= SMS ================= */
bool smsView=false;
String smsSender="";
String smsText="";

/* ================= TIMERS ================= */
unsigned long lastSignal=0;
unsigned long lastBattery=0;

/* ================= SETUP ================= */
void setup() {

  sim800.begin(9600);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  for(int i=0;i<10;i++){
    pinMode(keyPins[i], INPUT_PULLUP);
    lastKey[i]=HIGH;
  }

  pinMode(CALL_BUTTON, INPUT_PULLUP);
  pinMode(END_BUTTON, INPUT_PULLUP);
  pinMode(BACK_BUTTON, INPUT_PULLUP);

  display.clearDisplay();
  display.setCursor(20,20);
  display.println("ULTRA MOBILE");
  display.display();

  delay(4000);

  sim800.println("AT+CLIP=1");          // Caller ID
  delay(300);
  sim800.println("AT+CMGF=1");          // SMS text mode
  delay(300);
  sim800.println("AT+CNMI=2,2,0,0,0");  // SMS AUTO PUSH
  delay(300);
}

/* ================= LOOP ================= */
void loop() {

  readGSM();
  readKeypad();
  readButtons();

  if (dialing && !inCall && millis() - dialingStart > 8000) {
    dialing=false;
    inCall=true;
    callStartTime=millis();
  }

  if (inCall)
    callDurationSec = (millis() - callStartTime) / 1000;

  if (!smsView && !inCall && millis() - lastSignal > 4000) {
    lastSignal = millis();
    readSignal();
  }

  if (millis() - lastBattery > 2000) {
    lastBattery = millis();
    readBattery();
  }

  drawUI();
}

/* ================= GSM INPUT ================= */
void readGSM(){

  while(sim800.available()){
    String line = sim800.readStringUntil('\n');
    line.trim();

    /* Incoming call */
    if(line == "RING"){
      incomingCall=true;
    }

    if(line.startsWith("+CLIP:")){
      int q1=line.indexOf('"');
      int q2=line.indexOf('"',q1+1);
      if(q1>=0 && q2>q1)
        incomingNumber=line.substring(q1+1,q2);
    }

    /* ===== SMS AUTO RECEIVE ===== */
    if(line.startsWith("+CMT:")){
      smsSender="";
      smsText="";
      smsView=true;

      int q1=line.indexOf('"');
      int q2=line.indexOf('"',q1+1);
      int q3=line.indexOf('"',q2+1);
      int q4=line.indexOf('"',q3+1);
      if(q3>0 && q4>q3)
        smsSender=line.substring(q3+1,q4);

      delay(120);
      smsText = sim800.readStringUntil('\n');
      smsText.trim();

      if(smsText.length()>42)
        smsText=smsText.substring(0,42);
    }

    /* Call ended */
    if(line.indexOf("NO CARRIER")>=0){
      inCall=false;
      incomingCall=false;
      dialing=false;
      callDurationSec=0;
      dialNumber="";
    }
  }
}

/* ================= KEYPAD ================= */
void readKeypad(){
  if(inCall || incomingCall || dialing || smsView) return;

  for(int i=0;i<10;i++){
    bool now=digitalRead(keyPins[i]);
    if(lastKey[i]==HIGH && now==LOW){
      if(dialNumber.length()<15)
        dialNumber+=keyChars[i];
    }
    lastKey[i]=now;
  }
}

/* ================= BUTTONS ================= */
void readButtons(){

  bool c=digitalRead(CALL_BUTTON);
  bool e=digitalRead(END_BUTTON);
  bool b=digitalRead(BACK_BUTTON);

  /* BACK hides SMS */
  if(lastBack==HIGH && b==LOW && smsView){
    smsView=false;
    smsSender="";
    smsText="";
  }

  /* CALL */
  if(lastCall==HIGH && c==LOW){
    if(incomingCall){
      sim800.println("ATA");
      inCall=true;
      incomingCall=false;
      callStartTime=millis();
    }
    else if(!inCall && dialNumber.length()){
      sim800.print("ATD");
      sim800.print(dialNumber);
      sim800.println(";");
      dialing=true;
      dialingStart=millis();
    }
  }

  /* END */
  if(lastEnd==HIGH && e==LOW){
    sim800.println("ATH");
    inCall=false;
    incomingCall=false;
    dialing=false;
    callDurationSec=0;
    dialNumber="";
  }

  lastCall=c;
  lastEnd=e;
  lastBack=b;
}

/* ================= SIGNAL ================= */
void readSignal(){
  signalStrength=0;
  sim800.println("AT+CSQ");
  delay(200);

  while(sim800.available()){
    String r=sim800.readStringUntil('\n');
    int i=r.indexOf("+CSQ:");
    if(i>=0){
      int c=r.indexOf(",",i);
      signalStrength=r.substring(i+6,c).toInt();
      signalStrength=constrain(signalStrength,0,31);
    }
  }
}

/* ================= BATTERY ================= */
void readBattery(){
  int raw=analogRead(BATTERY_PIN);
  float v=raw*(5.0/1023.0)*2.0;

  if(v>=4.2) batteryPercent=100;
  else if(v<=3.2) batteryPercent=0;
  else batteryPercent=(v-3.2)*100.0/(4.2-3.2);

  batteryPercent=constrain(batteryPercent,0,100);
}

/* ================= SIGNAL ICON ================= */
void drawSignalIcon(int x,int y){
  int bars=map(signalStrength,0,31,0,5);
  for(int i=0;i<bars;i++)
    display.fillRect(x+i*4,y-i*2,3,i*2+4,SSD1306_WHITE);
}

/* ================= UI ================= */
void drawUI(){

  display.clearDisplay();

  drawSignalIcon(0,8);

  display.drawRect(108,0,18,8,SSD1306_WHITE);
  display.fillRect(110,2,map(batteryPercent,0,100,0,14),4,SSD1306_WHITE);
  display.setCursor(80,0);
  display.print(batteryPercent);
  display.print("%");

  display.setCursor(20,16);
  display.println("ULTRA MOBILE");

  if(smsView){
    display.setCursor(0,32);
    display.println("NEW SMS");
    display.setCursor(0,42);
    display.println(smsSender);
    display.setCursor(0,52);
    display.println(smsText);
  }
  else if(incomingCall){
    display.setCursor(10,36);
    display.println("INCOMING CALL");
    display.setCursor(0,48);
    display.println(incomingNumber);
  }
 else if(dialing){
  display.setCursor(20,32);
  display.println("CALLING");

  display.setCursor(0,48);
  display.println(dialNumber);   // <-- number show 
}
 else if(inCall){
  display.setCursor(10,28);
  display.println("CALL CONNECTED");

  // ---- Show number ----
  display.setCursor(10,40);
  display.println(dialNumber);

  // ---- Call timer ----
  int mm = callDurationSec / 60;
  int ss = callDurationSec % 60;
  display.setCursor(40,54);
  if(mm < 10) display.print("0");
  display.print(mm);
  display.print(":");
  if(ss < 10) display.print("0");
  display.print(ss);
}

  else{
    display.setCursor(0,36);
    display.print("Dial:");
    display.println(dialNumber);
  }

  display.display();
}
