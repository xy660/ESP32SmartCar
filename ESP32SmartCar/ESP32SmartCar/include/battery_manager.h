#include <Arduino.h>

float getVBat();
void initBatteryManager();
float sample_raw_vbat();
void shutdown_and_deep_sleep(uint64_t sleep_us = 0);

extern float prev_read_raw_vol; //上次读取到的原始电压值（没有经过校准）