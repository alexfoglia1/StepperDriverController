#ifndef BOARD_H
#define BOARD_H

#include <avr/io.h>

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define RESET  12
#define SLEEP  11
#define STEP   10
#define DIR     9
#define PROBE   8
#define PHOTO_1 7
#define PHOTO_2 4

#define RESET_MASK   0x10
#define SLEEP_MASK   0x08
#define STEP_MASK    0x04
#define DIR_MASK     0x02
#define PROBE_MASK   0x01
#define PHOTO_1_MASK 0x80
#define PHOTO_2_MASK 0x10

#define BUTTON_IN A2
#define BUTTON_1 917
#define BUTTON_2 994
#define BUTTON_3 1015
#define BUTTON_4 977
#define BTN_TOL  2

#endif