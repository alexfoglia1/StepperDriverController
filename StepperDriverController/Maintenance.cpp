#include "Maintenance.h"
#include "EEPROM_IMG.h"
#include "Board.h"
#include "Motor.h"

#include <Arduino.h>
#include <string.h>

maint_rx_state_t rx_state = WAIT_SYNC;


void data_ingest(maint_rx_byte_t rxByte, bool* eepromUpdate, bool* stepperMoving)
{
  if (rxByte.Bits.get_min_delay)
  {
    Serial.write(SYNC_CHAR);
    Serial.write(rxByte.Byte);
    uint16_t minDelayMicros = eepromParams.Values.minDelayMicros;
    Serial.write((minDelayMicros & 0xFF00) >> 8);
    Serial.write((minDelayMicros & 0x00FF));
  }
  if (rxByte.Bits.get_max_delay)
  {
    Serial.write(SYNC_CHAR);
    Serial.write(rxByte.Byte);
    uint16_t maxDelayMicros = eepromParams.Values.maxDelayMicros;
    Serial.write((maxDelayMicros & 0xFF00) >> 8);
    Serial.write((maxDelayMicros & 0x00FF));
  }
  if (rxByte.Bits.set_default)
  {
    memcpy(eepromParams.Bytes, eepromDefaults.Bytes, EEPROM_SIZE);
    *eepromUpdate = true;
  }
  if (rxByte.Bits.motor_move)
  {
    eepromParams.Values.ctrlMode = 1;
    *stepperMoving = !*stepperMoving;
    motorPower(*stepperMoving);
  }
  if (rxByte.Bits.invert_dir)
  {
    eepromParams.Values.curDirection = !eepromParams.Values.curDirection;
    if (eepromParams.Values.curDirection)
    {
      PORTB |= DIR_MASK;
    }
    else
    {
      PORTB &= ~DIR_MASK;
    }

    *eepromUpdate = true;
  }
  if (rxByte.Bits.set_min_delay)
  {
    uint16_t MSB, LSB;
    if (Serial.available())
    {
      MSB = Serial.read() & 0xFF;
      if (Serial.available())
      {
        LSB = Serial.read() & 0xFF;
        eepromParams.Values.minDelayMicros = (MSB << 8 | LSB);
        for (int i = 0; i < MAX_VEL_INT + 1; i++)
        {
          VEL_TO_STEP_DELAY[MAX_VEL_INT - i] = map(i, 0, MAX_VEL_INT, eepromParams.Values.minDelayMicros, eepromParams.Values.maxDelayMicros);
        }

        *eepromUpdate = true;
      }
    }
  }
  if (rxByte.Bits.set_max_delay)
  {
    uint16_t MSB, LSB;
    if (Serial.available())
    {
      MSB = Serial.read() & 0xFF;
      if (Serial.available())
      {
        LSB = Serial.read() & 0xFF;
        eepromParams.Values.maxDelayMicros = (MSB << 8 | LSB);
        for (int i = 0; i < MAX_VEL_INT + 1; i++)
        {
          VEL_TO_STEP_DELAY[MAX_VEL_INT - i] = map(i, 0, MAX_VEL_INT, eepromParams.Values.minDelayMicros, eepromParams.Values.maxDelayMicros);
        }
        *eepromUpdate = true;
      }
    }
  }  
}


void update_fsm(maint_rx_byte_t rxByte, bool* eepromUpdate, bool* stepperMoving)
{
  if (rx_state == WAIT_SYNC && rxByte.Byte == SYNC_CHAR)
  {
    rx_state = WAIT_DATA;
  }
  else
  {
    if (rxByte.Bits.zero == 0)
    {
      data_ingest(rxByte, eepromUpdate, stepperMoving);
    }
    else
    {
      rx_state = WAIT_SYNC;
    }
  }
}


void MAINT_Handler(bool* eepromUpdate, bool* stepperMoving)
{
  if (Serial.available())
  {
    maint_rx_byte_t rxMaintByte;
    rxMaintByte.Byte = (uint8_t)(Serial.read() & 0xFF);

    update_fsm(rxMaintByte, eepromUpdate, stepperMoving);
  }
}