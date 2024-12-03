#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include <stdint.h>

#define SYNC_CHAR 0xFF

typedef union
{
    struct
    {
        uint8_t get_min_delay   :1;
        uint8_t get_max_delay   :1;
        uint8_t set_min_delay   :1;
        uint8_t set_max_delay   :1;
        uint8_t set_default     :1;
        uint8_t motor_move      :1;
        uint8_t invert_dir      :1;
        uint8_t zero          :1;
    } Bits;
    uint8_t Byte;
} maint_header_byte_1;

typedef union
{
    struct
    {
        uint8_t get_btn_state   :1;
        uint8_t get_analog2_in  :1;
        uint8_t get_analog4_in  :1;
        uint8_t get_btn_12_val  :1;
        uint8_t get_btn_345_val :1;
        uint8_t set_btn_val     :1;
        uint8_t zero            :3;
    } Bits;
    uint8_t Byte;
} maint_header_byte_2;

typedef union
{
    struct
    {
        maint_header_byte_1 byte_1;
        maint_header_byte_2 byte_2;
    } Bytes;
    uint16_t Word;
} maint_header_t;


typedef enum
{
  WAIT_SYNC = 0,
  WAIT_HEADER_1,
  WAIT_HEADER_2,
  WAIT_DATA
} maint_rx_state_t;

void MAINT_Handler(bool* eepromUpdate, bool* stepperMoving);


#endif
