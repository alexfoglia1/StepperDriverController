#define EEPROM_SIZE 26

typedef union
{
  struct
  {
    float velErog;
    float distSpellic;
    float tempoStart;
    uint32_t ctrlMode;
    uint32_t minDelayMicros;
    uint32_t maxDelayMicros;
    uint8_t curDirection;
    uint8_t checksum;
  }__attribute__((packed)) Values;
  byte Bytes[EEPROM_SIZE];
} EEPROM_IMG;

extern EEPROM_IMG eepromParams;
