#include <WiFi.h>                 //WiFi қосылым үшін керек кітапхана
#include <esp_now.h>              //ESP-NOW арнасымен деректер алмасу
#include <WiFiClientSecure.h>     //Қауіпсіз интернет қосылымы
#include <UniversalTelegramBot.h> //Telegram ботты басқару
#include <ArduinoJson.h>          //JSON деректерін өңдеу

const char* ssid = "INSERT_YOUR_SSID"; //WiFi желісінің атауы
const char* password = "INSERT_YOUR_WIFI_PASSWORD";      //WiFi құпия сөзі

#define BOTtoken "INSERT_YOUR_BOT_TOKEN" //Telegram бот токені
#define CHAT_ID "INSERT_YOUR_TELEGRAM_ID" //Telegram чат ID

uint8_t broadcastAddress[]={0x4C, 0xEB, 0xD6, 0x1F, 0xA5, 0x66}; //Деректер жіберілетін құрылғының MAC мекенжайы

typedef struct struct_message {  //Жіберілетін деректер құрылымы
  int analogValue;               //Газ сенсорының мәні
}struct_message;

struct_message myData; //Деректер объектісі

const int smokePin=36; //Газ сенсоры піні (GPIO36)
int minValue=4095;     //Минималды мән(сенсордың максималды диапазонынан басталады)
int maxValue=0;        //Максималды мән
unsigned long lastSensorRead=0;   //Соңғы сенсор оқу уақыты
const int sensorReadInterval=500; //Сенсорды оқу аралығы (0.5 секунд)

WiFiClientSecure client; //Қауіпсіз клиент
UniversalTelegramBot bot(BOTtoken, client); //Telegram бот объектісі
int botRequestDelay=1000; //Ботты тексеру жиілігі (1 секунд)
unsigned long lastTimeBotRan; //Соңғы бот тексеру уақыты

esp_now_peer_info_t peerInfo; //ESP-NOW қабылдағыш құрылғы туралы ақпарат

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  //Деректер жіберу нәтижесін басып шығару
  Serial.print("Жіберу статусы: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Сәтті" : "Сәтсіз");
}

void connectWiFi() {
  Serial.println("WiFi-ге қосылу...");
  WiFi.mode(WIFI_STA); //WiFi-ді станция режімінде іске қосу
  WiFi.begin(ssid, password); //WiFi желісіне қосылу
  
  //Қосылу әрекетінің таймауты (20 секунд)
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-startAttemptTime < 20000) {
    delay(500);
  }

  if (WiFi.status()==WL_CONNECTED) {
    Serial.println("Қосылды! IP: " +WiFi.localIP().toString());
  } else {
    Serial.println("Қосылым сәтсіз!");
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i=0; i<numNewMessages; i++) {
    String chat_id=String(bot.messages[i].chat_id);
    // Чат ID тексеру (рұқсатты пайдаланушылар)
    if (chat_id!=CHAT_ID) {
      bot.sendMessage(chat_id,"Рұқсат жоқ","");
      continue;
    }

    String text=bot.messages[i].text;
    String from_name=bot.messages[i].from_name;

    // Командаларды өңдеу
    if (text=="/start") {
      String welcome="Қош келдіңіз, " + from_name + "!\n";
      welcome += "Командалар:\n/value-Сенсор мәні\n/status-Мин/Макс\n/level-Газ деңгейі";
      bot.sendMessage(chat_id,welcome,"");
    }

    if (text=="/value") {
      int sensorValue=analogRead(smokePin);
      bot.sendMessage(chat_id,"Газ мәні: "+String(sensorValue), "");
    }

    if (text=="/status") {
      String message = "Мин: "+String(minValue)+"\nМакс: "+String(maxValue);
      bot.sendMessage(chat_id,message,"");
    }

    if (text=="/level"){
      int sensorValue=analogRead(smokePin);
      String status;
      if (sensorValue<1000) status="ҚАУІПСІЗ";
      else if (sensorValue <= 2000) status="ЕСКЕРТУ";
      else status="ҚАУІПТІ!";
      bot.sendMessage(chat_id,"Деңгей: " + status,"");
    }
  }
}

void setup() {
  Serial.begin(115200); //Сериялық портты іске қосу
  pinMode(smokePin, INPUT); //Сенсор пінін кіріс ретінде орнату
  
  connectWiFi(); //WiFi-ге қосылу
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); //Telegram сертификаты

  //ESP-NOW инициализациясы
  if (esp_now_init()!=ESP_OK) {
    Serial.println("ESP-NOW инициализациясы сәтсіз");
    return;
  }

  //Деректер жіберу функциясын тіркеу
  esp_now_register_send_cb(OnDataSent);
  
  //Қабылдағыш құрылғыны конфигурациялау
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel=0;
  peerInfo.encrypt=false;
  
  //Қабылдағыш құрылғыны қосу
  if(esp_now_add_peer(&peerInfo)!=ESP_OK) {
    Serial.println("Қабылдағыш қосу сәтсіз");
    return;
  }
}

void loop() {
  //WiFi қосылымын тексеру
  if(WiFi.status()!=WL_CONNECTED) {
    connectWiFi();
    delay(1000);
  }

  //Telegram ботты тексеру (1 секундтан кейін)
  if (millis()>lastTimeBotRan+botRequestDelay) {
    int numNewMessages=bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages){
      handleNewMessages(numNewMessages);
    }
    lastTimeBotRan=millis();
  }

  //Сенсорды уақыт аралығында тексеру
  if (millis()>lastSensorRead + sensorReadInterval) {
    myData.analogValue=analogRead(smokePin);
    
    //Минималды/максималды мәндерді жаңарту
    minValue=min(minValue,myData.analogValue);
    maxValue=max(maxValue,myData.analogValue);

    //Деректерді ESP-NOW арқылы жіберу
    esp_err_t result=esp_now_send(broadcastAddress, (uint8_t*)&myData, sizeof(myData));
    
    // Автоматты ескертулер
    if (myData.analogValue>=1000) {
      String alertMsg;
      if (myData.analogValue<=2000) {
        alertMsg="ЕСКЕРТУ: Газ ағып кетуі мүмкін!";
      } else {
        alertMsg="ҚАУІП: Газ концентрациясы жоғары!";
      }
      bot.sendMessage(CHAT_ID, alertMsg+" Мән: "+String(myData.analogValue),"");
    }

    lastSensorRead=millis();
  }
}
