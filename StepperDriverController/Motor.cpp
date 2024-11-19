#include "Motor.h"
#include "Board.h"

#include <Arduino.h>

int stepCount = 0;

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


void motorStep(int stepDelay)
{
  //digitalWrite(STEP, HIGH);
  PORTB |= STEP_MASK;
  delayMicroseconds(stepDelay);
  //digitalWrite(STEP, LOW);
  PORTB &= ~STEP_MASK;
  delayMicroseconds(stepDelay);
    
  stepCount += 1;
}