#ifndef BOARD_H
#define BOARD_H

#define BOARD_REV_A

#include <avr/io.h>

#define MAJOR_V '1'
#define MINOR_V '5'
#define STAGE_V 'B'
#define SW_PN   'F'

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define DIR_P     12
#define DIR_M     11
#define STEP_P    10
#define STEP_M    9
#define PROBE     8
#define PHOTO_1   4
#define PHOTO_2   5

#define DIR_P_MASK   0x10
#define DIR_M_MASK   0x08
#define STEP_P_MASK  0x04
#define STEP_M_MASK  0x02
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
