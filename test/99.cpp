#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h> 
#include <Arduino.h>

LiquidCrystal_I2C lcd(0x27,16,2);  // Устанавливаем дисплей

// TODO: поменять пины сервоприводов в соответвии со схемой, смотри сехму во Вконтакте!
const int RST_PIN = 9; // пин RST
const int SS_PIN = 10; // пин SDA (SS)
const int soundPin = A0;
const int servo1Pin = 2; // !
const int servo2Pin = 3; // !
const int servo3Pin = 4; // !
const int servo4Pin = 5; // !

// TODO: поменять заничения уровня в соотвествии со значением во время отладки!
int soundMoment_Value = 0;    // Моментальное значение
int soundActiv_Value = 500;   // Уровень по которому сравниваем //!

int metalMoment_Value = 0;    // Моментальное значение 
int metalActiv_Value = 9999;  // Уровень по которому сравниваем //!

bool flagAuth = false;           // Признак успешной авторизации
bool flagListening = false;      // Признак состояния опрашивания датчика звука
bool flagSoundLikeGlass = false; // Признак звука стелка


#define sound 1              // 0 - в стиле счётчика гейгера, 1 - пищалка верещалка
#define default_mode 1       // если нет переключателя режимов, то по умолчанию стоит: 1 - статический, 0 - динамический
#define sens_change 1        // 0 - нет регулировки чувствительности, 1 - есть
float SENSITIVITY = 1000.0;  // ручная установка чувствительности
//------------НАСТРОЙКИ-------------

#define sens 6

#define SET(x,y) (x |=(1<<y))            //-Bit set/clear macros
#define CLR(x,y) (x &= (~(1<<y)))         // |
#define CHK(x,y) (x & (1<<y))             // |
#define TOG(x,y) (x^=(1<<y))              //-+

float koef = 0.5;
unsigned long t0 = 0;
unsigned long last_step;
int t = 0;
unsigned char tflag = 0;
float clf;
int v0 = 0;
float f = 0;
unsigned int FTW = 0;
unsigned int PCW = 0;

// Создаём объекты ---------------
MFRC522 mfrc522(SS_PIN, RST_PIN); 
Servo Servo1;
Servo Servo2;
Servo Servo3;
Servo Servo4;
//--------------------------------

// Объявление функций-
void draw_info(uint32_t);
void draw_intro();
bool check_metal();
bool check_glass();
float absf(float);
void metalHandler();
// -------------------


//----------------------------------------------------//----------------------------------------------------
//------------НАСТРОЙКИ-------------


// Срабатываем каждые 1000 импульсов с генератора
SIGNAL(TIMER1_COMPA_vect)
{
  OCR1A += 1000;
  t = micros() - t0;
  t0 += t;
  tflag = 1;
}

float absf(float f)
{
  if (f < 0.0)
    return -f;
  else
    return f;
}

//----------------------------------------------------//----------------------------------------------------

void setup()
{
  Serial.begin(9600);     // Инициализация послед. порта
  SPI.begin();            // Инициализация шины SPI

  mfrc522.PCD_Init();     // Инициализация считывателя RC522
  lcd.init();              
  lcd.backlight();        // Включаем подсветку дисплея
  Servo1.attach(servo1Pin);
  Servo2.attach(servo2Pin);
  Servo3.attach(servo3Pin);
  Servo4.attach(servo4Pin);

  //----------------------------------------------------------------//----------------------------------------------------------------

  //-Set up counter1 to count at pin 5
  TCCR1A = 0;
  TCCR1B = 0x07;
  SET(TIMSK1, OCF1A);
  //----------------------------------------------------------------//----------------------------------------------------------------

  //Начальные положения сервоприводов-
  Servo1.write(0);
  Servo2.write(0);
  Servo3.write(0);
  Servo4.write(0);
  //----------------------------------

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
  metalHandler();

  // TODO: Определить положение в градусах "Х" для открытой створки и закрытой, 
  // TODO: исправить градусы в "Servo1.write(Х)"
  
  if (flagAuth){             // Если авторизация прошла успешно
    if (check_metal()){
      Serial.println("GO to metal box");
      Servo1.write(90);      // Открытие створки!
      Servo2.write(90); 
      delay(500);            // ?Задержка на падение бутылки от металла
    } else {
      Serial.println("GO to glass check");
      Servo1.write(90);
      Servo2.write(0);       // Изначальное положение створки!
      delay(200);            // ?Задержка на движение бутылки от металла???
      if (check_glass()){
        Serial.println("GO to glass box");
        Servo3.write(90);
        Servo4.write(90);
        delay(2000);          // !Задержка на движение бутылки от металла
      } else {
        Serial.println("---It is plastic material!");
        Serial.println("GO to plastic box");
        Servo3.write(90);
        Servo4.write(0);
        delay(500);          // ?Задержка на движение бутылки
      };
    };
    flagAuth = false;        // Завершение операций по пользователю,
                             // Сброс признака авторизации в "False"
    Serial.println("_____Auth OFF_____");
    Serial.println(" ");
    draw_intro();            // Рисуем заставку
  }
}

bool check_metal (){
  Serial.println(" ");
  Serial.println("Start metal check");
  Serial.print("Metal moment value: ");
  Serial.println(metalMoment_Value);
  // Условие на проверку металла
  if (metalMoment_Value >= metalActiv_Value){
    Serial.println("---Metal detected!");
    Serial.println("Stop metal check");
    Serial.println(" ");
    return true;
  } else {
    Serial.println("---NO metal detected!");
    Serial.println("Stop metal check");
    Serial.println(" ");
    return false;
    } 
}

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
    flagSoundLikeGlass = false; // Сброс признака звука стекла
    return true;
  } else {
    Serial.println("---No glass");
    Serial.println(" ");
    flagSoundLikeGlass = false; // Сброс признака звука стекла 
    return false; 
  }
}

void yield() {
  if (flagListening){                              // Слушаем? 
    soundMoment_Value = analogRead(soundPin);      // ±515 в технопарке!
    if (soundMoment_Value <= soundActiv_Value){    //! Подобрать значение "soundActiv_Value" в объявлении переменной
      flagSoundLikeGlass = true;                   // Установка признака звука стекла 
      Serial.print("Glass moment value: ");
      Serial.println(soundMoment_Value);
    }
  }  
}

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

void draw_intro(){
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Hello, i'm a inn");
  lcd.setCursor(0, 1); 
  lcd.print("ovation musorka");
} 

void metalHandler(){
  
  if (tflag)  {
    Serial.print("$");
    Serial.print(f);
    Serial.println(";");

    f = f * 0.85 + absf(t - v0) * 0.15;             // фильтруем сигнал 

    if (sens_change)                                // если разрешено внешнее изменение чувствительности
      SENSITIVITY = map(analogRead(sens), 0, 1023, 500, 2000);        // принять с аналогового пина, преобразовать
    clf = f * SENSITIVITY;                          // конвертация частоты в писк
    if (clf > 10000)
      clf = 10000;
    FTW = clf;
    tflag = 0;                //-Reset flag
  }
}


