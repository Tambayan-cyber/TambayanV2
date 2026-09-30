#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,20,4);
#define RELAY_WASH_FWD 23
#define RELAY_WASH_REV 19
#define RELAY_INLET 18
#define RELAY_DRAIN 5
#define RELAY_SPIN 13
#define PIN_PRESSURE 34
#define PIN_COIN 4
#define PIN_BTN_UP 15
#define PIN_BTN_DOWN 2
#define PIN_BTN_SELECT 25
#define PIN_DOOR_TUB1 16
#define PIN_DOOR_TUB2 17
#define PIN_CURRENT 26
#define PIN_BUZZER 27
WebServer server(80);
String USERNAME="admin",PASSWORD="1234";
bool isLoggedIn=false;
struct Settings{
int washFwd=6,washRev=6,washDelay=3,washMinDefault=15,washPricePerMin=1;
int rinseFwd=4,rinseRev=4,rinseDelay=3,rinseMinDefault=3,rinseCycleDefault=2,rinsePricePerMin=1;
int spinMinDefault=5,spinPricePerMin=1;
int drainFinal=30,drainManual=30,drainMin=10,drainMax=120;
int autoWash=20,autoRinseMin=3,autoRinseCycle=2;
int startWash=1,startRinse=1,startSpin=1;
int waterLow=400,waterHi=800;
int washOptions[4]={10,15,20,25};
int rinseMinOptions[3]={3,5,10};
int rinseCycleOptions[3]={2,3,4};
int spinOptions[3]={3,5,10};
};
Settings set;
volatile int coinCount=0;
int pondo=0;
enum State{TAMBAYAN,MENU_AUTO,MENU_MANUAL,MENU_DRAIN,WASH_RUN,RINSE_RUN,SPIN_RUN,DRAIN_RUN,SELECT_WASH,SELECT_RINSE,SELECT_SPIN};
State state=TAMBAYAN;
bool tub1Busy=false,tub2Busy=false;
unsigned long endTime=0,drainEndTime=0;
int sel=0,manualSel=0;
bool rinseQueued=false;
int queuedRinseMin=0,queuedRinseCycle=0,currentRinseMin=3,currentRinseCycle=2;
void IRAM_ATTR coinISR(){coinCount++;}
*=== PART 2/6 ===*
bool btnUp(){return digitalRead(PIN_BTN_UP)==LOW;}
bool btnDown(){return digitalRead(PIN_BTN_DOWN)==LOW;}
bool btnSelect(){return digitalRead(PIN_BTN_SELECT)==LOW;}
bool btnDownLong(){if(digitalRead(PIN_BTN_DOWN)==LOW){delay(800);if(digitalRead(PIN_BTN_DOWN)==LOW)return true;}return false;}
void buzz(int ms){digitalWrite(PIN_BUZZER,HIGH);delay(ms);digitalWrite(PIN_BUZZER,LOW);}
void handleCoinPondo(){if(coinCount>0){pondo+=coinCount;coinCount=0;buzz(100);}}
void handleDoorLogic(){bool o=digitalRead(PIN_DOOR_TUB1)==HIGH;if(tub1Busy&&o){digitalWrite(RELAY_WASH_FWD,HIGH);digitalWrite(RELAY_WASH_REV,HIGH);}}
bool checkCurrentLimiter(){if(digitalRead(PIN_CURRENT)==LOW){digitalWrite(RELAY_WASH_FWD,HIGH);digitalWrite(RELAY_WASH_REV,HIGH);digitalWrite(RELAY_SPIN,HIGH);lcd.clear();lcd.setCursor(0,0);lcd.print("OVER CURRENT!");delay(1000);lcd.clear();return true;}return false;}
void saveSettings(){EEPROM.put(0,set);EEPROM.commit();}
void loadSettings(){EEPROM.get(0,set);if(set.drainFinal<10||set.drainFinal>120)set.drainFinal=30;if(set.drainManual<10||set.drainManual>120)set.drainManual=30;}
void drainPhase(){digitalWrite(RELAY_DRAIN,LOW);drainEndTime=millis()+set.drainFinal*1000;state=DRAIN_RUN;}
void startWash(int m,bool q,int rm,int rc){tub1Busy=true;endTime=millis()+m*60000;state=WASH_RUN;if(q){rinseQueued=true;queuedRinseMin=rm;queuedRinseCycle=rc;}}
void startRinse(int mi,int cy){tub1Busy=true;currentRinseMin=mi;currentRinseCycle=cy;endTime=millis()+mi*cy*60000;state=RINSE_RUN;}
void startSpin(int m){tub2Busy=true;endTime=millis()+m*60000;digitalWrite(RELAY_SPIN,LOW);state=SPIN_RUN;}
void startManualDrain(){drainEndTime=millis()+set.drainManual*1000;digitalWrite(RELAY_DRAIN,LOW);state=DRAIN_RUN;}
void washMotorLoop(){static unsigned long last=0;static bool fwd=true;if(millis()-last>set.washFwd*1000){fwd=!fwd;last=millis();digitalWrite(RELAY_WASH_FWD,fwd?LOW:HIGH);digitalWrite(RELAY_WASH_REV,fwd?HIGH:LOW);}}
void rinseMotorLoop(){washMotorLoop();}
*=== PART 3/6 ===*
void handleRoot(){String h="<h1>VENDO WASH V3</h1><p>Pondo:"+String(pondo)+"</p><a href='/settings'>Settings</a> <a href='/drainOn'>DrainON</a> <a href='/drainOff'>DrainOFF</a>";server.send(200,"text/html",h);}
void handleLogin(){server.send(200,"text/html","Login OK");}
void handleSettings(){server.send(200,"text/html","<h1>Settings</h1><p>OK</p>");}
void handleSave(){saveSettings();server.send(200,"text/html","Saved");}
void handleCalibrate(){server.send(200,"text/html","Calib OK");}
void handleDrainOn(){digitalWrite(RELAY_DRAIN,LOW);server.send(200,"text/html","Drain ON");}
void handleDrainOff(){digitalWrite(RELAY_DRAIN,HIGH);server.send(200,"text/html","Drain OFF");}
void setup(){Serial.begin(115200);EEPROM.begin(1024);loadSettings();
pinMode(RELAY_WASH_FWD,OUTPUT);pinMode(RELAY_WASH_REV,OUTPUT);pinMode(RELAY_INLET,OUTPUT);pinMode(RELAY_DRAIN,OUTPUT);pinMode(RELAY_SPIN,OUTPUT);
pinMode(PIN_BTN_UP,INPUT_PULLUP);pinMode(PIN_BTN_DOWN,INPUT_PULLUP);pinMode(PIN_BTN_SELECT,INPUT_PULLUP);
pinMode(PIN_DOOR_TUB1,INPUT_PULLUP);pinMode(PIN_DOOR_TUB2,INPUT_PULLUP);pinMode(PIN_CURRENT,INPUT_PULLUP);pinMode(PIN_COIN,INPUT_PULLUP);pinMode(PIN_BUZZER,OUTPUT);
digitalWrite(RELAY_WASH_FWD,HIGH);digitalWrite(RELAY_WASH_REV,HIGH);digitalWrite(RELAY_INLET,HIGH);digitalWrite(RELAY_DRAIN,HIGH);digitalWrite(RELAY_SPIN,HIGH);
lcd.init();lcd.backlight();lcd.clear();lcd.setCursor(0,0);lcd.print(" V3 VENDO WASH");delay(2000);
WiFi.softAP("VENDO-WASH-V3","12345678");
server.on("/",handleRoot);server.on("/login",handleLogin);server.on("/settings",handleSettings);server.on("/save",handleSave);
server.on("/calibrate",handleCalibrate);server.on("/drainOn",handleDrainOn);server.on("/drainOff",handleDrainOff);server.begin();
attachInterrupt(digitalPinToInterrupt(PIN_COIN),coinISR,FALLING);}
START ===*
void loop(){server.handleClient();if(checkCurrentLimiter())return;handleDoorLogic();handleCoinPondo();
if(state==TAMBAYAN){lcd.setCursor(0,0);lcd.print("Tambayan P:"+String(pondo)+" ");lcd.setCursor(0,1);if(sel==0)lcd.print(">Auto ");else if(sel==1)lcd.print(">Manual ");else lcd.print(">Drain ");lcd.setCursor(0,2);lcd.print("UP/DN SEL ");lcd.setCursor(0,3);lcd.print("Insert Coin ");if(btnUp()){sel++;if(sel>2)sel=0;delay(150);}if(btnDown()){sel--;if(sel<0)sel=2;delay(150);}if(btnSelect()){if(sel==0){if(tub1Busy){lcd.clear();lcd.print("TUB1 BUSY!");buzz(1000);delay(1000);}else{state=MENU_AUTO;lcd.clear();}}if(sel==1){state=MENU_MANUAL;sel=0;lcd.clear();}if(sel==2){if(tub1Busy){lcd.clear();lcd.print("BUSY!");buzz(1000);delay(1500);}else{state=MENU_DRAIN;lcd.clear();}}delay(300);}}
if(state==MENU_AUTO){int total=set.autoWash+(set.autoRinseMin*set.autoRinseCycle);int price=(set.autoWash*set.washPricePerMin)+(set.autoRinseMin*set.autoRinseCycle*set.rinsePricePerMin);lcd.setCursor(0,0);lcd.print("Auto:"+String(set.autoWash)+"+"+String(set.autoRinseMin)+"x"+String(set.autoRinseCycle)+" ");lcd.setCursor(0,1);lcd.print("Tot:"+String(total)+"m P"+String(price)+" ");lcd.setCursor(0,2);lcd.print("Pondo:"+String(pondo)+" Need:"+String(price-pondo)+" ");if(btnUp()){state=TAMBAYAN;sel=0;lcd.clear();delay(200);}if(btnSelect()){if(pondo>=price){pondo-=price;startWash(set.autoWash,true,set.autoRinseMin,set.autoRinseCycle);}else buzz(500);}}
*=== PART 5/6 ===*
if(state==MENU_MANUAL){lcd.setCursor(0,0);lcd.print("Manual Select:");lcd.setCursor(0,1);if(sel==0)lcd.print(">Wash 10/15/20/25 ");else if(sel==1)lcd.print(">Rinse 3/5/10 x2/3/4");else lcd.print(">Spin 3/5/10 ");lcd.setCursor(0,2);lcd.print("Pondo:"+String(pondo));if(btnUp()){sel++;if(sel>2)sel=0;delay(150);}if(btnDown()){if(sel==0){state=TAMBAYAN;lcd.clear();}else sel--;delay(150);}if(btnSelect()){if(sel==0){state=SELECT_WASH;manualSel=0;lcd.clear();}if(sel==1){state=SELECT_RINSE;manualSel=0;lcd.clear();}if(sel==2){state=SELECT_SPIN;manualSel=0;lcd.clear();}delay(300);}}
if(state==SELECT_WASH){int m=set.washOptions[manualSel];int price=m*set.washPricePerMin;lcd.setCursor(0,0);lcd.print("Wash Select:");lcd.setCursor(0,1);lcd.print(">"+String(m)+" min P"+String(price)+" ");if(btnUp()){manualSel++;if(manualSel>3)manualSel=0;delay(150);}if(btnDown()){manualSel--;if(manualSel<0)manualSel=3;delay(150);}if(btnSelect()){if(!tub1Busy&&pondo>=price){pondo-=price;startWash(m,false,0,0);}else buzz(500);delay(300);}}
if(state==SELECT_RINSE){int min=set.rinseMinOptions[manualSel%3];int cyc=set.rinseCycleOptions[manualSel/3%3];if(manualSel>8)manualSel=0;int price=min*cyc*set.rinsePricePerMin;lcd.setCursor(0,0);lcd.print("Rinse "+String(min)+"m x"+String(cyc)+" P"+String(price)+" ");if(btnUp()){manualSel++;delay(150);}if(btnDown()){manualSel--;if(manualSel<0)manualSel=8;delay(150);}if(btnSelect()){if(tub1Busy){long rem=(endTime-millis())/60000;if(rem<=1)buzz(1000);else if(pondo>=price){pondo-=price;rinseQueued=true;queuedRinseMin=min;queuedRinseCycle=cyc;buzz(200);state=WASH_RUN;}}else if(pondo>=price){pondo-=price;startRinse(min,cyc);}}}
*=== PART 6/6 - LAST ===*
if(state==SELECT_SPIN){int m=set.spinOptions[manualSel];int price=m*set.spinPricePerMin;lcd.setCursor(0,0);lcd.print("Spin "+String(m)+"m P"+String(price)+" ");if(btnUp()){manualSel++;if(manualSel>2)manualSel=0;delay(150);}if(btnDown()){manualSel--;if(manualSel<0)manualSel=2;delay(150);}if(btnSelect()){if(!tub1Busy&&!tub2Busy&&pondo>=price){pondo-=price;startSpin(m);}else buzz(500);delay(300);}}
if(state==MENU_DRAIN){lcd.setCursor(0,0);lcd.print("Manual Drain TEST");lcd.setCursor(0,1);lcd.print("Time:"+String(set.drainManual)+"s");if(btnSelect()){if(tub1Busy){buzz(1000);state=TAMBAYAN;}else startManualDrain();delay(300);}}
if(state==DRAIN_RUN){long rem=(drainEndTime-millis())/1000;if(rem<0)rem=0;lcd.setCursor(0,0);lcd.print("DRAINING Rem:"+String(rem)+"s ");if(millis()>=drainEndTime){digitalWrite(RELAY_DRAIN,HIGH);state=TAMBAYAN;buzz(500);lcd.clear();}}
if(state==WASH_RUN){washMotorLoop();lcd.setCursor(0,0);lcd.print("Wash "+String((endTime-millis())/60000)+"m Rmn ");if(millis()>=endTime){drainPhase();if(rinseQueued){int rm=queuedRinseMin;int rc=queuedRinseCycle;rinseQueued=false;startRinse(rm,rc);}else{tub1Busy=false;state=TAMBAYAN;buzz(3000);}}}
if(state==RINSE_RUN){rinseMotorLoop();lcd.setCursor(0,0);lcd.print("Rinse Rem:"+String((endTime-millis())/60000)+"m ");if(millis()>=endTime){drainPhase();tub1Busy=false;state=TAMBAYAN;buzz(3000);}}
if(state==SPIN_RUN){lcd.setCursor(0,0);lcd.print("Spin Rem:"+String((endTime-millis())/60000)+"m ");if(millis()>=endTime){digitalWrite(RELAY_SPIN,HIGH);tub2Busy=false;state=TAMBAYAN;buzz(3000);}}}
