#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#include "EEPROM_IMG.h"
#include "Maintenance.h"
#include "Board.h"
#include "Motor.h"

#define BTN_LONG_PRESSURE_MILLIS 1000
#define BACKLIGHT_TIMEOUT_MILLIS 5000

#define LCD_BLANK_LINE "                    "


typedef enum
{
  MAIN_MENU = 0,
  MENU_VEL,
  MENU_DIST_SPELLIC,
  MENU_START,
  MENU_IGNORE_F1
} UserState;


typedef enum
{
  BTN_1_PRESSED = 0,
  BTN_2_PRESSED,
  BTN_3_PRESSED,
  BTN_4_PRESSED,
  BTN_1_LONGPRS,
  BTN_2_LONGPRS,
  BTN_3_LONGPRS,
  BTN_4_LONGPRS,
  BTN_1_RELEASE,
  BTN_2_RELEASE,
  BTN_3_RELEASE,
  BTN_4_RELEASE,
  NO_INPUT
} UserEvent;


typedef enum
{
  MODE_AUTO = 0,
  MODE_MANUAL
} CtrlMode;


LiquidCrystal_I2C lcd(0x27,20,2);
bool EEPROM_UPDATE = false;
bool lcdBlink = false;
int lcdBlinkTime = 0;
int buttonStates[4] = {0, 0, 0, 0};
int buttonFirstPressure[4] = {0, 0, 0, 0};
int buttonLastPressure[4] = {0, 0, 0, 0};
int ignoreRelease[4] = {0, 0, 0, 0};
EEPROM_IMG eepromParams;
EEPROM_IMG eepromDefaults = {MAX_VEL_INT, 1000, 1000, 100, MODE_AUTO, 1000, 5000, 0, 0};
uint16_t nextVel = MAX_VEL_INT;
uint16_t nextDistSpellic = 1000;
uint16_t nextTempoStart = 1000;
uint16_t nextIgnoreF1 = 10;
int lastUserInput = 0;
UserState userState = MAIN_MENU;
int menuIndex = 0;
const char* menuLabels[4] = {"VELO EROGA ",
                             "DIST SPELL",
                             "TEMP START",
                             "IGNR FOTC1"
                            };
char lcdLastPrint[20];
char lcdLastMode[20];

int VEL_TO_STEP_DELAY[MAX_VEL_INT + 1];
bool isBacklight = false;
int millisStart = 0;
int millisStop = 0;
bool isStepperMoving = false;
bool isFirstLoop = true;
int countBtn3Pressed = 0;
int countBtn4Pressed = 0;
bool isPhoto1 = false;
bool isPhoto2 = false;
bool startReceived = false;
bool stopReceived = false;
bool btn3PressedOnManual = false;


byte checksum(byte* bytes, int nBytes)
{
  byte res = 0;
  for (int i = 0; i < nBytes; i++)
  {
    res ^= bytes[i];
  }

  return res;
}


void writeEepromParams()
{
  byte curChecksum = checksum(eepromParams.Bytes, EEPROM_SIZE - 1);
  eepromParams.Values.checksum = curChecksum;
  for (uint32_t addr = EEPROM_ADDR; addr < EEPROM_ADDR + EEPROM_SIZE; addr++)
  {
    EEPROM.write(addr, eepromParams.Bytes[addr - EEPROM_ADDR]);
  }  
}


void readEepromParams()
{
  eepromDefaults.Values.checksum = checksum(eepromDefaults.Bytes, EEPROM_SIZE - 1);
  for (uint32_t addr = EEPROM_ADDR; addr < EEPROM_ADDR + EEPROM_SIZE; addr++)
  {
    eepromParams.Bytes[addr - EEPROM_ADDR] = EEPROM.read(addr);
  }
  //Serial.print("EEPROM velErog: "); Serial.println(eepromParams.Values.velErog);
  //Serial.print("EEPROM distSpellic: "); Serial.println(eepromParams.Values.distSpellic);
  //Serial.print("EEPROM tempoStart: "); Serial.println(eepromParams.Values.tempoStart);
  //Serial.print("EEPROM ctrlMode: "); Serial.println(eepromParams.Values.ctrlMode);
  //Serial.print("EEPROM checksum: "); Serial.println(eepromParams.Values.checksum);  
  
  //Serial.print("EEPROM minDelay: "); Serial.println(eepromParams.Values.minDelayMicros);
  //Serial.print("EEPROM maxDelay: "); Serial.println(eepromParams.Values.maxDelayMicros);  
  //Serial.print("EEPROM curDirection: "); Serial.println(eepromParams.Values.curDirection);  
  
  byte curChecksum = checksum(eepromParams.Bytes, EEPROM_SIZE - 1);
  //Serial.print("Calculated checksum: "); Serial.println(curChecksum);

  if (curChecksum != eepromParams.Values.checksum)
  {
    memcpy(eepromParams.Bytes, eepromDefaults.Bytes, EEPROM_SIZE);
    writeEepromParams();    
  }

  for (int i = 0; i < MAX_VEL_INT + 1; i++)
  {
    VEL_TO_STEP_DELAY[MAX_VEL_INT - i] = map(i, 0, MAX_VEL_INT, eepromParams.Values.minDelayMicros, eepromParams.Values.maxDelayMicros);
  }
}


void lcdPrint(char* prompt)
{
  if (strncmp(prompt, lcdLastPrint, min(strlen(prompt), 20)) != 0)
  {
    memset(lcdLastPrint, ' ', 20);
    memcpy(lcdLastPrint, prompt, min(strlen(prompt), 20));
    lcd.setCursor(0, 0);
    lcd.print(lcdLastPrint);
  }
}


void lcdPrintMode()
{
  const char* prompts[2] = {"MODO: AUTOMATICO", "MODO: MANUALE"};
  const char* prompt = prompts[eepromParams.Values.ctrlMode];

  if (strncmp(prompt, lcdLastMode, min(strlen(prompt), 20)) != 0)
  {
    lcd.setCursor(0, 1);
    lcd.print(LCD_BLANK_LINE);
    lcd.setCursor(0, 1);
    lcd.print(prompt);
    memcpy(lcdLastMode, prompt, min(strlen(prompt), 20));
    lcdBlink = false;
  }
  
}


void lcdClearMode()
{
  const char* prompt = LCD_BLANK_LINE;

  if (strncmp(prompt, lcdLastMode, min(strlen(prompt), 20)) != 0)
  {
    lcd.setCursor(0, 1);
    lcd.print(prompt);
    memcpy(lcdLastMode, prompt, min(strlen(prompt), 20));
    lcdBlink = true;
    lcdBlinkTime = millis();
  }
}


void lcdClearDisplay()
{
  lcd.setCursor(0, 0);
  lcd.print(LCD_BLANK_LINE);
  lcd.setCursor(0, 1);
  lcd.print(LCD_BLANK_LINE);
}


void inputValueToLcdString(const char* label, char prompt[20], uint16_t n, int nDec)
{
  memset(prompt, ' ', 20);
  uint16_t units = n % 10;
  uint16_t tens = (n / 10) % 10;
  uint16_t hundreds = (n / 100) % 10;
  uint16_t thousands = (n / 1000) % 10;
  if (nDec == 1)
    sprintf(prompt, "%s %d%d.%d", label, hundreds, tens, units); 
  else
    sprintf(prompt, "%s %d%d.%d%d", label, thousands, hundreds, tens, units); 
  //Serial.println(prompt);
}


void readUserButton(int curMillis)
{
  int valueRead = analogRead(BUTTON_IN);
  //Serial.println(valueRead);

  if (valueRead >= BUTTON_1 - BTN_TOL && valueRead <= BUTTON_1 + BTN_TOL)
  {
    if (buttonStates[0] == 0)
    {
      buttonFirstPressure[0] = curMillis;
      buttonFirstPressure[1] = 0;
      buttonFirstPressure[2] = 0;
      buttonFirstPressure[3] = 0;
    }
    buttonStates[0] += 1;
    buttonStates[1] -= 1;
    buttonStates[2] -= 1;
    buttonStates[3] -= 1;
    buttonLastPressure[0] = curMillis;
    lastUserInput = curMillis;
  }
  else if (valueRead >= BUTTON_2 - BTN_TOL && valueRead <= BUTTON_2 + BTN_TOL)
  {
    if (buttonStates[1] == 0)
    {
      buttonFirstPressure[0] = 0;
      buttonFirstPressure[1] = curMillis;
      buttonFirstPressure[2] = 0;
      buttonFirstPressure[3] = 0;
    }    
    buttonStates[0] -= 1;
    buttonStates[1] += 1;
    buttonStates[2] -= 1;
    buttonStates[3] -= 1;
    buttonLastPressure[1] = curMillis;
    lastUserInput = curMillis;
  }
  else if (valueRead >= BUTTON_3 - BTN_TOL && valueRead <= BUTTON_3 + BTN_TOL)
  {
    if (buttonStates[2] == 0)
    {
      buttonFirstPressure[0] = 0;
      buttonFirstPressure[1] = 0;
      buttonFirstPressure[2] = curMillis;
      buttonFirstPressure[3] = 0;
    }        
    buttonStates[0] -= 1;
    buttonStates[1] -= 1;
    buttonStates[2] += 1;
    buttonStates[3] -= 1;
    buttonLastPressure[2] = curMillis;
    lastUserInput = curMillis;
  }
  else if (valueRead >= BUTTON_4 - BTN_TOL && valueRead <= BUTTON_4 + BTN_TOL)
  {
    if (buttonStates[3] == 0)
    {
      buttonFirstPressure[0] = 0;
      buttonFirstPressure[1] = 0;
      buttonFirstPressure[2] = 0;
      buttonFirstPressure[3] = curMillis;
    }          
    buttonStates[0] -= 1;
    buttonStates[1] -= 1;
    buttonStates[2] -= 1;
    buttonStates[3] += 1;
    buttonLastPressure[3] = curMillis;
    lastUserInput = curMillis;
  }
  else
  {
    memset(buttonFirstPressure, 0, 4 * sizeof(int));
    memset(buttonLastPressure, 0, 4 * sizeof(int));
    memset(buttonStates, 0, 4 * sizeof(int));
  }

  if (buttonStates[0] < 0) buttonStates[0] = 0;
  if (buttonStates[1] < 0) buttonStates[1] = 0;
  if (buttonStates[2] < 0) buttonStates[2] = 0;
  if (buttonStates[3] < 0) buttonStates[3] = 0;

  if (buttonStates[0] > 5) buttonStates[0] = 5;
  if (buttonStates[1] > 5) buttonStates[1] = 5;
  if (buttonStates[2] > 5) buttonStates[2] = 5;
  if (buttonStates[3] > 5) buttonStates[3] = 5;

}


ISR(TIMER1_COMPA_vect)
{
  PORTB = (PORTB & PROBE_MASK) == 0 ? PORTB | PROBE_MASK : PORTB & ~PROBE_MASK;  
  isPhoto1 = ((PIND & PHOTO_1_MASK) > 0);
  isPhoto2 = ((PIND & PHOTO_2_MASK) > 0);
}


UserEvent readUserEvent(int curMillis)
{
  int oldButtonStates[4];
  memcpy(oldButtonStates, buttonStates, 4 * sizeof(int));
  readUserButton(curMillis);
  /** BUTTON 1 **/
  if (buttonStates[0] > 4 && (buttonLastPressure[0] - buttonFirstPressure[0] < BTN_LONG_PRESSURE_MILLIS))
  {
    return BTN_1_PRESSED;
  }
  else if (buttonStates[0] > 4 && (buttonLastPressure[0] - buttonFirstPressure[0] >= BTN_LONG_PRESSURE_MILLIS))
  {
    buttonFirstPressure[0] = buttonLastPressure[0];
    ignoreRelease[0] = 1;
    return BTN_1_LONGPRS;
  }
  else if (buttonStates[0] == 0 && oldButtonStates[0] && !ignoreRelease[0])
  {
    return BTN_1_RELEASE;
  }
  /** BUTTON 2 **/
  else if (buttonStates[1] > 4 && (buttonLastPressure[1] - buttonFirstPressure[1] < BTN_LONG_PRESSURE_MILLIS))
  {
    return BTN_2_PRESSED;
  }
  else if (buttonStates[1] > 4 && (buttonLastPressure[1] - buttonFirstPressure[1] >= BTN_LONG_PRESSURE_MILLIS))
  {
    buttonFirstPressure[1] = buttonLastPressure[1];
    ignoreRelease[1] = 1;
    return BTN_2_LONGPRS;
  }
  else if (buttonStates[1] == 0 && oldButtonStates[1] && !ignoreRelease[1])
  {
    return BTN_2_RELEASE;
  }  
  /** BUTTON 3 **/
  else if (buttonStates[2] > 4 && (buttonLastPressure[2] - buttonFirstPressure[2] < BTN_LONG_PRESSURE_MILLIS))
  {
    return BTN_3_PRESSED;
  }
  else if (buttonStates[2] > 4 && (buttonLastPressure[2] - buttonFirstPressure[2] >= BTN_LONG_PRESSURE_MILLIS))
  {
    buttonFirstPressure[2] = buttonLastPressure[2];
    ignoreRelease[2] = 1;
    return BTN_3_LONGPRS;
  }
  else if (buttonStates[2] == 0 && oldButtonStates[2] && !ignoreRelease[2])
  {
    return BTN_3_RELEASE;
  }    
  /** BUTTON 4 **/
  else if (buttonStates[3] > 4 && (buttonLastPressure[3] - buttonFirstPressure[3] < BTN_LONG_PRESSURE_MILLIS))
  {
    return BTN_4_PRESSED;
  }
  else if (buttonStates[3] > 4 && (buttonLastPressure[3] - buttonFirstPressure[3] >= BTN_LONG_PRESSURE_MILLIS))
  {
    buttonFirstPressure[3] = buttonLastPressure[3];
    ignoreRelease[3] = 1;    
    return BTN_4_LONGPRS;
  }
  else if (buttonStates[3] == 0 && oldButtonStates[3] && !ignoreRelease[3])
  {
    return BTN_4_RELEASE;
  }    
  else
  {
    memset(ignoreRelease, 0x00, 4 * sizeof(int));
    return NO_INPUT;
  }
  
}


byte OVERLINE[] = {
  B11111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};


void setup()
{
  // set AD prescale to 16
  sbi(ADCSRA,ADPS2);
  cbi(ADCSRA,ADPS1);
  cbi(ADCSRA,ADPS0);  
  
  /** Create 250 micros timer to read photo1 and photo2 **/
  noInterrupts(); // Disabilita gli interrupt durante la configurazione

  TCCR1A = 0; // Modalità normale
  TCCR1B = 0; // Reset del registro
  TCNT1 = 0;  // Reset del contatore

  // Configura il valore di confronto per 250 µs
  OCR1A = 499; // 499 cicli (250 µs con prescaler 8)

  TCCR1B |= (1 << WGM12); // Modalità CTC (Clear Timer on Compare Match)
  TCCR1B |= (1 << CS11);  // Prescaler 8

  TIMSK1 |= (1 << OCIE1A); // Abilita l'interrupt del Timer1

  interrupts(); // Riabilita gli interrupt

  Serial.begin(115200);

  memset(lcdLastPrint, ' ', 20);
  memset(lcdLastMode,  ' ', 20);
  
  pinMode(RESET, OUTPUT);
  pinMode(SLEEP, OUTPUT);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(PROBE, OUTPUT);
  pinMode(PHOTO_1, INPUT);
  pinMode(PHOTO_2, INPUT);

  motorPower(false);

  lcd.init();
  lcd.createChar(0, OVERLINE);
  
  lcdClearDisplay();
  lcd.backlight();
  isBacklight = true;
  lcd.setCursor(0, 0);
            //01234567890123456789
  lcd.print("    UNICODE      ");
  lcd.setCursor(0,1);
           //01234567890123456789
  lcd.print("  AUTOMAZIONE    ");
    
  readEepromParams();
  
  if (eepromParams.Values.curDirection)
  {
    PORTB |= DIR_MASK;
  }
  else
  {
    PORTB &= ~DIR_MASK;
  }

  delay(2000);
  lcdClearDisplay();
}


void automatic(int curMillis)
{
  int startDelta = startReceived ? (curMillis - millisStart) : 0;
  int stopDelta  = stopReceived ? (curMillis - millisStop) : 0;
  int startDelayMillis   = eepromParams.Values.tempoStart;
  int stopDelayMillis    = eepromParams.Values.distSpellic;
  int ignorePhoto1Millis = eepromParams.Values.ignoreF1;  

  if (isPhoto1 && !startReceived && !isStepperMoving)
  {
    if (((curMillis - millisStart) >= ignorePhoto1Millis) || (millisStart == 0))
    {
      millisStart = curMillis;
      startReceived = true;
    }
  }

  if (isPhoto2 && !stopReceived && isStepperMoving)
  {
    millisStop = curMillis;
    stopReceived = true;
  }

  if (startReceived && startDelta >= startDelayMillis)
  {
    isStepperMoving = true;
    startReceived = false;

    motorPower(true);
  }
  else if (stopReceived && stopDelta >= stopDelayMillis)
  {
    isStepperMoving = false;
    stopReceived = false;

    motorPower(false);
  }

  if (isStepperMoving)
  {
    motorStep(VEL_TO_STEP_DELAY[(int)(eepromParams.Values.velErog)]);
  }
}


void loop()
{
  long t0 = micros();
  int curMillis = (int)(t0 / 1000L);

  if (eepromParams.Values.ctrlMode == MODE_AUTO)
  {
    automatic(curMillis);
  }
  else
  {
    if (isStepperMoving)
    {
      int stepDelay = VEL_TO_STEP_DELAY[(int)(eepromParams.Values.velErog)];
      //Serial.println(stepDelay);
      motorStep(stepDelay);
    }
    else
    {
      motorPower(false);
      isStepperMoving = false;
    }
  }
  
  UserEvent e = readUserEvent(curMillis);

  if (curMillis - lastUserInput >= BACKLIGHT_TIMEOUT_MILLIS && isBacklight)
  {
    if (isFirstLoop)
    {
      if (curMillis - lastUserInput >= 2 * BACKLIGHT_TIMEOUT_MILLIS)
      {
        isFirstLoop = false;
      }
    }
    else
    {
      lcd.noBacklight();
      isBacklight = false;
      e = NO_INPUT;
      int buttonsToIgnore = userState == MAIN_MENU ? 2 : 4;
      memset(ignoreRelease, 0x01, buttonsToIgnore * sizeof(int));
      memcpy(buttonFirstPressure, buttonLastPressure, buttonsToIgnore * sizeof(int));
    }
  }
  else if (curMillis - lastUserInput == 0 && !isBacklight)
  {
    lcd.backlight();
    isBacklight = true;
    e = NO_INPUT;
    int buttonsToIgnore = userState == MAIN_MENU ? 2 : 4;
    memset(ignoreRelease, 0x01, buttonsToIgnore * sizeof(int));
    memcpy(buttonFirstPressure, buttonLastPressure, buttonsToIgnore * sizeof(int));
  }

  if (lcdBlink)
  {
    int blinkDelta = curMillis - lcdBlinkTime;
    //Serial.println(blinkDelta);
    int startColumn = 11;
    if (menuIndex == 0)
    {
      startColumn = 12;
    }
    if (blinkDelta < 700)
    {
      for (int i = startColumn; i < 16; i++)
      {
        lcd.setCursor(i, 1);
        lcd.write(byte(0));
      }
    }
    else if (blinkDelta >= 700 && blinkDelta < 1000)
    {
      lcd.setCursor(11, 1);
      lcd.print("     ");
    }
    else
    {
      lcdBlinkTime = curMillis;
    }
  }

  char prompt[20];
  switch (userState)
  {
    case MAIN_MENU:
    {
      
      inputValueToLcdString(menuLabels[menuIndex], prompt, menuIndex == 0 ? eepromParams.Values.velErog :
                                                      menuIndex == 1 ? eepromParams.Values.distSpellic :
                                                      menuIndex == 2 ? eepromParams.Values.tempoStart  :
                                                      eepromParams.Values.ignoreF1, menuIndex == 0 ? 1 : 2);
      lcdPrint(prompt);   
      lcdPrintMode();
      switch (e)
      {
        case BTN_1_RELEASE:
        {
          menuIndex += 1;
          menuIndex %= 4;
        }
        break;
        case BTN_2_RELEASE:
        {        
            nextVel = eepromParams.Values.velErog;
            nextDistSpellic = eepromParams.Values.distSpellic;
            nextTempoStart = eepromParams.Values.tempoStart;
            nextIgnoreF1 = eepromParams.Values.ignoreF1;
            userState = menuIndex == 0 ? MENU_VEL :
                        menuIndex == 1 ? MENU_DIST_SPELLIC :
                        menuIndex == 2 ? MENU_START :
                        MENU_IGNORE_F1;
        }
        break;
        case BTN_3_PRESSED:
        {
          if (eepromParams.Values.ctrlMode == MODE_MANUAL && isStepperMoving)
          {
            motorPower(false);
            btn3PressedOnManual = true;
          }
        }
        break;
        case BTN_3_RELEASE:
        {
          if (eepromParams.Values.ctrlMode == MODE_AUTO)
          {
            motorPower(false);
            isStepperMoving = false;
            startReceived = false;
            stopReceived = false;
            
            eepromParams.Values.ctrlMode = MODE_MANUAL;
            EEPROM_UPDATE = true;
          }
          else
          {
            if (btn3PressedOnManual)
            {
              btn3PressedOnManual = false;
              isStepperMoving = false;
            }
            else
            {
              isStepperMoving = true;
              motorPower(true);
            }
          }
        }
        break;
        case BTN_4_RELEASE:
        {
          motorPower(false);
          isStepperMoving = false;
          startReceived = false;
          stopReceived = false;
          
          eepromParams.Values.ctrlMode = MODE_AUTO;
          EEPROM_UPDATE = true;
        }
        break;        
        default: break;
      }
    }
    break;
    case MENU_VEL:
    {
      switch (e)
      {
        case BTN_1_LONGPRS:
        {
          eepromParams.Values.velErog = nextVel;
          EEPROM_UPDATE = true;
          userState = MAIN_MENU;
        }
        break;
        case BTN_1_RELEASE:
        {
          if (nextVel > 1)
            nextVel -= 1;
          else
            nextVel = MAX_VEL_INT;
                    
          inputValueToLcdString(menuLabels[menuIndex], prompt, nextVel, 1);
          lcdPrint(prompt);      
        }
        break;
        case BTN_2_RELEASE:
        {
          if (nextVel < MAX_VEL_INT)
            nextVel += 1;
          else 
            nextVel = 1;
          
          inputValueToLcdString(menuLabels[menuIndex], prompt, nextVel, 1);
          lcdPrint(prompt);            
        }
        break;
        case BTN_3_PRESSED:
        {
          countBtn3Pressed += 1;
          if (countBtn3Pressed < 20)
          {
            break;
          }
        }
        case BTN_3_RELEASE:
        {
          countBtn3Pressed = 0;
          if (nextVel > 10)
            nextVel -= 10;
          else
            nextVel = MAX_VEL_INT;

          inputValueToLcdString(menuLabels[menuIndex], prompt, nextVel, 1);
          lcdPrint(prompt);                      
        }
        break;
        case BTN_4_PRESSED:
        {
          countBtn4Pressed += 1;
          if (countBtn4Pressed < 20)
          {
            break;
          }
        }
        case BTN_4_RELEASE:
        {
          countBtn4Pressed = 0;
          if (nextVel <= MAX_VEL_INT - 10)
            nextVel += 10;
          else
            nextVel = 1;

          inputValueToLcdString(menuLabels[menuIndex], prompt, nextVel, 1);
          lcdPrint(prompt);                
        }
        break;
        default: break;
      }
        
    }
    break;
    case MENU_DIST_SPELLIC:
    {
      switch (e)
      {
        case BTN_1_LONGPRS:
        {
          eepromParams.Values.distSpellic = nextDistSpellic;
          EEPROM_UPDATE = true;
          userState = MAIN_MENU;                 
        }
        break;
        case BTN_1_RELEASE:
        {
          if (nextDistSpellic > 1)
            nextDistSpellic -= 1;
          else
            nextDistSpellic = MAX_TEMPO_INT;
    
          inputValueToLcdString(menuLabels[menuIndex], prompt, nextDistSpellic, 2);
          lcdPrint(prompt);                  
        }
        break;
        case BTN_2_RELEASE:
        {
          if (nextDistSpellic < MAX_TEMPO_INT)
            nextDistSpellic += 1;
          else
            nextDistSpellic = 0;

          inputValueToLcdString(menuLabels[menuIndex], prompt, nextDistSpellic, 2);
          lcdPrint(prompt);             
        }
        break;
        case BTN_3_PRESSED:
        {
          countBtn3Pressed += 1;
          if (countBtn3Pressed < 20)
          {
            break;
          }
        }
        case BTN_3_RELEASE:
        {
          countBtn3Pressed = 0;
          if (nextDistSpellic > 10)
            nextDistSpellic -= 10;
          else
            nextDistSpellic = MAX_TEMPO_INT;

          inputValueToLcdString(menuLabels[menuIndex], prompt, nextDistSpellic, 2);
          lcdPrint(prompt);                
        }
        break;
        case BTN_4_PRESSED:
        {
          countBtn4Pressed += 1;
          if (countBtn4Pressed < 20)
          {
            break;
          }
        }
        case BTN_4_RELEASE:
        {
          countBtn4Pressed = 0;
          if (nextDistSpellic <= MAX_TEMPO_INT - 10)
            nextDistSpellic += 10;
          else
            nextDistSpellic = 0;

          inputValueToLcdString(menuLabels[menuIndex], prompt, nextDistSpellic, 2);
          lcdPrint(prompt);             
        }
        break;
        default: break;
      }     
    }
    break;
    case MENU_START:
    {
      switch (e)
      {
        case BTN_1_LONGPRS:
        {
          eepromParams.Values.tempoStart = nextTempoStart;
          EEPROM_UPDATE = true;
          userState = MAIN_MENU;             
        }
        break;
        case BTN_1_RELEASE:
        {
          if (nextTempoStart > 1)
            nextTempoStart -= 1;
          else
            nextTempoStart = MAX_TEMPO_INT;
    
          inputValueToLcdString(menuLabels[menuIndex], prompt, nextTempoStart, 2);
          lcdPrint(prompt);                 
        }
        break;
        case BTN_2_RELEASE:
        {
          if (nextTempoStart < MAX_TEMPO_INT)
            nextTempoStart += 1;
          else
            nextTempoStart = 0;
    
          inputValueToLcdString(menuLabels[menuIndex], prompt, nextTempoStart, 2);
          lcdPrint(prompt);              
        }
        break;
        case BTN_3_PRESSED:
        {
          countBtn3Pressed += 1;
          if (countBtn3Pressed < 20)
          {
            break;
          }
        }
        case BTN_3_RELEASE:
        {
          countBtn3Pressed = 0;
          if (nextTempoStart > 10)
            nextTempoStart -= 10;
          else
            nextTempoStart = MAX_TEMPO_INT;
    
          inputValueToLcdString(menuLabels[menuIndex], prompt, nextTempoStart, 2);
          lcdPrint(prompt);                 
        }
        break;
        case BTN_4_PRESSED:
        {
          countBtn4Pressed += 1;
          if (countBtn4Pressed < 20)
          {
            break;
          }
        }
        case BTN_4_RELEASE:
        {
          countBtn4Pressed = 0;
          if (nextTempoStart <= MAX_TEMPO_INT - 10)
            nextTempoStart += 10;
          else
            nextTempoStart = 0;

          inputValueToLcdString(menuLabels[menuIndex], prompt, nextTempoStart, 2);
          lcdPrint(prompt);              
        }
        break;
        default: break;
      }      
    }
    break;
    case MENU_IGNORE_F1:
    {
      switch (e)
      {
        case BTN_1_LONGPRS:
        {
          eepromParams.Values.ignoreF1 = nextIgnoreF1;
          EEPROM_UPDATE = true;
          userState = MAIN_MENU;             
        }
        break;
        case BTN_1_RELEASE:
        {
          if (nextIgnoreF1 > 1)
            nextIgnoreF1 -= 1;
          else
            nextIgnoreF1 = MAX_TEMPO_INT;
    
          inputValueToLcdString(menuLabels[menuIndex], prompt, nextIgnoreF1, 2);
          lcdPrint(prompt);                 
        }
        break;
        case BTN_2_RELEASE:
        {
          if (nextIgnoreF1 < MAX_TEMPO_INT)
            nextIgnoreF1 += 1;
          else
            nextIgnoreF1 = 0;
    
          inputValueToLcdString(menuLabels[menuIndex], prompt, nextIgnoreF1, 2);
          lcdPrint(prompt);              
        }
        break;
        case BTN_3_PRESSED:
        {
          countBtn3Pressed += 1;
          if (countBtn3Pressed < 20)
          {
            break;
          }
        }
        case BTN_3_RELEASE:
        {
          countBtn3Pressed = 0;
          if (nextIgnoreF1 > 10)
            nextIgnoreF1 -= 10;
          else
            nextIgnoreF1 = MAX_TEMPO_INT;
    
          inputValueToLcdString(menuLabels[menuIndex], prompt, nextIgnoreF1, 2);
          lcdPrint(prompt);                 
        }
        break;
        case BTN_4_PRESSED:
        {
          countBtn4Pressed += 1;
          if (countBtn4Pressed < 20)
          {
            break;
          }
        }
        case BTN_4_RELEASE:
        {
          countBtn4Pressed = 0;
          if (nextIgnoreF1 <= MAX_TEMPO_INT - 10)
            nextIgnoreF1 += 10;
          else
            nextIgnoreF1 = 0;

          inputValueToLcdString(menuLabels[menuIndex], prompt, nextIgnoreF1, 2);
          lcdPrint(prompt);              
        }
        break;
        default: break;
      }      
    }
    break;      
    default: break;
  }

  if (userState != MAIN_MENU)
  {
    lcdClearMode();
  }

  if (EEPROM_UPDATE && !isStepperMoving)
  {
    writeEepromParams();
    EEPROM_UPDATE = false;
  }
  
  MAINT_Handler(&EEPROM_UPDATE, &isStepperMoving);
#if 0
  long deltaMicros = micros() - t0;
  long toWait = eepromParams.Values.minDelayMicros - deltaMicros;
  if (toWait > 0 && isStepperMoving)
  {
    Serial.print("wait "); Serial.println(toWait);
    delayMicroseconds(toWait);
  }
#endif

  
}
