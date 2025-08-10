#include <Arduino.h>
#include <Servo.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

using namespace std;

const char* settingHtml = "<!DOCTYPE html>\n<head>\n    <meta charset=\"UTF-8\">\n    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, height=device-width\">\n    <title>智能小车配网模式</title>\n    <style>\n        html, body {\n            height: 100%;\n            display: flex;\n            justify-content: center;\n            align-items: center;\n            background-color: #b6b6b6;\n        }\n        .container {\n            text-align: center;\n            border: 1px solid #00aeff;\n            padding: 20px;\n            box-shadow: 0 0 10px rgba(0, 140, 255, 0.1);\n            background-color: #ffffff;\n        }\n        .title {\n            margin-bottom: 20px;\n            font-size: 24px;\n            font-weight: bold;\n        } \n        .button {\n            margin: 5px;\n            padding: 10px 20px;\n            font-size: 16px;\n            cursor: pointer;\n            width: 100%;\n            border: none;\n            border-radius: 5px;\n            background-color : rgb(56, 175, 0);\n            color: white;\n            text-align:center;\n            text-decoration:none;\n            display:flex;\n            transition: background-color 0.3s ease;\n            border-radius: 5cm;\n        }\n        .button:hover {\n            background-color:  rgb(0, 255, 72)\n        }\n    </style>\n</head>\n<body>\n    <div class=\"container\">\n        <label class=\"title\">智能小车配网</label>\n        <div>WIFI名称：</div>\n        <input id=\"ssid\"></input>\n        <div>WiFi密码：</div>\n        <input id=\"pass\"></input>\n        <div>服务器地址：</div>\n        <input id=\"ip\"></input>\n        <div>服务器端口：</div>\n        <input id=\"port\"></input>\n        <div>服务器身份码：</div>\n        <input id=\"auth\"></input>\n        <button class=\"button\" onclick=\"save()\">保存</button>\n    </div>\n</body>\n<script>\n    function save()\n    {\n        fetch(\"/config?ssid=\" + document.getElementById(\"ssid\").value + \n    \"&pass=\" + document.getElementById(\"pass\").value + \n    \"&ip=\" + document.getElementById(\"ip\").value +\n    \"&port=\" + document.getElementById(\"port\").value +\n    \"&auth=\" + document.getElementById(\"auth\").value).then((rep)=>{\n            return rep.text();\n        }).then((data)=>{\n            if(data == \"success\")\n            {\n                alert(\"设置成功，按下RST重启\");\n            }\n            else{\n                alert(data);\n            }\n        })\n    }\n</script>\n</html>";

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
};

Servo rotateServo;
ESP8266WebServer web;
DNSServer dns;
WiFiClient client;
CarSettings settings;
IPAddress selfAddr(192,168,4,1);
IPAddress serverAddr;
int engineMotorPinLA = D5; //舵机车Left马达就是驱动轮，Right马达为非舵机转向电机
int engineMotorPinLB = D6;
int engineMotorPinRA = D7;
int engineMotorPinRB = D8;
int VBatInputPin = A0;
int rotateServoPin = D4;
float ADCDelta = 3.3f / 1024.0f;
float ADCScale = 5.0f;
bool reverseEngineL = true; //反转动力电机
bool reverseEngineR = true; //反转动力电机
int engineSpeedPWM = 500;
int rotateMotorPWM = 750;
bool configMode = false;
bool useServoRotate = false; //是否是舵机转向结构（非舵机阿克曼结构也为true）
int chasuPWM = 256;
int turnLeftServoDeg = 25;
int centServoDeg = 97; //左右转都是相对值
int turnRightServoDeg = -25;
int trueEEPFlag = 114514;


void EEPWriteInt(int addr,int val)
{
  char* p = (char*)&val;
  for(int i = 0;i < 4;i++)
  {
    EEPROM.write(addr + i,p[i]);
  }
  EEPROM.commit();
}
void EEPWriteBlock(int addr,int size,const char* src)
{
  Serial.printf("len=%d  addr=%d\n",size,addr);

  for(int i = 0;i < size;i++)
  {
    EEPROM.write(addr + i,src[i]);        
  }
  EEPROM.commit();
}
void EEPReadBlock(int addr,int size,char* dst)
{
  for(int i = 0;i < size;i++)
  {
    dst[i] = EEPROM.read(addr + i);
  }
}
int EEPReadInt(int addr)
{
  int ret = 0;
  char* p = (char*)&ret;
  for(int i = 0;i < 4;i++)
  {
    p[i] = EEPROM.read(addr + i);
  }
  return ret;
}

float getRawVBat()
{
  return analogRead(VBatInputPin) * ADCDelta * ADCScale; 
}
float getVBat()
{
  return getRawVBat() + settings.adcOffset;
}
bool readSetting()
{
  int eepFlag = EEPReadInt(0);
  Serial.printf("eepFlag=%d\n",eepFlag);
  if(eepFlag == trueEEPFlag)
  {
    EEPReadBlock(4,sizeof(CarSettings),(char*)&settings);
    serverAddr = IPAddress(settings.serverIP);
    if(reverseEngineL)
    {
      int temp = engineMotorPinLA;
      engineMotorPinLA = engineMotorPinLB;
      engineMotorPinLB = temp;
    }
    if(reverseEngineR)
    {
      int temp = engineMotorPinRA;
      engineMotorPinRA = engineMotorPinRB;
      engineMotorPinRB = temp;
    }
    return true;
  }
  else
  {
    return false;
  }
}

void setRotatePWM(int enginePWM,int rotate,double PWMScare = 1) //差速车专用计算两个电机出力
{
  double val = rotate * 2 - 100;
  Serial.printf("val=%d\n",val);
  int lmotor = enginePWM + (val / 100 * chasuPWM * PWMScare);
  int rmotor = enginePWM - (val / 100 * chasuPWM * PWMScare);
  if(lmotor < 0)
  {
    analogWrite(engineMotorPinLB,-lmotor);
    digitalWrite(engineMotorPinLA,LOW);
  }
  else
  {
    analogWrite(engineMotorPinLA,lmotor);
    digitalWrite(engineMotorPinLB,LOW);
  }
  
  if(rmotor < 0)
  {
    analogWrite(engineMotorPinRB,-rmotor);
    digitalWrite(engineMotorPinRA,LOW);
  }
  else
  {
    analogWrite(engineMotorPinRA,rmotor);
    digitalWrite(engineMotorPinRB,LOW);
  }
  Serial.printf("rotate=%d lmotor=%d  rmotor=%d\n",rotate,lmotor,rmotor);
}

void connectServer()
{
  digitalWrite(2,HIGH);
  digitalWrite(engineMotorPinLA,LOW);  
  digitalWrite(engineMotorPinLB,LOW);
  digitalWrite(engineMotorPinRA,LOW);  
  digitalWrite(engineMotorPinRB,LOW);
  client = WiFiClient();
  if(!client.connect(IPAddress(settings.serverIP),settings.serverPort))
  {
    Serial.println("connect failed,reconnecting...\n");
    return;
  }
  analogWrite(2,150);
  char buffer[16];
  char sendbuf[16];
  int* pSendbuf = (int*)sendbuf;
  int timeout_delay = 2500; //看门狗最大等待数据包时间
  int chasuVal = 50; //差速车的差速转向比例值
  int MotorState = 0; //车辆行驶状态 0=停止  1=前进  2=后退
  unsigned long timeout = millis() + timeout_delay;
  pSendbuf[0] = 10;
  long long* tmppll = (long long*)&pSendbuf[1]; //身份码是64位的，转换指针
  tmppll[0] = settings.authId;
  client.write_P((const char*)sendbuf,16); //发送登录包
  while(client.connected() && WiFi.isConnected())
  {
    if(millis() > timeout)
    {
      Serial.println("server heartbeat timeout,reconnecting...");
      client.stop();
      return;
    }
    if(client.available())
    {
      client.readBytes(buffer,16);
      int* p = (int*)buffer;
      if(p[0] == 1) //控制车辆
      {
        Serial.printf("car run cmd=%d\n",p[1]);     
        if(p[1] == 0) //停车
        {
          MotorState = 0;
          digitalWrite(engineMotorPinLA,LOW);  
          digitalWrite(engineMotorPinLB,LOW);      
          if(!useServoRotate)
          {
            digitalWrite(engineMotorPinRA,LOW);  
            digitalWrite(engineMotorPinRB,LOW);  
          }         
        }
        else if(p[1] == 1) //前进
        {     
          MotorState = 1;
          if(!useServoRotate)
          {
            setRotatePWM(engineSpeedPWM,chasuVal);
          }       
          else
          {
            analogWrite(engineMotorPinLA,engineSpeedPWM);
            digitalWrite(engineMotorPinLB,LOW);
          }
        }
        else if(p[1] == 2) //后退
        {         
          MotorState = 2;
          if(!useServoRotate)
          {
            setRotatePWM(-engineSpeedPWM,chasuVal);
          }   
          else
          {
            analogWrite(engineMotorPinLB,engineSpeedPWM);
            digitalWrite(engineMotorPinLA,LOW);
          }    
        }
        else if(p[1] == 3) //左转（无比例转向的车）
        {
          if(useServoRotate)
          {
            analogWrite(engineMotorPinRA,rotateMotorPWM);
            digitalWrite(engineMotorPinRB,LOW);
          }
          //analogWrite(rotateMotorPinA,rotateMotorPWM);
          //digitalWrite(rotateMotorPinB,LOW);
        }
        else if(p[1] == 4) //右转（无比例转向的车）
        {
          if(useServoRotate)
          {
            analogWrite(engineMotorPinRB,rotateMotorPWM);
            digitalWrite(engineMotorPinRA,LOW);
          }
          //analogWrite(rotateMotorPinB,rotateMotorPWM);
          //digitalWrite(rotateMotorPinA,LOW);  
        }
        else if(p[1] == 5) //回中（无比例转向的车）
        {
          if(useServoRotate)
          {
            digitalWrite(engineMotorPinRB,LOW);
            digitalWrite(engineMotorPinRA,LOW);
          }
          //digitalWrite(rotateMotorPinA,LOW);
          //digitalWrite(rotateMotorPinB,LOW);
        }
        else if(p[1] == 6) //转向比例(50回中,0左转,100右转)
        {
          if(useServoRotate)
          {
            int val = p[2];
            if(val >= 0 && val <= 100)
            {
              if(val == 50)
              {
                rotateServo.write(centServoDeg);
                Serial.printf("turn center\n");
              }
              else if(val < 50)
              {
                int deg = (50.0f - val) / 50.0f * turnLeftServoDeg +  centServoDeg;
                rotateServo.write(deg);
                Serial.printf("turn left val=%d,deg=%d\n",val,deg);
              }
              else if(val > 50)
              {
                int deg = (val - 50.0f) / 50.0f * turnRightServoDeg +  centServoDeg;
                rotateServo.write(deg);
                Serial.printf("turn right val=%d,deg=%d\n",val,deg);
              }
            }
          }
          else //差速车
          {
            chasuVal = p[2];
            if(MotorState == 0)
            {
              setRotatePWM(0,chasuVal,1.3);
            }
            else if(MotorState == 1)
            {
              setRotatePWM(engineSpeedPWM,chasuVal,1);
            }
            else if(MotorState == 2)
            {
              setRotatePWM(-engineSpeedPWM,chasuVal,-1);
            }
          }
        }
      }
      else if(p[0] == 2)  //连接测试
      {
        timeout = millis() + timeout_delay; //喂狗
        memset(sendbuf,0,16);
        float* sendpf = (float*)sendbuf;
        pSendbuf[0] = 2;
        sendpf[1] = getVBat();
        client.write_P(sendbuf,16); //发送心跳包回复
        Serial.println("connection check called");
      }
      else if(p[0] == 3) //PWM调速
      {
        engineSpeedPWM = p[1];
        rotateMotorPWM = p[2];
        Serial.printf("pwm config enginePWM=%d,rotatePWM=%d\n",engineSpeedPWM,rotateMotorPWM);
      }
      else if(p[0] == 4) //电压计ADC校准
      {
        float adcVal = getRawVBat();
        float* pfloat = (float*)p;
        settings.adcOffset = pfloat[1] - adcVal;
        EEPWriteBlock(4,sizeof(CarSettings),(char*)&settings); //保存设置，将设置信息结构体存入EEPROM
        memset(sendbuf,0,16);
        pSendbuf[0] = 3;
        pSendbuf[1] = 1;
        client.write_P(sendbuf,16);
        Serial.printf("rawVol=%.2f  adcOffset=%.2f\n",adcVal,settings.adcOffset);
      }
      else if(p[0] == 5) //转向舵机校准
      {
        if(p[1] == 1) //测试舵机角度
        {
          int deg = p[2];
          if(deg >= 0 && deg <= 180)
          {
            rotateServo.write(deg);
          }         
        }
        else if(p[1] == 2) //保存设置
        {
          short* pshort = (short*)&p[2];
          int cent = pshort[0];
          int left = pshort[1];
          int right = pshort[2];

        }
      }
    }
  }
}
bool connectWifi()
{
  WiFi.begin(settings.wifiSSID,settings.wifiPassword);
  Serial.print("connecting.");
  int tryCount = 0;
  while(WiFi.status() != WL_CONNECTED && tryCount < 20)
  {
    delay(950);
    digitalWrite(2,LOW);
    delay(50);
    digitalWrite(2,HIGH);
    Serial.print(".");
    ++tryCount;
  }
  return WiFi.status() == WL_CONNECTED;
}
void sendPage()
{
  web.send(200,"text/html",settingHtml);
}
void onConfig()
{
  strcpy(settings.wifiSSID,web.arg("ssid").c_str());
  strcpy(settings.wifiPassword,web.arg("pass").c_str());
  IPAddress serverip;
  serverip.fromString(web.arg("ip"));
  settings.serverIP = serverip.v4();
  settings.serverPort = atoi(web.arg("port").c_str());
  settings.authId = strtoll(web.arg("auth").c_str(),NULL,10);
  EEPWriteInt(0,trueEEPFlag);
  EEPWriteBlock(4,sizeof(CarSettings),(char*)&settings);
  web.send(200,"text/plain","success");
  delay(1000);
  ESP.restart();
}
void apSetting()
{
  Serial.println("进入配网模式");
  configMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("智能小车配网");
  web.onNotFound(sendPage);
  web.on("/config",onConfig);
  web.begin(80);
  dns.start(53,"*",selfAddr);
}

void setup() {
  Serial.begin(9600);
  EEPROM.begin(1024);
  analogWriteRange(1023);
  pinMode(2,OUTPUT);
  pinMode(engineMotorPinLA,OUTPUT);
  pinMode(engineMotorPinLB,OUTPUT);
  pinMode(engineMotorPinRA,OUTPUT);
  pinMode(engineMotorPinRB,OUTPUT);
  pinMode(VBatInputPin,INPUT);
  pinMode(rotateServoPin,OUTPUT);
  rotateServo.attach(rotateServoPin,500,2500);
  digitalWrite(2,HIGH);
  if(readSetting())
  {
    Serial.printf("setting:\nssid=%s\npass=%s\nserver=%s\nport=%d\nauthId=%lld\nadcOffset=%.2f\n",settings.wifiSSID,settings.wifiPassword,serverAddr.toString().c_str(),settings.serverPort,settings.authId,settings.adcOffset);
    //配置过，按照设置连接wifi和服务器r
    if(connectWifi())
    {
      //连接成功，开始连接服务器
      Serial.println("connect wifi successed\nconnecting server.");
      digitalWrite(2,LOW);
      delay(500);
      digitalWrite(2,HIGH);
      rotateServo.write(centServoDeg); //回中转向架
    }
    else
    {
      //连接失败，开启配网模式
      apSetting();
    }
  }
  else
  {
    //没有配置过，开启热点进入配网模式
    apSetting();
  }
}

void loop() {
  if(configMode)
  {
    dns.processNextRequest();
    web.handleClient();    
  }
  else
  {
    connectServer();
  }

}
