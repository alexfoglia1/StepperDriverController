#include "Maintenance.h"
#include "EEPROM_IMG.h"
#include "Board.h"
#include "Motor.h"

#include <Arduino.h>
#include <string.h>

maint_rx_state_t rx_state = WAIT_SYNC;

uint8_t rxBuffer[2] = {0, 0};


void data_ingest(maint_header_t* rxHeader, bool* eepromUpdate, bool* stepperMoving)
{
  Serial.write(SYNC_CHAR);
  Serial.write(rxHeader->Bytes.byte_1.Byte);
  Serial.write(rxHeader->Bytes.byte_2.Byte);
      
  if (rxHeader->Bytes.byte_1.Bits.get_min_delay)
  {
    uint16_t minDelayMicros = eepromParams.Values.minDelayMicros;
    Serial.write((minDelayMicros & 0xFF00) >> 8);
    Serial.write((minDelayMicros & 0x00FF));
  }
  if (rxHeader->Bytes.byte_1.Bits.get_max_delay)
  {
    uint16_t maxDelayMicros = eepromParams.Values.maxDelayMicros;
    Serial.write((maxDelayMicros & 0xFF00) >> 8);
    Serial.write((maxDelayMicros & 0x00FF));
  }
  if (rxHeader->Bytes.byte_1.Bits.set_default)
  {
    memcpy(eepromParams.Bytes, eepromDefaults.Bytes, EEPROM_SIZE);
    *eepromUpdate = true;
  }
  if (rxHeader->Bytes.byte_1.Bits.motor_move)
  {
    eepromParams.Values.ctrlMode = 1;
    *stepperMoving = !*stepperMoving;
    motorPower(*stepperMoving);
  }
  if (rxHeader->Bytes.byte_1.Bits.invert_dir)
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
  if (rxHeader->Bytes.byte_1.Bits.set_min_delay)
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
  if (rxHeader->Bytes.byte_1.Bits.set_max_delay)
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

  if (rxHeader->Bytes.byte_2.Bits.get_btn_state)
  {
    extern uint8_t buttonsState;
    Serial.write(buttonsState);
  }
  if (rxHeader->Bytes.byte_2.Bits.get_analog2_in)
  {
    uint16_t aIn = analogRead(BUTTON_IN);
    Serial.write((aIn & 0xFF00) >> 8);
    Serial.write((aIn & 0x00FF));
    
  }
  if (rxHeader->Bytes.byte_2.Bits.get_analog5_in)
  {
    // Unimplemented FULL
    Serial.write(0x00);
    Serial.write(0x00);
  }
  if (rxHeader->Bytes.byte_2.Bits.get_btn_12_val)
  {
    uint16_t btn1 = eepromParams.Values.btn1;
    uint16_t btn2 = eepromParams.Values.btn2;
    Serial.write((btn1 & 0xFF00) >> 8);
    Serial.write((btn1 & 0x00FF));
    Serial.write((btn2 & 0xFF00) >> 8);
    Serial.write((btn2 & 0x00FF));
    
  }
  if (rxHeader->Bytes.byte_2.Bits.get_btn_345_val)
  {
    uint16_t btn3 = eepromParams.Values.btn3;
    uint16_t btn4 = eepromParams.Values.btn4;
    uint16_t btn5 = eepromParams.Values.btn5;
    Serial.write((btn3 & 0xFF00) >> 8);
    Serial.write((btn3 & 0x00FF));
    Serial.write((btn4 & 0xFF00) >> 8);
    Serial.write((btn4 & 0x00FF));
    Serial.write((btn5 & 0xFF00) >> 8);
    Serial.write((btn5 & 0x00FF));
  }
  if (rxHeader->Bytes.byte_2.Bits.set_btn_val)
  {
    uint8_t idx = 0;
    uint16_t expectedBytes[10];
    while (1)
    {
      while (!Serial.available());
      while (Serial.available() && idx < 10)
      {
        expectedBytes[idx] = Serial.read() & 0xFF;
        Serial.write(expectedBytes[idx]);
        idx += 1;
      }
      if (idx == 10) break;
    }

    eepromParams.Values.btn1 = (expectedBytes[0] << 8 | expectedBytes[1]);
    eepromParams.Values.btn2 = (expectedBytes[2] << 8 | expectedBytes[3]);
    eepromParams.Values.btn3 = (expectedBytes[4] << 8 | expectedBytes[5]);
    eepromParams.Values.btn4 = (expectedBytes[6] << 8 | expectedBytes[7]);
    eepromParams.Values.btn5 = (expectedBytes[8] << 8 | expectedBytes[9]);

    *eepromUpdate = true;
  }
  if (rxHeader->Bytes.byte_2.Bits.get_sw_ver)
  {
    Serial.write(MAJOR_V);
    Serial.write(MINOR_V);
    Serial.write(STAGE_V);
    Serial.write(SW_PN);
  }
    
}


void update_fsm(uint8_t byteIn, bool* eepromUpdate, bool* stepperMoving)
{
  if (rx_state == WAIT_SYNC && byteIn == SYNC_CHAR)
  {
    rx_state = WAIT_HEADER_1;
  }
  else if (rx_state == WAIT_HEADER_1)
  {
    maint_header_byte_1* pByte1 = (maint_header_byte_1*)(&byteIn);
    if (pByte1->Bits.zero == 0)
    {
      rxBuffer[0] = byteIn;
      rx_state = WAIT_HEADER_2;
    }
    else
    {
      rx_state = WAIT_SYNC;
    }
  }
  else if (rx_state == WAIT_HEADER_2)
  {
    maint_header_byte_2* pByte2 = (maint_header_byte_2*)(&byteIn);
    if (pByte2->Bits.zero == 0)
    {
      rxBuffer[1] = byteIn;
      data_ingest((maint_header_t*) rxBuffer, eepromUpdate, stepperMoving);
    }
    rx_state = WAIT_SYNC;
  }
}


void MAINT_Handler(bool* eepromUpdate, bool* stepperMoving)
{
  if (Serial.available())
  {
    uint8_t byteIn = (uint8_t)(Serial.read() & 0xFF);

    update_fsm(byteIn, eepromUpdate, stepperMoving);
  }
}
