#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include <stdint.h>

#define SYNC_CHAR 0xFF

typedef union
{
  struct
  {
    uint8_t get_min_delay :1;
    uint8_t get_max_delay :1;
    uint8_t set_min_delay :1;
    uint8_t set_max_delay :1;
    uint8_t set_default   :1;
    uint8_t motor_move    :1;
    uint8_t invert_dir    :1;
    uint8_t zero          :1;
  } Bits;
  uint8_t Byte;
} maint_rx_byte_t;


typedef enum
{
  WAIT_SYNC = 0,
  WAIT_DATA
} maint_rx_state_t;

void MAINT_Handler(bool* eepromUpdate, bool* stepperMoving);


#endif