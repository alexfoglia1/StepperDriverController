#ifndef MOTOR_H
#define MOTOR_H
#include "Board.h"

void motorStep(int stepDelay);
#ifdef BOARD_REV_B
void motorPower(bool powerOn);
#endif

extern int stepCount;

#endif
