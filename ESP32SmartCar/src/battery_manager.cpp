#include <Arduino.h>
#include "GPIOInit.h"

float prev_read_vol = 0; //上次读取的电压（校准）
float prev_read_raw_vol = 0; //上次读取的原始电压


float sample_raw_vbat()
{
  //return analogRead(VBatInputPin) * ADCDelta * 5; 
  //之前的单次采样不要了
  uint32_t average = 0;
  for(int i = 0;i < 100;i++){ //ESP32的ADC精度比较垃圾，暴力采样100次然后取平均值
    average += analogRead(VBatInputPin);
    delayMicroseconds(100); //微小延迟隔开采样点
  }
  return (float)average / 100.0f * ADCDelta * ADCScale;
}
float internal_get_vbat()  //获取经过修正的电压值（含偏移量和缩放倍率）
{
    prev_read_raw_vol = sample_raw_vbat();
    //Serial.printf("111 vb=%f\nraw=%f\noffset=%f\n\n",prev_read_raw_vol + settings.adcOffset,prev_read_raw_vol,settings.adcOffset);
    return prev_read_raw_vol + settings.adcOffset;
}

float getVBat() //提供给主函数的获取电压方法
{
  return prev_read_vol;
}

void shutdown_and_deep_sleep(uint64_t sleep_us = 0){
    digitalWrite(powerENPin,LOW); //关闭外设供电
    
    digitalWrite(engineMotorPinLA,LOW);
    digitalWrite(engineMotorPinLB,LOW);
    digitalWrite(engineMotorPinRA,LOW);
    digitalWrite(engineMotorPinLB,LOW);
    pinMode(engineMotorPinLA,INPUT);
    pinMode(engineMotorPinLB,INPUT);
    pinMode(engineMotorPinRA,INPUT);
    pinMode(engineMotorPinRB,INPUT); //设置电机信号引脚为输入，防止驱动模块偷电
    delay(500);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if(sleep_us != 0){ //如果有休眠时间那就设置定时器唤醒
        esp_sleep_enable_timer_wakeup(sleep_us);
    }
    //否则无限期休眠直到重新上电（关机）
    esp_deep_sleep_start(); 
}

void battery_man_proc(void* context){
    delay(3000); //延迟3秒启动，防止反复重启
    Serial.printf("battery manager: adc_pin=%d,adcOffset=%.2f\n",VBatInputPin,settings.adcOffset);
    while(true){
        prev_read_vol = internal_get_vbat();

        //判断是否电压抵达截至电压
        if(settings.shutdownVol != 0.0f && prev_read_vol < settings.shutdownVol){
            shutdown_and_deep_sleep();
        }

        vTaskDelay(500 / portTICK_PERIOD_MS); //500ms监控一次电压
    }
}

void initBatteryManager(){
    xTaskCreate(&battery_man_proc,"battery_manager",1024 * 4,NULL,16,NULL);
}