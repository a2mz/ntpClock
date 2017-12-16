#include <FS.h>
#include "Arduino.h"
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>       //http://www.arduino.cc/playground/Code/Time
#include <Timezone.h>    //https://github.com/JChristensen/Timezone
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic


// const char *ssid     = "ulitko";
// const char *password = "ghjnjnbg";

// for NodeMCU 1.0

/*
Connect:
NodeMCU    -> Matrix
MOSI-D7-GPIO13  -> DIN
CLK-D5-GPIO14   -> CK
GPIO0-D3        -> CS
*/

#define DIN_PIN 13  // D7 - din
#define CS_PIN  0  // D3 - cs
#define CLK_PIN 14  // D5 -clk

#define NUM_MAX 4
#define MAX_DIGITS 20

#include "max7219.h"
#include "fonts.h"

//https://www.timeanddate.com/time/zone/ukraine/kyiv
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 3, 180};
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 4, 120};
Timezone myTimeZone(CEST, CET);
TimeChangeRule *tcr;
time_t utc ;


int newHours = 0;
int newMinutes = 0;

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "time.google.com", 0, 60000);
//-----------
int repeatCounter = 0;
byte del=0;
byte digold[MAX_DIGITS]={0};
byte dig[MAX_DIGITS]={0};
byte digtrans[MAX_DIGITS]={0};
int h=0;
int m=0;
int s=0;
int dy=0;
int dots = 0;
long dotTime = 0;
int dx=0;
int countSync = 1200; // every 1200 second run time sync
//========================================================================


void setup(){
  Serial.begin(115200);

  //init max7219
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN,1);
  sendCmdAll(CMD_INTENSITY,10);
  printStringWithShift("run   ",80);



 //init wifi manager
 WiFiManager wifiManager;
 // wifiManager.resetSettings();
wifiManager.setAPCallback(configModeCallback);
 wifiManager.autoConnect(".ntpclock");


  clr();
  printStringWithShift(".....sync time",80);
  timeClient.begin();
  timeClient.forceUpdate();



//----------------------------------------------

if (SPIFFS.begin()) {
    Serial.print("mount fs");
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
      Serial.print(dir.fileName());
      File f = dir.openFile("r");
      Serial.println(f.size());
}
}
//----------------------------------------------



}

void loop() {

 utc = timeClient.getEpochTime();
 newHours= int(hour(myTimeZone.toLocal(utc, &tcr)) );
 newMinutes =int(minute(myTimeZone.toLocal(utc, &tcr)));

 if(millis()-dotTime > 500) {

  Serial.println(".");

     dotTime = millis();
     dots = !dots;
     syncTime();
   }
  checkChangeTime();
  showAnimClock();
}
//========================================================================

void configModeCallback (WiFiManager *myWiFiManager) {

   printStringWithShift("set-wifi credentions ot config page 192.168.4.1",80);

}

//========================================================================

void checkChangeTime() {
 if (h != newHours || m != newMinutes) {
  h = newHours;
  m = newMinutes;
 }
}

void syncTime() {
 if (repeatCounter > countSync) {
  timeClient.forceUpdate();
    repeatCounter=0;
  } else {
    repeatCounter++;
    }
}

// =======================================================================

void showSimpleClock()
{
  dx=dy=0;
  clr();
  showDigit(h/10,  0, dig6x8);
  showDigit(h%10,  8, dig6x8);
  showDigit(m/10, 17, dig6x8);
  showDigit(m%10, 25, dig6x8);
  showDigit(s/10, 34, dig6x8);
  showDigit(s%10, 42, dig6x8);
  setCol(15,dots ? B00100100 : 0); // h:m
  setCol(32,dots ? B00100100 : 0); // m:s
  refreshAll();
}

// =======================================================================

void showAnimClock()
{
  byte digPos[6]={0,8,17,25,34,42};
  int digHt = 12;
  int num = 6;
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;

  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig6x8);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig6x8);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig6x8);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
  delay(30);
}

// =======================================================================

void showDigit(char ch, int col, const uint8_t *data)
{
  if(dy<-8 | dy>8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += dx;
  for (int i = 0; i < w; i++)
    if(col+i>=0 && col+i<8*NUM_MAX) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if(!dy) scr[col + i] = v; else scr[col + i] |= dy>0 ? v>>dy : v<<-dy;
    }
}

// =======================================================================

void setCol(int col, byte v)
{
  if(dy<-8 | dy>8) return;
  col += dx;
  if(col>=0 && col<8*NUM_MAX)
    if(!dy) scr[col] = v; else scr[col] |= dy>0 ? v>>dy : v<<-dy;
}

// =======================================================================

int showChar(char ch, const uint8_t *data)
{
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[NUM_MAX*8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[NUM_MAX*8 + i] = 0;
  return w;
}
// =======================================================================

void printCharWithShift(unsigned char c, int shiftDelay) {

  if (c < ' ' || c > '~'+25) return;
  c -= 32;
  int w = showChar(c, font);
  for (int i=0; i<w+1; i++) {
    delay(shiftDelay);
    scrollLeft();
    refreshAll();
  }
}

// =======================================================================

void printStringWithShift(const char* s, int shiftDelay){
  while (*s) {
    printCharWithShift(*s, shiftDelay);
    s++;
  }
}
