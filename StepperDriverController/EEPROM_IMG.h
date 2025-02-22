#ifndef EEPROM_IMG_H
#define EEPROM_IMG_H

#include <stdint.h>

#define EEPROM_SIZE 25
#define MAX_VEL_INT 500
#define MAX_TEMPO_INT 65535
#define EEPROM_ADDR 0

typedef union
{
  struct
  {
    uint16_t velErog;
    uint16_t distSpellic;
    uint16_t tempoStart;
    uint16_t ignoreF1;
    uint8_t  ctrlMode;
    uint16_t minDelayMicros;
    uint16_t maxDelayMicros;
    uint8_t  curDirection;
    uint16_t btn1;
    uint16_t btn2;
    uint16_t btn3;
    uint16_t btn4;
    uint16_t btn5;
    uint8_t  checksum;
  }__attribute__((packed)) Values;
  uint8_t Bytes[EEPROM_SIZE];
} EEPROM_IMG;

extern EEPROM_IMG eepromParams;
extern EEPROM_IMG eepromDefaults;
extern int VEL_TO_STEP_DELAY[MAX_VEL_INT + 1];

extern void writeEepromParams();
extern void readEepromParams();

#endif
