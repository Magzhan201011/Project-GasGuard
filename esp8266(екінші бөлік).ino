#include <ESP8266WiFi.h>  //ESP8266 модуліне Wi-Fi қосылу үшін керек кітапхана
#include <espnow.h>       //Теңіздегі ESP-NOW хабарламасын қабылдауға арналған кітапхана
#include <Stepper.h>      //Шаговый моторды басқаруға арналған кітапхана

// Пинтердің тағайындалуы:
const int buzzer=D1;    //Дыбыс сигналы үшін пин (buzzer)
const int blue=D2;      //Көк түсті RGB светодиод үшін пин
const int green=D5;     //Жасыл түсті RGB светодиод үшін пин
const int red=D6;       //Қызыл түсті RGB светодиод үшін пин
const int steps=2048;   //Мотордың бір айналым жасауы үшін қажетті қадамдар саны

//Моторды D3, D4, D7, D8 пиндеріне қосамыз (ULN2003 драйвері арқылы)
Stepper motor(steps,D3,D4,D7,D8);

// Айнымалылар:
int sensor=0;               //Газ сенсорынан келетін мән (ESP-NOW арқылы)
bool ledOn=false;           //Диодтың жануы/сөнуі
bool sirenOn=false;         //Сиренаның қазіргі күйі
unsigned long lastLed=0;    //Светодиодтың соңғы рет ауыстырылған уақыты
unsigned long lastSiren=0;  //Сиренаның соңғы рет ауыстырылған уақыты
bool go=false;              //Моторды айналдыру керек пе?
int turns=0;                //Жасалған қадамдар саны
unsigned long lastTurn=0;   //Соңғы қадам жасалған уақыт
bool forward=true;          //Айналу бағыты (алға/артқа)

// Деректерді қабылдағанда шақырылатын функция
void OnDataRecv(u8 *mac, u8 *data, u8 len) {
  memcpy(&sensor,data,sizeof(sensor));               //Келген деректерді sensor айнымалысына сақтау
  Serial.println("Алынған мән: " + String(sensor));  //Мәнді портқа шығару
}

void setup(){
  Serial.begin(115200);   //Сериялық портты іске қосу (бағдарламалау үшін)
  motor.setSpeed(5);      //Мотордың жылдамдығын орнату (5 айн/мин)

  // Пинтерді шығыс ретінде орнату:
  pinMode(buzzer,OUTPUT);
  pinMode(red,OUTPUT);
  pinMode(green,OUTPUT);
  pinMode(blue,OUTPUT);

  // Барлық диодтарды өшіру:
  analogWrite(red,0);
  analogWrite(green,0);
  analogWrite(blue,0);

  // Wi-Fi және ESP-NOW баптаулары:
  WiFi.mode(WIFI_STA);                   //Wi-Fi станциясы ретінде жұмыс істеу
  WiFi.disconnect();                     //Барлық желілерден ажырату
  if (esp_now_init()!=0) return;         //ESP-NOW-ды іске қосу
  esp_now_register_recv_cb(OnDataRecv);  //Деректерді қабылдау функциясын тіркеу
}

void loop() {
  // Шаговый моторды басқару:
  if(sensor<1000) {   //Газ қауіпсіз деңгейде
    go=false;         //Моторды тоқтату
    turns=0;          //Қадамдарды нөлге қайтару
  } 
  else if (sensor>=1000 && turns==0 && !go) {  //Газ шекті деңгейден асып кетті
    go=true;                                   //Моторды іске қосу
    lastTurn=millis();                         //Басталу уақытын есте сақтау
  }

  // Моторды 1 секунд сайын айналдыру
  if (go && millis()-lastTurn>=1000) {
    motor.step(forward ? 64 : -64);  //Алға бағытта 64 қадам немесе артқа -64 қадам
    turns +=64;                      //Жасалған қадамдар санын көбейту
    
    //10 айналым жасалғанда (2048 * 10 = 20480 қадам)
    if (turns>=steps*10) {
      go=false;          //Айналдыруды тоқтату
      turns=0;           //Қадамдарды нөлге қайтару
      forward =!forward; //Бағытты өзгерту
    }
  }

  //RGB диодты басқару:
  if (millis()-lastLed>=200) {  //200 миллисекунд сайын
    ledOn=!ledOn;               //Диод күйін өзгерту (жану/сөну)
    
    //Газдың деңгейіне байланысты:
    if (sensor<1000) {  //Қауіпсіз
      analogWrite(blue,ledOn ? 255 : 0);  //Көк түсті жыпылықтату
    } 
    else if (sensor>=1000 && sensor<2000) {  //Орташа деңгей
      analogWrite(green,ledOn ? 255 : 0);  //Жасыл түсті жыпылықтату
    } 
    else {  //Жоғары деңгей (қауіпті)
      analogWrite(red, ledOn ? 255 : 0);  //Қызыл түсті жыпылықтату
    }
    
    lastLed=millis();  //Соңғы ауыстыру уақытын есте сақтау
  }

  // Зуммерді басқару:
  if (sensor>1000 && sensor<2000) {  //Орташа деңгей
    tone(buzzer,1000);  //Үздіксіз сигнал (1000 Гц)
  } 
  else if (sensor>=2000) {               //Жоғары деңгей
    if (millis()-lastSiren>500) {        //500 миллисекунд сайын
      sirenOn=!sirenOn;                  //Сирена күйін өзгерту
      tone(buzzer,sirenOn ? 1200 : 800); //1200 Гц және 800 Гц дыбыстарын ауыстыру
      lastSiren=millis();                //Соңғы ауыстыру уақытын есте сақтау
    }
  } 
  else {  //Қауіпсіз деңгей
    noTone(buzzer);  //Дыбысты өшіру
  }
}
