#include <Arduino.h>
#include <EEPROM.h>

void EEPWriteInt(int addr,int val);
void EEPWriteBlock(int addr,int size,const char* src);
void EEPReadBlock(int addr,int size,char* dst);
int EEPReadInt(int addr);