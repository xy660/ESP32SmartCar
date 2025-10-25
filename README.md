# ESP32SmartCar
ESP32远程控制小车，支持公网远程控制，手机app遥控

## Features

- 支持多种车架结构（阿克曼车，差速车，玩具直流电机转向架车）
- WIFI公网远程控制
- 可扩展外设，如舵机等，可以扩展水弹炮塔或机械臂
- 动态电池电压检测

## 材料清单

- l298n或类似H桥电机驱动器（1-2个，根据结构选择）
- ESP32开发板（任意型号，只要引脚数量足够）
- 杜邦线若干
- 5V DCDC模块若干（用于外设供电可选）
- 舵机（可选）
- 减速直流电机
- 车架（可以使用市面上常见的智能车车架或者玩具车改装）
- 分压电阻（用于ADC分压检测电池电压）

## 电路连接图

默认代码连接图，如需修改引脚需要修改源码

![how_to_build](https://github.com/xy660/ESP32SmartCar/raw/main/imgs/how_to_build.bmp)

```plain

GPIO27---l298n.IN1
GPIO26---l298n.IN2
GPIO14---l298n.IN3
GPIO12---l298n.IN4
GPIO13---转向架舵机PWM信号
GPIO17---外设mos管/继电器控制
GPIO16---外设舵机PWM信号
GPIO34---电源电压输入（需要分压至ESP32 ADC可接受范围内）
GPIO25---外设电源控制MOS管

```

## 部署步骤

1. 修改main.cpp头部的引脚定义，确保与你连接的外设一致，修改ADCDelta和ADCScale属性，与你的分压电阻倍率适配，确保ADC换算电压线性一致

2. 编译PIO项目，烧录固件到ESP32开发板
   
3. 根据服务器OS编译服务端部署到服务器，在服务端程序目录下放置ESPRCAuth.txt作为登录白名单

4. 上电，第一次上电会发射一个AP，手机连接AP自动弹出配置网页，按照你的WIFI接入点填写，保存后自动重启连接网络

5. 编译手机app项目或者PC winforms上位机项目，通过服务器IP和身份码连接到小车，即可开始控制

## 成品示例图

### 4G远程小车（需要搭配网络摄像头和USB随身WIFI使用）

![1](https://github.com/xy660/ESP32SmartCar/raw/main/imgs/1.jpg)

### 水弹战车

![2](https://github.com/xy660/ESP32SmartCar/raw/main/imgs/2.jpg)



