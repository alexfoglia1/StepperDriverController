#ifndef BOARD_H
#define BOARD_H

#include <avr/io.h>

#define MAJOR_V '1'
#define MINOR_V '4'
#define STAGE_V 'B'
#define SW_PN   'F'

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
#define PHOTO_1 4
#define PHOTO_2 5

#define RESET_MASK   0x10
#define SLEEP_MASK   0x08
#define STEP_MASK    0x04
#define DIR_MASK     0x02
#define PROBE_MASK   0x01
#define PHOTO_1_MASK 0x10
#define PHOTO_2_MASK 0x20

#define BUTTON_IN A2
#define BUTTON_1 1014
#define BUTTON_2 1004
#define BUTTON_3 975
#define BUTTON_4 890
#define BUTTON_5 853
#define BTN_TOL 4

#endif
