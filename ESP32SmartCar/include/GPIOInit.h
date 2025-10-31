#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

struct CarSettings
{
  char wifiSSID[32];
  char wifiPassword[16];
  int serverIP;
  int serverPort;
  long long authId;
  float adcOffset;
  short rotateCentDeg;
  short rotateLeftDeg;
  short rotateRightDeg; 
  float shutdownVol; //关机电压，为0.0则不主动关机（禁用）
};

//静态全局变量扩展
extern Servo rotateServo;
extern Servo shootPitchServo;
extern WebServer web;
extern DNSServer dns;
extern WiFiClient client;
extern CarSettings settings;
extern IPAddress selfAddr;
extern IPAddress serverAddr;

extern int engineMotorPinLA;
extern int engineMotorPinLB;
extern int engineMotorPinRA;
extern int engineMotorPinRB;
extern int VBatInputPin;
extern int rotateServoPin;
extern int shootPitchServoPin;
extern int shootFireMOSPin;
extern int powerENPin;

extern float ADCDelta;
extern float ADCScale;
extern bool reverseEngineL;
extern bool reverseEngineR;
extern int engineSpeedPWM;
extern int rotateMotorPWM;
extern bool configMode;
extern bool useServoRotate;
extern int chasuPWM;
extern int turnLeftServoDeg;
extern int centServoDeg;
extern int turnRightServoDeg;
extern int trueEEPFlag;

void initGpio();