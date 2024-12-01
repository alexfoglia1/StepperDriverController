#define SATURATE(x, _min, _max)(x < _min ? _min : x > _max ? _max : x)
#define RESET  12
#define SLEEP  11
#define STEP   10
#define DIR     9
#define M0      6
#define M1      7
#define M2      8


void setup() {
  Serial.begin(38400);
  
  pinMode(RESET, OUTPUT);
  pinMode(SLEEP, OUTPUT);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);

  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  

  digitalWrite(RESET, HIGH);
  digitalWrite(SLEEP, HIGH);
  digitalWrite(STEP, LOW);
  digitalWrite(DIR, LOW);

  digitalWrite(LED_BUILTIN, LOW);
}

int tempoStep = 500;

void loop() {

  if (Serial.available())
  {
    Serial.read();
    tempoStep += 50;
    if (tempoStep > 1000)
    {
      tempoStep = 500;
    }

    Serial.println(tempoStep);
  }
    
  digitalWrite(STEP, HIGH);

  delayMicroseconds(tempoStep);

  digitalWrite(STEP, LOW);

  delayMicroseconds(tempoStep);

}