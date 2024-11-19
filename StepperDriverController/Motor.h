#ifndef MOTOR_H
#define MOTOR_H

void motorStep(int stepDelay);
void motorPower(bool powerOn);

extern int stepCount;

#endif