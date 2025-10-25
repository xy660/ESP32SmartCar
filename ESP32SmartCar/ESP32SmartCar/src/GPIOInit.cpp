#include "GPIOInit.h"

//初始化所有静态全局变量
Servo rotateServo;
Servo shootPitchServo;
WebServer web(80);
DNSServer dns;
WiFiClient client;
CarSettings settings;
IPAddress selfAddr(192,168,4,1);
IPAddress serverAddr;

int engineMotorPinLA = 27;
int engineMotorPinLB = 26;
int engineMotorPinRA = 14;
int engineMotorPinRB = 12;
int VBatInputPin = 34;
int rotateServoPin = 13;
int shootPitchServoPin = 16;
int shootFireMOSPin = 17;
int powerENPin = 25; //外设供电使能信号引脚，自动关机要用

float ADCDelta = 3.3f / 4095.0f;
float ADCScale = 5.0f; //分压器缩放倍率
bool reverseEngineL = true;
bool reverseEngineR = true;
int engineSpeedPWM = 500;
int rotateMotorPWM = 750;
bool configMode = false;
bool useServoRotate = false;
int chasuPWM = 256;
int turnLeftServoDeg = 25;
int centServoDeg = 97;
int turnRightServoDeg = -25;
int trueEEPFlag = 114514;


void initGpio()
{
    pinMode(powerENPin,OUTPUT);
    digitalWrite(powerENPin,HIGH); //上电自动打开外设电源

    Serial.begin(9600);
    EEPROM.begin(1024);
    // analogWriteRange(1023);
    ledcSetup(4, 25000, 10); // 初始化esp32 ledc控制器
    ledcSetup(5, 25000, 10);
    ledcSetup(6, 25000, 10);
    ledcSetup(7, 25000, 10);
    pinMode(2, OUTPUT);
    pinMode(engineMotorPinLA, OUTPUT);
    pinMode(engineMotorPinLB, OUTPUT);
    pinMode(engineMotorPinRA, OUTPUT);
    pinMode(engineMotorPinRB, OUTPUT);
    pinMode(VBatInputPin, INPUT);
    pinMode(rotateServoPin, OUTPUT);
    rotateServo.attach(rotateServoPin, 500, 2500); // 绑定引脚
    shootPitchServo.attach(shootPitchServoPin, 500, 2500);
    ledcAttachPin(engineMotorPinLA, 4);
    ledcAttachPin(engineMotorPinLB, 5);
    ledcAttachPin(engineMotorPinRA, 6);
    ledcAttachPin(engineMotorPinRB, 7);
    pinMode(shootFireMOSPin, OUTPUT);
    digitalWrite(shootFireMOSPin, LOW);
    digitalWrite(2, LOW);
}