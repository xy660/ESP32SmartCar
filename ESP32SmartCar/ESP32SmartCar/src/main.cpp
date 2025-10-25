#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFiServer.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

#include "ConfigHtml.h"
#include "GPIOInit.h"
#include "eeprom_util.h"
#include "battery_manager.h"

using namespace std;

bool readSetting()
{
  int eepFlag = EEPReadInt(0);
  Serial.printf("eepFlag=%d\n", eepFlag);
  if (eepFlag == trueEEPFlag)
  {
    EEPReadBlock(4, sizeof(CarSettings), (char *)&settings);
    serverAddr = IPAddress(settings.serverIP);
    if (reverseEngineL)
    {
      int temp = engineMotorPinLA;
      engineMotorPinLA = engineMotorPinLB;
      engineMotorPinLB = temp;
    }
    if (reverseEngineR)
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

void setRotatePWM(int enginePWM, int rotate, double PWMScare = 1) // 差速车专用计算两个电机出力
{
  double val = rotate * 2 - 100;
  Serial.printf("val=%d\n", val);
  int lmotor = enginePWM + (val / 100 * chasuPWM * PWMScare);
  int rmotor = enginePWM - (val / 100 * chasuPWM * PWMScare);
  if (lmotor < 0)
  {
    ledcWrite(5, -lmotor);
    ledcWrite(4, 0);
  }
  else
  {
    ledcWrite(4, lmotor);
    ledcWrite(5, 0);
  }

  if (rmotor < 0)
  {
    ledcWrite(7, -rmotor);
    ledcWrite(6, 0);
  }
  else
  {
    ledcWrite(6, rmotor);
    ledcWrite(7, 0);
  }
  Serial.printf("rotate=%d lmotor=%d  rmotor=%d\n", rotate, lmotor, rmotor);
}

void connectServer()
{
  digitalWrite(2, LOW);
  digitalWrite(engineMotorPinLA, LOW);
  digitalWrite(engineMotorPinLB, LOW);
  digitalWrite(engineMotorPinRA, LOW);
  digitalWrite(engineMotorPinRB, LOW);
  client = WiFiClient();
  if (!client.connect(IPAddress(settings.serverIP), settings.serverPort))
  {
    Serial.println("connect failed,reconnecting...\n");
    return;
  }
  analogWrite(2, 150);
  char buffer[16];
  char sendbuf[16];
  int *pSendbuf = (int *)sendbuf;
  int timeout_delay = 2500; // 看门狗最大等待数据包时间
  int chasuVal = 50;        // 差速车的差速转向比例值
  int MotorState = 0;       // 车辆行驶状态 0=停止  1=前进  2=后退
  unsigned long timeout = millis() + timeout_delay;
  pSendbuf[0] = 10;
  long long *tmppll = (long long *)&pSendbuf[1]; // 身份码是64位的，转换指针
  tmppll[0] = settings.authId;
  client.write_P((const char *)sendbuf, 16); // 发送登录包
  while (client.connected() && WiFi.isConnected())
  {
    if (millis() > timeout)
    {
      Serial.println("server heartbeat timeout,reconnecting...");
      client.stop();
      return;
    }
    if (client.available())
    {
      client.readBytes(buffer, 16);
      int *p = (int *)buffer;
      if (p[0] == 1) // 控制车辆
      {
        Serial.printf("car run cmd=%d\n", p[1]);
        if (p[1] == 0) // 停车
        {
          MotorState = 0;
          ledcWrite(4, 0);
          ledcWrite(5, 0);
          if (!useServoRotate)
          {
            ledcWrite(6, 0);
            ledcWrite(7, 0);
          }
        }
        else if (p[1] == 1) // 前进
        {
          MotorState = 1;
          if (!useServoRotate)
          {
            setRotatePWM(engineSpeedPWM, chasuVal);
          }
          else
          {
            ledcWrite(4, engineSpeedPWM);
            ledcWrite(5, 0);
          }
        }
        else if (p[1] == 2) // 后退
        {
          MotorState = 2;
          if (!useServoRotate)
          {
            setRotatePWM(-engineSpeedPWM, chasuVal);
          }
          else
          {
            ledcWrite(5, engineSpeedPWM);
            ledcWrite(4, 0);
          }
        }
        else if (p[1] == 3) // 左转（无比例转向的车）
        {
          if (useServoRotate)
          {
            ledcWrite(6, rotateMotorPWM);
            ledcWrite(7, 0);
          }
        }
        else if (p[1] == 4) // 右转（无比例转向的车）
        {
          if (useServoRotate)
          {
            ledcWrite(7, rotateMotorPWM);
            ledcWrite(6, 0);
          }
        }
        else if (p[1] == 5) // 回中（无比例转向的车）
        {
          if (useServoRotate)
          {
            ledcWrite(6, 0);
            ledcWrite(7, 0);
          }
        }
        else if (p[1] == 6) // 转向比例(50回中,0左转,100右转)
        {
          if (useServoRotate)
          {
            int val = p[2];
            if (val >= 0 && val <= 100)
            {
              if (val == 50)
              {
                rotateServo.write(centServoDeg);
                Serial.printf("turn center\n");
              }
              else if (val < 50)
              {
                int deg = (50.0f - val) / 50.0f * turnLeftServoDeg + centServoDeg;
                rotateServo.write(deg);
                Serial.printf("turn left val=%d,deg=%d\n", val, deg);
              }
              else if (val > 50)
              {
                int deg = (val - 50.0f) / 50.0f * turnRightServoDeg + centServoDeg;
                rotateServo.write(deg);
                Serial.printf("turn right val=%d,deg=%d\n", val, deg);
              }
            }
          }
          else // 差速车
          {
            chasuVal = p[2];
            if (MotorState == 0)
            {
              setRotatePWM(0, chasuVal, 2);
            }
            else if (MotorState == 1)
            {
              setRotatePWM(engineSpeedPWM, chasuVal, 1);
            }
            else if (MotorState == 2)
            {
              setRotatePWM(-engineSpeedPWM, chasuVal, -1);
            }
          }
        }
      }
      else if (p[0] == 2) // 连接测试
      {
        timeout = millis() + timeout_delay; // 喂狗
        memset(sendbuf, 0, 16);
        float *sendpf = (float *)sendbuf;
        pSendbuf[0] = 2;
        sendpf[1] = getVBat();
        
        client.write_P(sendbuf, 16); // 发送心跳包回复
        //Serial.println("connection check called");
      }
      else if (p[0] == 3) // PWM调速
      {
        engineSpeedPWM = p[1];
        rotateMotorPWM = p[2];
        Serial.printf("pwm config enginePWM=%d,rotatePWM=%d\n", engineSpeedPWM, rotateMotorPWM);
      }
      else if (p[0] == 4) // 电压计ADC校准
      {
        float adcVal = prev_read_raw_vol; //使用缓存值，之前直接调用sample_vbat采样会偏低，可能是多线程同时访问adc导致
        float *pfloat = (float *)p;
        settings.adcOffset = pfloat[1] - adcVal;
        EEPWriteBlock(4, sizeof(CarSettings), (char *)&settings); // 保存设置，将设置信息结构体存入EEPROM
        memset(sendbuf, 0, 16);
        pSendbuf[0] = 3;
        pSendbuf[1] = 1;
        client.write_P(sendbuf, 16);
        Serial.printf("rawVol=%.2f  adcOffset=%.2f\n", adcVal, settings.adcOffset);
      }
      else if (p[0] == 5) // 转向舵机校准
      {
        if (p[1] == 1) // 测试舵机角度
        {
          int deg = p[2];
          if (deg >= 0 && deg <= 180)
          {
            rotateServo.write(deg);
          }
        }
        else if (p[1] == 2) // 保存设置
        {
          short *pshort = (short *)&p[2];
          int cent = pshort[0];
          int left = pshort[1];
          int right = pshort[2];
          // 保留，暂未实现，后面再搞
        }
      }
      else if (p[0] == 6) // 炮塔控制（或其他外设）
      {
        if (p[1] == 1) // 俯仰轴舵机移动
        {
          shootPitchServo.write(p[2]);
          Serial.printf("shoot pitch degree:%d\n", p[2]);
        }
        else if (p[1] == 10) // 开火控制
        {
          if (p[2] == 1) // 点射
          {
            digitalWrite(shootFireMOSPin, HIGH);
            delay(400);
            digitalWrite(shootFireMOSPin, LOW);
          }
          else if (p[2] == 2) // 连发开启
          {
            digitalWrite(shootFireMOSPin, HIGH);
          }
          else if (p[2] == 3) // 停止连发
          {
            digitalWrite(shootFireMOSPin, LOW);
          }
          Serial.printf("shoot control,command=%d\n", p[2]);
        }
      }
      else if (p[0] == 7) // 自动关机/休眠相关设置
      {
        if (p[1] == 1) // 设置自动关机电压
        {
          
          float *pfloat = (float *)p;
          float targetShutdownVol = pfloat[2];
          settings.shutdownVol = targetShutdownVol;
          EEPWriteBlock(4, sizeof(CarSettings), (char *)&settings); // 写入ROM存储
          memset(sendbuf, 0, 16);
          pSendbuf[0] = 3;             // 3=小车消息类型
          pSendbuf[1] = 2;             // 2=消息->设置自动关机电压成功
          client.write_P(sendbuf, 16); // 回复客户端
          Serial.printf("关机电压=%.2f\n",targetShutdownVol);
        }
        else if (p[1] == 2) // 进入深度睡眠+定时器唤醒
        {
          int seconds = p[2];
          //回复给客户端我在5秒后开始休眠
          memset(sendbuf, 0, 16);
          pSendbuf[0] = 3;             // 3=小车消息类型
          pSendbuf[1] = 3;             // 3=消息->即将开始休眠
          client.write_P(sendbuf, 16);
          delay(5000);
          shutdown_and_deep_sleep(seconds * 1000000ULL);
          Serial.printf("关机电压=%d\n",seconds);
        }
      }
    }
  }
}
bool connectWifi()
{
  WiFi.begin(settings.wifiSSID, settings.wifiPassword);
  Serial.print("connecting.");
  int tryCount = 0;
  while (WiFi.status() != WL_CONNECTED && tryCount < 20)
  {
    delay(950);
    digitalWrite(2, HIGH);
    delay(50);
    digitalWrite(2, LOW);
    Serial.print(".");
    ++tryCount;
  }
  return WiFi.status() == WL_CONNECTED;
}
void sendPage()
{
  web.send(200, "text/html", settingHtml);
}
void onConfig()
{
  strcpy(settings.wifiSSID, web.arg("ssid").c_str());
  strcpy(settings.wifiPassword, web.arg("pass").c_str());
  IPAddress serverip;
  serverip.fromString(web.arg("ip"));
  settings.serverIP = serverip;
  settings.serverPort = atoi(web.arg("port").c_str());
  settings.authId = strtoll(web.arg("auth").c_str(), NULL, 10);
  EEPWriteInt(0, trueEEPFlag);
  EEPWriteBlock(4, sizeof(CarSettings), (char *)&settings);
  web.send(200, "text/plain", "success");
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
  web.on("/config", onConfig);
  web.begin(80);
  dns.start(53, "*", selfAddr);
}

void setup()
{

  initGpio();           // 引脚初始化
  initBatteryManager(); // 初始化电源管理

  if (readSetting())
  {
    Serial.printf("setting:\nssid=%s\npass=%s\nserver=%s\nport=%d\nauthId=%lld\nadcOffset=%.2f\nshutdownVol=%.2f\n", settings.wifiSSID, settings.wifiPassword, serverAddr.toString().c_str(), settings.serverPort, settings.authId, settings.adcOffset, settings.shutdownVol);
    // 配置过，按照设置连接wifi和服务器
    if (connectWifi())
    {
      // 连接成功，开始连接服务器
      Serial.println("connect wifi successed\nconnecting server.");
      digitalWrite(2, HIGH);
      delay(500);
      digitalWrite(2, LOW);
      rotateServo.write(centServoDeg); // 回中转向架
    }
    else
    {
      // 连接失败，开启配网模式
      apSetting();
    }
  }
  else
  {
    // 没有配置过，开启热点进入配网模式
    apSetting();
  }
}

void loop()
{
  if (configMode)
  {
    dns.processNextRequest();
    web.handleClient();
  }
  else
  {
    connectServer();
  }
}