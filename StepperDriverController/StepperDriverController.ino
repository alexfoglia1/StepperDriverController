#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#include "EEPROM_IMG.h"
#include "Maintenance.h"
#include "Board.h"
#include "Motor.h"

#define LCD_BLANK_LINE "                    "

#define BTN_MASK(BTN)(1 << BTN)


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
  BTN_5_PRESSED,
  BTN_1_RELEASE,
  BTN_2_RELEASE,
  BTN_3_RELEASE,
  BTN_4_RELEASE,
  BTN_5_RELEASE,
  NO_INPUT
} UserEvent;


typedef enum
{
  MODE_AUTO = 0,
  MODE_MANUAL
} CtrlMode;


LiquidCrystal_I2C lcd(0x27,20,2);
bool EEPROM_UPDATE = false;
uint8_t buttonsState = 0;
int spinupStep = 0;
int spinupStopStep = 0;
EEPROM_IMG eepromParams;
EEPROM_IMG eepromDefaults = {MAX_VEL_INT, 1000, 1000, 100, MODE_AUTO, 800, 2000, 0, BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4, BUTTON_5, 0};
uint16_t nextVel = MAX_VEL_INT;
uint16_t nextDistSpellic = 1000;
uint16_t nextTempoStart = 1000;
uint16_t nextIgnoreF1 = 10;

UserState userState = MAIN_MENU;
int menuIndex = 0;
const char* menuLabels[5] = {"VEL EROGA ",
                             "DIS SPELL",
                             "TEM START",
                             "IGN FOTC1",
                             "SW. VERS.",
                            };
char lcdLastPrint[20];
char lcdLastMode[20];

int VEL_TO_STEP_DELAY[MAX_VEL_INT + 1];
int millisStart = 0;
int millisStop = 0;
bool isStepperMoving = false;
bool isPhoto1 = false;
bool isPhoto2 = false;
bool startReceived = false;
bool stopReceived = false;
uint16_t countBtn1Pressed = 0;
uint16_t countBtn2Pressed = 0;
uint16_t countBtn3Pressed = 0;
uint16_t countBtn4Pressed = 0;

bool isMotorSpinup = false;

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
  
  byte curChecksum = checksum(eepromParams.Bytes, EEPROM_SIZE - 1);
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
  }
}


void lcdClearDisplay()
{
  lcd.setCursor(0, 0);
  lcd.print(LCD_BLANK_LINE);
  lcd.setCursor(0, 1);
  lcd.print(LCD_BLANK_LINE);
}


void inputValueToLcdString(const char* label, char prompt[20], uint16_t n, bool isNumeric)
{
  memset(prompt, ' ', 20);
  if (!isNumeric)
  {
    sprintf(prompt, "%s %c.%c-%c", label, MAJOR_V, MINOR_V, STAGE_V);
    return;
  }
  uint16_t units = n % 10;
  uint16_t tens = (n / 10) % 10;
  uint16_t hundreds = (n / 100) % 10;
  uint16_t thousands = (n / 1000) % 10;
  uint16_t tensthousands = (n / 10000) % 10;

  sprintf(prompt, "%s %d%d%d%d.%d", label, tensthousands, thousands, hundreds, tens, units); 
}


uint8_t btnCount[5] = {0, 0, 0, 0, 0};
void readUserButton(int curMillis)
{
  int valueRead = analogRead(BUTTON_IN);

  if (valueRead + BTN_TOL >= eepromParams.Values.btn1 && valueRead - BTN_TOL <= eepromParams.Values.btn1)
  {
    if (btnCount[0] < 32) btnCount[0] += 1;
  }
  else if (valueRead + BTN_TOL >= eepromParams.Values.btn2 && valueRead - BTN_TOL <= eepromParams.Values.btn2)
  {
    if (btnCount[1] < 32) btnCount[1] += 1;
  }
  else if (valueRead + BTN_TOL >= eepromParams.Values.btn3 && valueRead - BTN_TOL <= eepromParams.Values.btn3)
  {
    if (btnCount[2] < 32) btnCount[2] += 1;
  }
  else if (valueRead + BTN_TOL >= eepromParams.Values.btn4 && valueRead - BTN_TOL <= eepromParams.Values.btn4)
  {
    if (btnCount[3] < 32) btnCount[3] += 1;        
  }
  else if (valueRead + BTN_TOL >= eepromParams.Values.btn5 && valueRead - BTN_TOL <= eepromParams.Values.btn5)
  {        
    if (btnCount[4] < 32) btnCount[4] += 1;
  }
  else
  {
    for (int i = 0; i < 5; i++)
    {
      if (btnCount[i] > 1) btnCount[i] /= 2;
      else btnCount[i] = 0;
    }
  }

  for (int i = 0; i < 5; i++)
  {
    if (btnCount[i] == 32)
    {
      buttonsState |= (BTN_MASK(i + 1));
    }
    else if (btnCount[i] == 0)
    {
      buttonsState &= ~(BTN_MASK(i + 1));
    }
  }
}


ISR(TIMER1_COMPA_vect)
{
  PORTB = (PORTB & PROBE_MASK) == 0 ? PORTB | PROBE_MASK : PORTB & ~PROBE_MASK;  
  isPhoto1 = ((PIND & PHOTO_1_MASK) > 0);
  isPhoto2 = ((PIND & PHOTO_2_MASK) > 0);
}



UserEvent readUserEvent(int curMillis)
{
  uint8_t oldButtonsState = buttonsState;
  readUserButton(curMillis);
  /** BUTTON 1 **/
  if (buttonsState & BTN_MASK(1) && !(oldButtonsState & BTN_MASK(1)))
  {
    //lcdPrint("BTN_1_PRESSED");
    return BTN_1_PRESSED;
  }
  else if (oldButtonsState & BTN_MASK(1) && !(buttonsState & BTN_MASK(1)))
  {
    //lcdPrint("BTN_1_RELEASED");
    return BTN_1_RELEASE;
  }
  /** BUTTON 2 **/
  else if (buttonsState & BTN_MASK(2) && !(oldButtonsState & BTN_MASK(2)))
  {
    return BTN_2_PRESSED;
  }
  else if (oldButtonsState & BTN_MASK(2) && !(buttonsState & BTN_MASK(2)))
  {
    return BTN_2_RELEASE;
  }  
  /** BUTTON 3 **/
  else if (buttonsState & BTN_MASK(3) && !(oldButtonsState & BTN_MASK(3)))
  {
    return BTN_3_PRESSED;
  }
  else if (oldButtonsState & BTN_MASK(3) && !(buttonsState & BTN_MASK(3)))
  {
    return BTN_3_RELEASE;
  }    
  /** BUTTON 4 **/
  else if (buttonsState & BTN_MASK(4) && !(oldButtonsState & BTN_MASK(4)))
  {
    return BTN_4_PRESSED;
  }
  else if (oldButtonsState & BTN_MASK(4) && !(buttonsState & BTN_MASK(4)))
  {
    return BTN_4_RELEASE;
  }    
  /** BUTTON 5 **/
  else if (buttonsState & BTN_MASK(5) && !(oldButtonsState & BTN_MASK(5)))
  {
    return BTN_5_PRESSED;
  }
  else if (oldButtonsState & BTN_MASK(5) && !(buttonsState & BTN_MASK(5)))
  {
    return BTN_5_RELEASE;
  }    
  else
  {
    return NO_INPUT;
  }
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
    spinupStopStep = eepromParams.Values.velErog;
    spinupStep = 0;
    
    isMotorSpinup = true;
    startReceived = false;
#ifdef BOARD_REV_B
    motorPower(true);
#endif    
  }
  else if (stopReceived && stopDelta >= stopDelayMillis)
  {
    isStepperMoving = false;
    stopReceived = false;
#ifdef BOARD_REV_B
    motorPower(false);
#endif    
  }
}


void debugUserEvent(UserEvent e)
{
  switch (e)
  {
      case BTN_1_PRESSED: Serial.println("1 PRESSED"); break;
      case BTN_2_PRESSED: Serial.println("2 PRESSED"); break;
      case BTN_3_PRESSED: Serial.println("3 PRESSED"); break;
      case BTN_4_PRESSED: Serial.println("4 PRESSED"); break;
      case BTN_5_PRESSED: Serial.println("5 PRESSED"); break;
      case BTN_1_RELEASE: Serial.println("1 RELEASED"); break;
      case BTN_2_RELEASE: Serial.println("2 RELEASED"); break;
      case BTN_3_RELEASE: Serial.println("3 RELEASED"); break;
      case BTN_4_RELEASE: Serial.println("4 RELEASED"); break;
      case BTN_5_RELEASE: Serial.println("5 RELEASED"); break;
  }
}



void navigateSubMenu(UserEvent e, uint16_t* valueToUpdate, uint16_t* eepromValue, int minValue, int maxValue)
{
  char prompt[20];

  if (buttonsState & BTN_MASK(1)) countBtn1Pressed += 1;
  else if (buttonsState & BTN_MASK(2)) countBtn2Pressed += 1;
  else if (buttonsState & BTN_MASK(3)) countBtn3Pressed += 1;
  else if (buttonsState & BTN_MASK(4)) countBtn4Pressed += 1;

  if (e == BTN_1_RELEASE) countBtn1Pressed = 0;
  else if (e == BTN_2_RELEASE) countBtn2Pressed = 0;
  else if (e == BTN_3_RELEASE) countBtn3Pressed = 0;
  else if (e == BTN_4_RELEASE) countBtn4Pressed = 0;

  if (countBtn1Pressed > 16383) { countBtn1Pressed = 16384; e = BTN_1_RELEASE; }
  else if (countBtn2Pressed > 16383) { countBtn2Pressed = 16384; e = BTN_2_RELEASE; }
  else if (countBtn3Pressed > 16383) { countBtn3Pressed = 16384; e = BTN_3_RELEASE; }
  else if (countBtn4Pressed > 16383) { countBtn4Pressed = 16384; e = BTN_4_RELEASE; }
  
  switch (e)
  {
    case BTN_5_RELEASE:
    {
      *eepromValue = *valueToUpdate;
      EEPROM_UPDATE = true;
      userState = MAIN_MENU;
    }
    break;
    case BTN_1_RELEASE:
    {
      if (*valueToUpdate > minValue)
        *valueToUpdate -= 1;
      else
        *valueToUpdate = maxValue;
                    
      inputValueToLcdString(menuLabels[menuIndex], prompt, *valueToUpdate, true);
      lcdPrint(prompt);      
    }
    break;
    case BTN_2_RELEASE:
    {
      if (*valueToUpdate < maxValue)
        *valueToUpdate += 1;
      else 
        *valueToUpdate = minValue;
          
      inputValueToLcdString(menuLabels[menuIndex], prompt, *valueToUpdate, true);
      lcdPrint(prompt);            
    }
    break;
    case BTN_3_RELEASE:
    {
      if (*valueToUpdate > 10 * minValue)
        *valueToUpdate -= 10;
      else
        *valueToUpdate = maxValue;

      inputValueToLcdString(menuLabels[menuIndex], prompt, *valueToUpdate, true);
      lcdPrint(prompt);                      
    }
    break;
    case BTN_4_RELEASE:
    {
      if (*valueToUpdate <= maxValue - 10)
        *valueToUpdate += 10;
      else
        *valueToUpdate = minValue;

      inputValueToLcdString(menuLabels[menuIndex], prompt, *valueToUpdate, true);
      lcdPrint(prompt);                
    }
    break;
    default: break;
  }
}


void navigateMainMenu(UserEvent e)
{
  char prompt[20];
  inputValueToLcdString(menuLabels[menuIndex], prompt, menuIndex == 0 ? eepromParams.Values.velErog :
                                                       menuIndex == 1 ? eepromParams.Values.distSpellic :
                                                       menuIndex == 2 ? eepromParams.Values.tempoStart  :
                                                       menuIndex == 3 ? eepromParams.Values.ignoreF1 : 0,
                                                       menuIndex != 4);
  lcdPrint(prompt);   
  lcdPrintMode();
  switch (e)
  {
    case BTN_1_RELEASE:
    {
      if (menuIndex == 0)
        menuIndex = 4;
      else
        menuIndex -= 1;
    }
    break;
    case BTN_2_RELEASE:
    {
      if (menuIndex == 4)
        menuIndex = 0;
      else
        menuIndex += 1;
    }
    break;
    case BTN_5_RELEASE:
    {          
       nextVel = eepromParams.Values.velErog;
       nextDistSpellic = eepromParams.Values.distSpellic;
       nextTempoStart = eepromParams.Values.tempoStart;
       nextIgnoreF1 = eepromParams.Values.ignoreF1;
        userState = menuIndex == 0 ? MENU_VEL :
                    menuIndex == 1 ? MENU_DIST_SPELLIC :
                    menuIndex == 2 ? MENU_START :
                    menuIndex == 3 ? MENU_IGNORE_F1 : userState;
    }
    break;
    case BTN_3_RELEASE:
    {
      if (eepromParams.Values.ctrlMode == MODE_AUTO)
      {
#ifdef BOARD_REV_B            
        motorPower(false);
#endif
        isStepperMoving = false;
        startReceived = false;
        stopReceived = false;
            
        eepromParams.Values.ctrlMode = MODE_MANUAL;
        EEPROM_UPDATE = true;
      }
      else
      {
        if (isStepperMoving)
        {
          isStepperMoving = false;
        }
        else
        {
          spinupStopStep = eepromParams.Values.velErog;
          spinupStep = 0;
    
          isMotorSpinup = true;
        }
#ifdef BOARD_REV_B
        motorPower(isStepperMoving);
#endif            
      }
    }
    break;
    case BTN_4_RELEASE:
    {
#ifdef BOARD_REV_B          
      motorPower(false);
#endif
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
  
  pinMode(STEP_M, OUTPUT);
  pinMode(STEP_P, OUTPUT);
  pinMode(DIR_M, OUTPUT);
  pinMode(DIR_P, OUTPUT);
  pinMode(PROBE, OUTPUT);
  pinMode(PHOTO_1, INPUT);
  pinMode(PHOTO_2, INPUT);

  digitalWrite(STEP_P, HIGH);
  digitalWrite(DIR_P, HIGH);

  PORTB |= (STEP_P_MASK | DIR_P_MASK);
  
#ifdef BOARD_REV_B
  motorPower(false);
#endif

  lcd.init();
  
  lcdClearDisplay();
  lcd.backlight();
  lcd.setCursor(0, 0);
            //01234567890123456789
  lcd.print("    UNICODE      ");
  lcd.setCursor(0,1);
           //01234567890123456789
  lcd.print("  AUTOMAZIONE    ");
    
  readEepromParams();
  
  if (eepromParams.Values.curDirection)
  {
    PORTB |= DIR_M_MASK;
  }
  else
  {
    PORTB &= ~DIR_M_MASK;
  }

  delay(2000);
  lcdClearDisplay();
}


void __loop__()
{
  long t0 = micros();
  int curMillis = (int)(t0 / 1000L);
  UserEvent e = readUserEvent(curMillis);
  debugUserEvent(e);
  
}


void loop()
{
  long t0 = micros();
  int curMillis = (int)(t0 / 1000L);

  if (isMotorSpinup)
  {
    if (spinupStep < spinupStopStep)
    {
      int stepDelay = VEL_TO_STEP_DELAY[spinupStep];
      motorStep(stepDelay);

      spinupStep += 30;
    }
    else
    {
      isMotorSpinup = false;
      isStepperMoving = true;
    }
  }

  if (eepromParams.Values.ctrlMode == MODE_AUTO)
  {
    automatic(curMillis);
  }
  
  if (isStepperMoving)
  {
    motorStep(VEL_TO_STEP_DELAY[(int)(eepromParams.Values.velErog)]);
  }
  
  UserEvent e = readUserEvent(curMillis);
  
  switch (userState)
  {
    case MAIN_MENU:
      navigateMainMenu(e);
    break;
    case MENU_VEL:
      navigateSubMenu(e, &nextVel, &eepromParams.Values.velErog, 1, MAX_VEL_INT);
    break;
    case MENU_DIST_SPELLIC:
      navigateSubMenu(e, &nextDistSpellic, &eepromParams.Values.distSpellic, 0, MAX_TEMPO_INT);
    break;
    case MENU_START:
      navigateSubMenu(e, &nextTempoStart, &eepromParams.Values.tempoStart, 0, MAX_TEMPO_INT);
    break;
    case MENU_IGNORE_F1:
      navigateSubMenu(e, &nextIgnoreF1, &eepromParams.Values.ignoreF1, 0, MAX_TEMPO_INT);
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
