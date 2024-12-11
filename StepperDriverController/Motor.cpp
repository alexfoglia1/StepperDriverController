#include "Motor.h"
#include "Board.h"

#include <Arduino.h>

int stepCount = 0;

#ifdef BOARD_REV_B
void motorPower(bool powerOn)
{
  if (powerOn)
  {
    //Serial.println("HIGH");
    digitalWrite(ENA_P, HIGH);
    digitalWrite(ENA_M, HIGH);
  }
  else
  {
    //Serial.println("LOW");
    digitalWrite(ENA_P, LOW);
    digitalWrite(ENA_M, LOW);
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
