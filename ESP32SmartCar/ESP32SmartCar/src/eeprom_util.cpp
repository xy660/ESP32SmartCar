#include <Arduino.h>
#include <EEPROM.h>

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