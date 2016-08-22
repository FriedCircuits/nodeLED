#include "AlaLedRgb.h"
#include <ESP8266WiFi.h> //Has to be after AlaLedRgb otherwise min() is undefined
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ClickButton.h>

const char* ssid = "ssid";
const char* password = "password";

#define LED D0
#define GREENPIN D5
#define REDPIN D6
#define BLUEPIN D7
#define WHITEPIN D8
#define BRIGHTNESS A0
#define BTN1 D2
#define BTN2 D1
#define MAXMODE 5

ClickButton pwrBtn(BTN1, LOW, CLICKBTN_PULLUP);
ClickButton modeBtn(BTN2, LOW, CLICKBTN_PULLUP);
 
uint8_t bright = 128;
AlaLedRgb rgbLed;
AlaColor blue_[1] = {0x0000FF};
AlaPalette mycolors = {1, blue_};
AlaSeq seq[] =
{
  { ALA_FADECOLORSLOOP, 4000, 8000, alaPalRgb },
  { ALA_ENDSEQ }
};
AlaSeq seqOff[] =
{
  { ALA_OFF, 1000, 1000, alaPalNull },
  { ALA_ENDSEQ }
};
AlaSeq seqBlue[] =
{
  {ALA_ON, 5000, 5000, mycolors},
  { ALA_ENDSEQ }
};
AlaSeq seqStrobe[] =
{
  {ALA_STROBO, 100, 1000, mycolors},
  { ALA_ENDSEQ }
};
AlaSeq seqGlow[] =
{
  {ALA_GLOW, 6000, 2000, alaPalRgb},
  { ALA_ENDSEQ }
};

WiFiClient espClient;
ESP8266WebServer server(80);

bool pwrState = 1; //1 = On 0 = Off
uint8_t modeState = 1; //0 = Color test mode
bool lastMode = 0;
bool lastPwr = 0;
unsigned long lastCheckBrightness = 0;
const long checkBrightnessInterval = 100;


void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.hostname("nodeLED");
  WiFi.begin(ssid, password);
  unsigned long lastCheck = millis(); //Track how long we have been waiting for WiFi connection
  bool connected = false;

  while (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < 500; i++) {
      delay(1);
    }
    Serial.print(".");
    if (millis() - lastCheck >= 10000) {
      connected = false;
      break;
    }
    connected = true;
  }
  Serial.println("");
  if (!connected) {
    Serial.println("WiFi not connected");
    Serial.println("Enabling AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("NODELED");
    Serial.println("AP enabled");
    Serial.println("192.168.4.1");
  } else {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", [](){
    server.send(200, "text/plain", "nodeLED v1.0");
  });
  server.begin();
  Serial.println("HTTP server started");
}

void setup_pins() {
  //Status onboard LED
  pinMode(LED, OUTPUT);
  //Initialize the RGB RED
  rgbLed.initPWM(REDPIN, GREENPIN, BLUEPIN);
  pinMode(BRIGHTNESS, INPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  //Quickly turn them off, GPIO's are high on boot
  digitalWrite(REDPIN, LOW);
  digitalWrite(BLUEPIN, LOW);
  digitalWrite(GREENPIN, LOW);
  digitalWrite(WHITEPIN, LOW);
}
//Returns ADC value of pot mapped to a PWM range value
int get_brightness() {
  int bri =  analogRead(BRIGHTNESS);
  map(bri, 0, 1023, 0, 255);
}

void flash_led() {
  digitalWrite(LED, LOW);
  delay(10);
  digitalWrite(LED, HIGH);
  delay(10);
}

void checkButton() {
  int8_t btnState = 0;
  btnState = pwrBtn.clicks;
  if (btnState == 1) {
    pwrState = !pwrState;
    flash_led();
  }
  btnState = modeBtn.clicks;
  if (btnState == 1) {
    modeState = (modeState + 1) % MAXMODE;
    flash_led();
  }
}

void setup() {
  Serial.begin(115200);
  setup_pins();
  setup_wifi();
  rgbLed.setBrightness(0x66FF44);
  rgbLed.setAnimation(seqOff);
}

void loop() {
  if(millis() - lastCheckBrightness >= checkBrightnessInterval){
    lastCheckBrightness = millis();
    bright = get_brightness();  
  }
  rgbLed.setBrightness(((long)bright << 16L) | ((long)bright << 8L) | (long)bright);
  pwrBtn.Update();
  modeBtn.Update();
  checkButton();
  rgbLed.runAnimation();
  server.handleClient();
  if (pwrState == 0 and lastPwr == 1) {
    rgbLed.setAnimation(seqOff);
    analogWrite(WHITEPIN, 0);
    lastPwr = 0;
  }
  if (lastPwr == 0 and pwrState == 1) {}
  if (pwrState == 1) {
    switch (modeState) {
      case 0: //Run Animation
        if (lastMode != modeState or lastPwr == 0) {
          analogWrite(WHITEPIN, 0);
          rgbLed.setAnimation(seq);
        }
        break;
      case 1: //Warm White
        if (lastMode != modeState or lastPwr == 0)
          rgbLed.setAnimation(seqOff);
        analogWrite(WHITEPIN, bright);
        break;

      case 2: //Cool White
        if (lastMode != modeState or lastPwr == 0)
          rgbLed.setAnimation(seqBlue);
        analogWrite(WHITEPIN, bright);
        break;
      case 3: //Full Bright
        if (lastMode != modeState or lastPwr == 0) {
          rgbLed.setAnimation(seqOff);
          analogWrite(WHITEPIN, 255);
          analogWrite(BLUEPIN, 255);
          analogWrite(REDPIN, 255);
          analogWrite(GREENPIN, 255);
        }
        break;
      case 4: //Glow
        if (lastMode != modeState or lastPwr == 0) {
          rgbLed.setAnimation(seqGlow);
          analogWrite(WHITEPIN, 0);
        }
        break;
      default:
        break;
    }
    lastMode = modeState;
    lastPwr = pwrState;
  }
}
