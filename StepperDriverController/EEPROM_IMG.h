#define EEPROM_SIZE 15

typedef union
{
  struct
  {
    uint16_t velErog;
    uint16_t distSpellic;
    uint16_t tempoStart;
    uint16_t ignoreF1;
    uint8_t ctrlMode;
    uint16_t minDelayMicros;
    uint16_t maxDelayMicros;
    uint8_t  curDirection;
    uint8_t  checksum;
  }__attribute__((packed)) Values;
  byte Bytes[EEPROM_SIZE];
} EEPROM_IMG;

extern EEPROM_IMG eepromParams;
