#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Arduino.h>

LiquidCrystal_I2C lcd(0x27,16,2);  // Устанавливаем дисплей

const int RST_PIN = 9;             // пин RST
const int SS_PIN = 10;             // пин SDA (SS)
const int soundPin = A0;
const int servo1Pin = 2;
const int servo2Pin = 3;
const int servo3Pin = 4;           //! 5 пин занят металлодетектором
const int servo4Pin = 6;
const int servo5Pin = 7;
const int servo6Pin = 8;

//b --------------------------------------------Изменяемые переменные--------------------------------------------
int soundMoment_Value = 0;            // Моментальное значение
int soundActiv_Value = 500;           // Уровень по которому сравниваем //!--------------------

int metalMoment_Value = 0;            // Моментальное значение 
int metalActiv_Value = 450;           // Уровень по которому сравниваем //!--------------------

int pin;
int angle;
int pulseWidth;

bool flagAuth = false;                // Признак успешной авторизации
bool flagListening = false;           // Признак состояния опрашивания датчика звука
bool flagSoundLikeGlass = false;      // Признак звука стекла
bool flagDetectLikeMetal = false;     // Признак звука металла 
bool flagRotate = false;
//i --------------------------------------------Изменяемые переменные--------------------------------------------

//b --------------------------------------------Настройка Металлодетектора--------------------------------------------
#define SET(x,y) (x |=(1<<y))            //-Bit set/clear macros
#define CLR(x,y) (x &= (~(1<<y)))        // |
#define CHK(x,y) (x & (1<<y))            // |
#define TOG(x,y) (x^=(1<<y))             //-+

unsigned long t0 = 0;
int t = 0;
int val = 0;
unsigned char tflag = 0;
int v0 = 12000;
float f = 0;
unsigned int FTW = 0;
unsigned int PCW = 0;
//i--------------------------------------------Настройка Металлодетектора--------------------------------------------

//b--------------------------------------------Создаём объекты--------------------------------------------
MFRC522 mfrc522(SS_PIN, RST_PIN); 
//i--------------------------------------------Создаём объекты--------------------------------------------

//b--------------------------------------------Объявление функций--------------------------------------------

void draw_info(uint32_t);
void draw_intro();
bool check_metal();
bool check_glass();
float absf(float);
void metalHandler();
void servoPulse(int , int );
void draw_message(String, String);
//i--------------------------------------------Объявление функций--------------------------------------------

void setup()
{
  Serial.begin(9600);     // Инициализация послед. порта
  SPI.begin();            // Инициализация шины SPI

  mfrc522.PCD_Init();     // Инициализация считывателя RC522
  lcd.init();              
  lcd.backlight();        // Включаем подсветку дисплея

  // Установка счетчика таймера 1 на 5 пине
  TCCR1A = 0;
  TCCR1B = 0x07;
  SET(TIMSK1, OCF1A);

  // Начальные положения сервоприводов-
  servoPulse(servo1Pin, 0);
  servoPulse(servo2Pin, 0);
  servoPulse(servo3Pin, 0);
  servoPulse(servo4Pin, 0);
  servoPulse(servo5Pin, 0);
  servoPulse(servo6Pin, 0);

  draw_intro(); // Рисуем заставку
  
}
void loop()
{
  // Если поднесена метка
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    // запишем метку в 4 байта
    uint32_t ID;
    for (byte i = 0; i < 4; i++) {
      ID <<= 8;
      ID |= mfrc522.uid.uidByte[i];
    }
    // выведем
    lcd.clear();
    draw_info(ID);
    Serial.print("User: ");
    Serial.println(ID);
  }

  metalHandler();             //! Опрос показаний металлодетектора
  
  if (flagAuth){              // Если авторизация прошла успешно
    if (check_metal()){
      Serial.println("GO to metal box");
      servoPulse(servo1Pin,90);    
      servoPulse(servo2Pin,90);
      delay(500);             //? Задержка на падение бутылки от металла
    } else {
      Serial.println("GO to glass check");
      servoPulse(servo1Pin,0);
      servoPulse(servo2Pin, 90);
      //delay(200);           //? Задержка на движение бутылки от металла???
      if (check_glass()){
        Serial.println("GO to glass box");
        servoPulse(servo3Pin,90);     
        servoPulse(servo4Pin,90); 
        delay(2000);          //! Задержка на движение бутылки от металла
      } else {
        Serial.println("---It is plastic material!");
        Serial.println("GO to plastic box");
        draw_message("Material: ","plastic");
        servoPulse(servo3Pin,90);     
        servoPulse(servo4Pin,0); 
        delay(500);          //? Задержка на движение бутылки
      };
    };
    flagAuth = false;        // Завершение операций по пользователю,
                             // Сброс признака авторизации в "False"
    Serial.println("_____Auth OFF_____");
    Serial.println(" ");
    //draw_intro();            // Рисуем заставку
  }
}

// Проверка на металл
bool check_metal (){
  Serial.println(" ");
  Serial.println("Start metal check");
  Serial.print("Metal moment value: ");
  Serial.println(metalMoment_Value);

  // Условие на проверку металла
  if (metalMoment_Value <= metalActiv_Value){
    Serial.println("---Metal detected!");
    Serial.println("Stop metal check");
    Serial.println(" ");
    draw_message("Material: ","metal");
    return true;
  } else {
    Serial.println("---NO metal detected!");
    Serial.println("Stop metal check");
    Serial.println(" ");
    return false;
    } 
}

// Проверка на стекло
bool check_glass (){
  flagListening = true;         // Установка признака слушателя
  Serial.println(" ");
  Serial.println("Start listen glass");
  delay(500);                   /* Задержка на движение бутылки к датчику звука */ //! Подобрать значение
  Serial.println("Stop listen glass");
  flagListening = false;        // Сброс признака слушателя

  // Условие на проверку стекла
  if (flagSoundLikeGlass){
    Serial.println("---Sounds like glass");
    Serial.println(" ");
    draw_message("Material: ","glass");
    flagSoundLikeGlass = false; // Сброс признака звука стекла
    return true;
  } else {
    Serial.println("---No glass");
    Serial.println(" ");
    flagSoundLikeGlass = false; // Сброс признака звука стекла 
    return false; 
  }
}

// Обработчик во время delay(x);
void yield() {
  if (flagListening){                              // Слушаем? 
    soundMoment_Value = analogRead(soundPin);      // ±515 в технопарке!
    if (soundMoment_Value <= soundActiv_Value){    //! Подобрать значение "soundActiv_Value" в объявлении переменной
      flagSoundLikeGlass = true;                   // Установка признака звука стекла 
      Serial.print("Glass moment value: ");
      Serial.println(soundMoment_Value);
    }
  }  
  if(flagRotate){
    // convert angle to 500-2480 pulse width
    digitalWrite(pin, HIGH); // set the level of servo pin as high
    delayMicroseconds(pulseWidth); // delay microsecond of pulse width
    digitalWrite(pin, LOW); // set the level of servo pin as low
  }
}

void draw_message(String mes1, String mes2){
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print(mes1);
  lcd.setCursor(0, 1); 
  lcd.print(mes2);
}

// Отрисовка на дисплее начального сообщения
void draw_info(uint32_t id){
  if (id == 1634854192){
    lcd.setCursor(0, 0); 
    lcd.print("Ivan Ershov ");
    lcd.setCursor(0, 1); 
    lcd.print("ID: " + String(id,HEX) );
    flagAuth = true;    // Признак удачной авторизации
    Serial.println("_____Auth ON_____");
  }

  if (id == 1779677123){
    lcd.setCursor(0, 0); 
    lcd.print("Igor Expert");
    lcd.setCursor(0, 1); 
    lcd.print("ID: " + String(id,HEX));
    flagAuth = true;    // Признак удачной авторизации
    Serial.println("_____Auth ON_____");
  }
}

// Отрисовка на дисплее начального сообщения
void draw_intro(){
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Hello, i'm a inn");
  lcd.setCursor(0, 1); 
  lcd.print("ovation musorka");
} 

// Работа по прерыванию на Таймере 1
SIGNAL(TIMER1_COMPA_vect)
{
  OCR1A += 1000;
  t = micros() - t0;
  t0 += t;
  tflag = 1;
}

// Модуль float f
float absf(float f)
{
  if (f < 0.0)
    return -f;
  else
    return f;
}

// Обработчик значения с металлодетектора
void metalHandler(){
  if (tflag)  {
    // Serial.println(f);
    f = f * 0.85 + absf(t - v0) * 0.15;  // фильтруем сигнал 
    metalMoment_Value = f;
    tflag = 0;                           //-Reset flag
  }
}

// Движение сервопривода
void servoPulse(int pin, int angle)
{
  flagRotate = true;
  pulseWidth = (angle * 11) + 500; 
	delay(20 - pulseWidth / 1000);
  flagRotate = false;
}