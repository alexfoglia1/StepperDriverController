#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27,20,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

#define RESET 12
#define SLEEP 11
#define STEP  10
#define DIR    9

#define RESET_MASK B00010000
#define SLEEP_MASK B00001000
#define STEP_MASK  B00000100
#define DIR_MASK   B00000010

#define BUTTON_IN A2
#define BUTTON_1 917
#define BUTTON_2 994
#define BUTTON_3 1015
#define BUTTON_4 977
#define BTN_TOL  5

#define BTN_LONG_PRESSURE_MILLIS 1000
#define BACKLIGHT_TIMEOUT_MILLIS 5000

#define MIN_DELAY_MICROS 500
#define MAX_DELAY_MICROS 10000
#define STOP_DELAY_MILLIS 1000

#define MAX_VEL_INT 500
#define EEPROM_ADDR 0

#define LCD_BLANK_LINE "                    "

#define TOGGLE(val)(val > 0 ? 0 : 1)

typedef union
{
  struct
  {
    float velErog;
    float distSpellic;
    float tempoStart;
    uint32_t ctrlMode; 
    uint8_t checksum;
  }__attribute__((packed)) Values;
  byte Bytes[17];
} EEPROM_IMG;


typedef enum
{
  MAIN_MENU = 0,
  MENU_VEL,
  MENU_DIST_SPELLIC,
  MENU_START
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


int VEL_TO_STEP_DELAY[MAX_VEL_INT + 1];

bool EEPROM_UPDATE = false;
const int EEPROM_SIZE = sizeof(EEPROM_IMG);
const int STOP_TIME = 100;
int stepCount = 0;
int curDirection = 0;
int buttonStates[4] = {0, 0, 0, 0};
int buttonFirstPressure[4] = {0, 0, 0, 0};
int buttonLastPressure[4] = {0, 0, 0, 0};
int ignoreRelease[4] = {0, 0, 0, 0};
EEPROM_IMG eepromParams;
EEPROM_IMG eepromDefaults = {25.0f, 10.0f, 10.0f, MODE_AUTO, 0};
float nextVel = 25.0f;
float nextDistSpellic = 10.0f;
float nextTempoStart = 10.0f;
int lastUserInput = 0;
UserState userState = MAIN_MENU;
int menuIndex = 0;
const char* menuLabels[3] = {"VEL. EROGAZIONE",
                             "DIST. SPELLIC.",
                             "TEMPO START",
                            };
char lcdLastPrint[20];
char lcdLastMode[20];
bool isBacklight = false;
int millisStart = 0;
bool isStepperMoving = false;
bool isFirstLoop = true;

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
  byte curChecksum = checksum(eepromParams.Bytes, EEPROM_SIZE - 1);
  //Serial.print("Calculated checksum: "); Serial.println(curChecksum);

  if (curChecksum != eepromParams.Values.checksum)
  {
    memcpy(eepromParams.Bytes, eepromDefaults.Bytes, EEPROM_SIZE);
    writeEepromParams();    
  }
}

void lcdPrint(char* prompt)
{
  if (strncmp(prompt, lcdLastPrint, min(strlen(prompt), 20)) != 0)
  {
    lcd.setCursor(0, 0);
    lcd.print(LCD_BLANK_LINE);
    lcd.setCursor(0, 0);
    lcd.print(prompt);
    memcpy(lcdLastPrint, prompt, min(strlen(prompt), 20));
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
  }
  
}

void lcdClearMode()
{
  const char* prompt = LCD_BLANK_LINE;

  if (strncmp(prompt, lcdLastMode, min(strlen(prompt), 20)) != 0)
  {
    lcd.setCursor(0, 1);
    lcd.print(LCD_BLANK_LINE);
    lcd.setCursor(0, 1);
    lcd.print(prompt);
    memcpy(lcdLastMode, prompt, min(strlen(prompt), 20));
  }
}

void lcdClearDisplay()
{
  lcd.setCursor(0, 0);
  lcd.print(LCD_BLANK_LINE);
  lcd.setCursor(0, 1);
  lcd.print(LCD_BLANK_LINE);
}


void floatToLcdString(char prompt[20], float val)
{
  int iVal = (int) round(10.0f * val);
  memset(prompt, ' ', 20);
  int cent = iVal / 100;
  int dec = (iVal - (100 * cent)) / 10;
  int uni = iVal % 10;
  sprintf(prompt, "VALORE: %d%d.%d", cent, dec, uni); 
  //Serial.println(prompt);
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


void readUserButton(int curMillis)
{
  int valueRead = analogRead(BUTTON_IN);

  if (valueRead >= BUTTON_1 - BTN_TOL && valueRead <= BUTTON_1 + BTN_TOL)
  {
    if (!buttonStates[0])
    {
      buttonFirstPressure[0] = curMillis;
      buttonFirstPressure[1] = 0;
      buttonFirstPressure[2] = 0;
      buttonFirstPressure[3] = 0;
    }
    buttonStates[0] = 1;
    buttonStates[1] = 0;
    buttonStates[2] = 0;
    buttonStates[3] = 0;
    buttonLastPressure[0] = curMillis;
    lastUserInput = curMillis;
  }
  else if (valueRead >= BUTTON_2 - BTN_TOL && valueRead <= BUTTON_2 + BTN_TOL)
  {
    if (!buttonStates[1])
    {
      buttonFirstPressure[0] = 0;
      buttonFirstPressure[1] = curMillis;
      buttonFirstPressure[2] = 0;
      buttonFirstPressure[3] = 0;
    }    
    buttonStates[0] = 0;
    buttonStates[1] = 1;
    buttonStates[2] = 0;
    buttonStates[3] = 0;
    buttonLastPressure[1] = curMillis;
    lastUserInput = curMillis;
  }
  else if (valueRead >= BUTTON_3 - BTN_TOL && valueRead <= BUTTON_3 + BTN_TOL)
  {
    if (!buttonStates[2])
    {
      buttonFirstPressure[0] = 0;
      buttonFirstPressure[1] = 0;
      buttonFirstPressure[2] = curMillis;
      buttonFirstPressure[3] = 0;
    }        
    buttonStates[0] = 0;
    buttonStates[1] = 0;
    buttonStates[2] = 1;
    buttonStates[3] = 0;
    buttonLastPressure[2] = curMillis;
    lastUserInput = curMillis;
  }
  else if (valueRead >= BUTTON_4 - BTN_TOL && valueRead <= BUTTON_4 + BTN_TOL)
  {
    if (!buttonStates[3])
    {
      buttonFirstPressure[0] = 0;
      buttonFirstPressure[1] = 0;
      buttonFirstPressure[2] = 0;
      buttonFirstPressure[3] = curMillis;
    }          
    buttonStates[0] = 0;
    buttonStates[1] = 0;
    buttonStates[2] = 0;
    buttonStates[3] = 1;
    buttonLastPressure[3] = curMillis;
    lastUserInput = curMillis;
  }
  else
  {
    memset(buttonFirstPressure, 0, 4 * sizeof(int));
    memset(buttonLastPressure, 0, 4 * sizeof(int));
    memset(buttonStates, 0, 4 * sizeof(int));
  }
}


UserEvent readUserEvent(int curMillis)
{
  int oldButtonStates[4];
  memcpy(oldButtonStates, buttonStates, 4 * sizeof(int));
  readUserButton(curMillis);
  /** BUTTON 1 **/
  if (buttonStates[0] && (buttonLastPressure[0] - buttonFirstPressure[0] < BTN_LONG_PRESSURE_MILLIS))
  {
    return BTN_1_PRESSED;
  }
  else if (buttonStates[0] && (buttonLastPressure[0] - buttonFirstPressure[0] >= BTN_LONG_PRESSURE_MILLIS))
  {
    buttonFirstPressure[0] = buttonLastPressure[0];
    ignoreRelease[0] = 1;
    return BTN_1_LONGPRS;
  }
  else if (!buttonStates[0] && oldButtonStates[0] && !ignoreRelease[0])
  {
    return BTN_1_RELEASE;
  }
  /** BUTTON 2 **/
  else if (buttonStates[1] && (buttonLastPressure[1] - buttonFirstPressure[1] < BTN_LONG_PRESSURE_MILLIS))
  {
    return BTN_2_PRESSED;
  }
  else if (buttonStates[1] && (buttonLastPressure[1] - buttonFirstPressure[1] >= BTN_LONG_PRESSURE_MILLIS))
  {
    buttonFirstPressure[1] = buttonLastPressure[1];
    ignoreRelease[1] = 1;
    return BTN_2_LONGPRS;
  }
  else if (!buttonStates[1] && oldButtonStates[1] && !ignoreRelease[1])
  {
    return BTN_2_RELEASE;
  }  
  /** BUTTON 3 **/
  else if (buttonStates[2] && (buttonLastPressure[2] - buttonFirstPressure[2] < BTN_LONG_PRESSURE_MILLIS))
  {
    return BTN_3_PRESSED;
  }
  else if (buttonStates[2] && (buttonLastPressure[2] - buttonFirstPressure[2] >= BTN_LONG_PRESSURE_MILLIS))
  {
    buttonFirstPressure[2] = buttonLastPressure[2];
    ignoreRelease[2] = 1;
    return BTN_3_LONGPRS;
  }
  else if (!buttonStates[2] && oldButtonStates[2] && !ignoreRelease[2])
  {
    return BTN_3_RELEASE;
  }    
  /** BUTTON 4 **/
  else if (buttonStates[3] && (buttonLastPressure[3] - buttonFirstPressure[3] < BTN_LONG_PRESSURE_MILLIS))
  {
    return BTN_4_PRESSED;
  }
  else if (buttonStates[3] && (buttonLastPressure[3] - buttonFirstPressure[3] >= BTN_LONG_PRESSURE_MILLIS))
  {
    buttonFirstPressure[3] = buttonLastPressure[3];
    ignoreRelease[3] = 1;    
    return BTN_4_LONGPRS;
  }
  else if (!buttonStates[3] && oldButtonStates[3] && !ignoreRelease[3])
  {
    return BTN_4_RELEASE;
  }    
  else
  {
    memset(ignoreRelease, 0x00, 4 * sizeof(int));
    return NO_INPUT;
  }
  
}


void motorPower(bool powerOn)
{
  if (powerOn)
  {
    PORTB |= (RESET_MASK | SLEEP_MASK);
    //digitalWrite(RESET, HIGH);
    //digitalWrite(SLEEP, HIGH);
  }
  else
  {
    PORTB &= ~(RESET_MASK | SLEEP_MASK);
    //digitalWrite(RESET, LOW);
    //digitalWrite(RESET, LOW);

    stepCount = 0;
  }
}


void setup()
{
  Serial.begin(115200);

  memset(lcdLastPrint, ' ', 20);
  memset(lcdLastMode,  ' ', 20);
  
  pinMode(RESET, OUTPUT);
  pinMode(SLEEP, OUTPUT);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  motorPower(false);
  //digitalWrite(DIR, curDirection);
  if (curDirection)
  {
    PORTB |= DIR_MASK;
  }
  else
  {
    PORTB &= DIR_MASK;
  }
  

  lcd.init();
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

  for (int i = 0; i < MAX_VEL_INT + 1; i++)
  {
    VEL_TO_STEP_DELAY[MAX_VEL_INT - i] = map(i, 0, MAX_VEL_INT, MIN_DELAY_MICROS, MAX_DELAY_MICROS);
  }
  //for (int i = 0; i < MAX_VEL_INT + 1; i++)
  //{
      //Serial.print("USER VEL: "); Serial.print(i/10.0f) + Serial.print(" -> DELAY: "); Serial.println(VEL_TO_STEP_DELAY[i]);
  //}

  delay(3000);
  lcdClearDisplay();

  long t0 = micros();
  millisStart = (int)(t0 / 1000L);
}


void automatic(int curMillis)
{
  int curDelta = (curMillis - millisStart);
  
  if (curDelta < ((int)(eepromParams.Values.tempoStart) * 100) && !isStepperMoving)
  {
    motorPower(false);
  }
  else if (curDelta >= ((int)(eepromParams.Values.tempoStart) * 100))
  {
    if (!isStepperMoving)
    {
      motorPower(true);
      millisStart = curMillis;
      isStepperMoving = true;
    }
    else
    {
      if (curDelta >= ((int)(eepromParams.Values.distSpellic) * 100))
      {
        motorPower(false);
        millisStart = curMillis;
        isStepperMoving = false;
      }
    }
  }

  if (isStepperMoving)
  {
    motorStep(VEL_TO_STEP_DELAY[(int)(eepromParams.Values.velErog) * 10]);
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
      motorStep(VEL_TO_STEP_DELAY[(int)(eepromParams.Values.velErog) * 10]);
    }
    else
    {
      motorPower(false);
      millisStart = curMillis;
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


  switch (userState)
  {
    case MAIN_MENU:
    {
      lcdPrint(menuLabels[menuIndex]);
      lcdPrintMode();
      switch (e)
      {
        case BTN_1_RELEASE:
        {
          menuIndex += 1;
          menuIndex %= 3;
        }
        break;
        case BTN_2_RELEASE:
        {        
          if (menuIndex == 0)
          {
            char prompt[20];
            floatToLcdString(prompt, eepromParams.Values.velErog);
            lcdPrint(prompt);
            nextVel = eepromParams.Values.velErog;
            userState = MENU_VEL;  
          }
          else if (menuIndex != 3)
          {
            char prompt[20];
            floatToLcdString(prompt, menuIndex == 1 ? eepromParams.Values.distSpellic : eepromParams.Values.tempoStart);
            lcdPrint(prompt);
            nextDistSpellic = eepromParams.Values.distSpellic;
            nextTempoStart = eepromParams.Values.tempoStart;
            userState = menuIndex == 1 ? MENU_DIST_SPELLIC : MENU_START;
          }
        }
        break;
        case BTN_3_RELEASE:
        {
          if (eepromParams.Values.ctrlMode == MODE_AUTO)
          {
            motorPower(false);
            millisStart = curMillis;
            isStepperMoving = false;
            
            eepromParams.Values.ctrlMode = MODE_MANUAL;
            EEPROM_UPDATE = true;
          }
          else
          {
            isStepperMoving = !isStepperMoving;
            motorPower(isStepperMoving);
          }
        }
        break;
        case BTN_4_RELEASE:
        {
          motorPower(false);
          millisStart = curMillis;
          isStepperMoving = false;
          
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
          lcdPrint(menuLabels[menuIndex]);
        }
        break;
        case BTN_1_RELEASE:
        {
          if (nextVel > 0.1f)
            nextVel -= 0.1f;
                    
        char prompt[20];
        floatToLcdString(prompt, nextVel);
        lcdPrint(prompt);      
        }
        break;
        case BTN_2_RELEASE:
        {
          if (nextVel <= 49.9f)
            nextVel += 0.1f;
          
          char prompt[20];
          floatToLcdString(prompt, nextVel);
          lcdPrint(prompt);            
        }
        break;
        case BTN_3_RELEASE:
        {
          if (nextVel >= 1.0f)
            nextVel -= 1.0f;

          char prompt[20];
          floatToLcdString(prompt, nextVel);
          lcdPrint(prompt);                      
        }
        break;
        case BTN_4_RELEASE:
        {
          if (nextVel <= 49.0f)
            nextVel += 1.0f;

          char prompt[20];
          floatToLcdString(prompt, nextVel);
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
          lcdPrint(menuLabels[menuIndex]);
        }
        break;
        case BTN_1_RELEASE:
        {
          if (nextDistSpellic >= 0.1f)
            nextDistSpellic -= 0.1f;
          else if (nextDistSpellic < 0.1f)
            nextDistSpellic = 0.0f;
    
          char prompt[20];
          floatToLcdString(prompt, nextDistSpellic);
          lcdPrint(prompt);                  
        }
        break;
        case BTN_2_RELEASE:
        {
          if (nextDistSpellic <= 249.9f)
            nextDistSpellic += 0.1f;

          char prompt[20];
          floatToLcdString(prompt, nextDistSpellic);
          lcdPrint(prompt);             
        }
        break;
        case BTN_3_RELEASE:
        {
          if (nextDistSpellic >= 1.0f)
            nextDistSpellic -= 1.0f;

          char prompt[20];
          floatToLcdString(prompt, nextDistSpellic);
          lcdPrint(prompt);                
        }
        break;
        case BTN_4_RELEASE:
        {
          if (nextDistSpellic <= 249.0f)
            nextDistSpellic += 1.0f;

          char prompt[20];
          floatToLcdString(prompt, nextDistSpellic);
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
          lcdPrint(menuLabels[menuIndex]);
        }
        break;
        case BTN_1_RELEASE:
        {
          if (nextTempoStart >= 0.1f)
            nextTempoStart -= 0.1f;
          else if (nextTempoStart < 0.1f)
            nextTempoStart = 0.0f;
    
          char prompt[20];
          floatToLcdString(prompt, nextTempoStart);
          lcdPrint(prompt);                 
        }
        break;
        case BTN_2_RELEASE:
        {
          if (nextTempoStart <= 249.9f)
            nextTempoStart += 0.1f;
    
          char prompt[20];
          floatToLcdString(prompt, nextTempoStart);
          lcdPrint(prompt);              
        }
        break;
        case BTN_3_RELEASE:
        {
          if (nextTempoStart >= 1.0f)
            nextTempoStart -= 1.0f;   
    
          char prompt[20];
          floatToLcdString(prompt, nextTempoStart);
          lcdPrint(prompt);                 
        }
        break;
        case BTN_4_RELEASE:
        {
          if (nextTempoStart <= 249.0f)
            nextTempoStart += 1.0f;
    
          char prompt[20];
          floatToLcdString(prompt, nextTempoStart);
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

  if (EEPROM_UPDATE)
  {
    writeEepromParams();
    EEPROM_UPDATE = false;
  }
  
  long deltaMicros = micros() - t0;
  long toWait = MIN_DELAY_MICROS - deltaMicros;
  if (toWait > 0)
  {
    delayMicroseconds(toWait);
  }

  
}
