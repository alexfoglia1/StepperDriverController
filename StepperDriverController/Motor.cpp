#include "Motor.h"
#include "Board.h"

#include <Arduino.h>

int stepCount = 0;

#ifdef BOARD_REVB
void motorPower(bool powerOn)
{
  if (powerOn)
  {
    PORTB |= (RESET_MASK | SLEEP_MASK);
  }
  else
  {
    PORTB &= ~(RESET_MASK | SLEEP_MASK);
    stepCount = 0;
  }
}
#endif


void motorStep(int stepDelay)
{
  PORTB |= STEP_M_MASK;
  delayMicroseconds(5);
  PORTB &= ~STEP_M_MASK;
  delayMicroseconds(stepDelay - 5);
    
  stepCount += 1;
}
