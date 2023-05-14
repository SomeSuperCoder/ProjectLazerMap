#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>
#include <Servo.h>
#include <math.h>

#define WIFI_SSID "VR"
#define WIFI_PASSWORD "Kvant1943"

#define servopin D0
#define relepin D6
#define lazerpin D7

#define joystick_speed 1

WiFiClientSecure secured_client;

#define BOT_TOKEN "6180741413:AAE6DnHWEV6vEviP6CYsW6RziIJWKY3f1Xk"
// #define CHAT_ID "1375299437"
unsigned long BOT_MTBS = 100;

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

unsigned long bot_lasttime;

enum BotState {
  NONE,
  ENTER_CITY_NAME_ADD,
  ENTER_CITY_SPX_ADD,
  ENTER_CITY_SPY_ADD,
  ENTER_CITY_NAME_RM,
  ENTER_DEGS_PER_JOY,
  ENTER_CITY_NAME_SHOW
};

BotState botState = BotState::NONE;

struct City {
  String name;
  int spY;
  int spX;
  bool isFilled = false;
};


#define MAX_CITY_AMOUNT 20
City city_list[MAX_CITY_AMOUNT];
City currently_addable_city;

Servo servoH;
Servo servoV;

int degs_per_joy = 10;
#define accuracy 10

int max_top = 0;
int max_bottom = 180;
int max_right = 180;
int max_left = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  EEPROM.begin(1024);

  pinMode(D2, OUTPUT);
  digitalWrite(D2, 1);
  delay(1000);
  digitalWrite(D2, 0);

  pinMode(relepin, OUTPUT);
  pinMode(lazerpin, OUTPUT);

  Serial.println();

  configTime(0, 0, "pool.ntp.org");
  secured_client.setTrustAnchors(&cert);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  digitalWrite(D2, 1);
  delay(3000);
  digitalWrite(D2, 0);
  servoH.attach(servopin);
  servoV.attach(servopin);

  // check first start
  if (EEPROM.read(10) != 'w') {
    EEPROM.put(11, city_list);
    EEPROM.put(10, 'w');
    EEPROM.commit();
    EEPROM.get(11, city_list);
  } else {
    EEPROM.get(11, city_list);
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  // if (analogRead(joystick_pin) != 0) {
  //   servo.write(servo.read() + joystick_speed);
  // }

  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {

    String text = bot.messages[i].text;

    // handle commands

    if (text == "/start") {
      bot.sendMessage(bot.messages[i].chat_id, "Привет!\nЯ бот для управления интерактивеой картой Чувашской Республики.");
      return;
    }

    if (text == "/add_city") {
      bot.sendMessage(bot.messages[i].chat_id, "Введите название города");
      botState = BotState::ENTER_CITY_NAME_ADD;
      return;
    }

    if (text == "/list") {
      String result = "Список городов:\n";
      result += "===============\n";
      for (int i = 0; i < MAX_CITY_AMOUNT; i++) {
        if (city_list[i].isFilled) {
          result += "Название: " + city_list[i].name + "\n";
          result += "Сеточных едениц по оси x: " + String(city_list[i].spX) + "\n";
          result += "Сеточных едениц по оси y: " + String(city_list[i].spY) + "\n";
          result += "===============\n";
        }
      }
      bot.sendMessage(bot.messages[i].chat_id, result);
      return;
    }

    if (text == "/rm_city") {
      bot.sendMessage(bot.messages[i].chat_id, "Введите название города");
      botState = BotState::ENTER_CITY_NAME_RM;
      return;
    }

    if (text == "/joystick") {
      joystick(i);
      return;
    }

    if (text == "/up") {
      up(i);
      return;
    }
    if (text == "/down") {
      down(i);
      return;
    }
    if (text == "/left") {
      left(i);
      return;
    }
    if (text == "/right") {
      right(i);
      return;
    }

    if (text == "/degs_per_joy") {
      bot.sendMessage(bot.messages[i].chat_id, "Введите чувствительность");
      botState = BotState::ENTER_DEGS_PER_JOY;
      return;
    }

    if (text == "/show_city") {
      bot.sendMessage(bot.messages[i].chat_id, "Введите назавание города");
      botState = BotState::ENTER_CITY_NAME_SHOW;
      return;
    }

    if (text == "/set_max_right") {
      max_right = readServoH(); 
      return; 
    }

    if (text == "/set_max_left") {
      max_right = readServoH();
      return;
    }

    if (text == "/set_max_top") {
      max_top = readServoV();
      return;      
    }

    if (text == "/set_max_bottom") {
      max_bottom = readServoV();
      return;
    }

    if (text == "/0") {
      writeServoH(0);
      delay(500);
      writeServoV(0);
      delay(500);
      return;
    }

    if (text == "/180") {
      writeServoH(180);
      delay(500);
      writeServoV(180);
      delay(500);
      return;
    }

    if (text == "/lazer_on") {
      digitalWrite(lazerpin, 1);
      bot.sendMessage(bot.messages[i].chat_id, "The lazer is on!");
      return;
    }

    if (text == "/lazer_off") {
      digitalWrite(lazerpin, 0);
      bot.sendMessage(bot.messages[i].chat_id, "The lazer is off!");
      return;
    }

    // handle states
    if (botState == BotState::ENTER_CITY_NAME_ADD) {
      currently_addable_city.name = text;
      // currently_addable_city.spX = round((readServoH() / calcSpX()) * readServoH());
      // // currently_addable_city.spY = round(calcSpY() * readServoV());
      // currently_addable_city.spY = round((readServoV() / calcSpY()) * readServoV());
      
      currently_addable_city.spX = readServoH();
      currently_addable_city.spY = readServoV();
      
      currently_addable_city.isFilled = true;
      // bot.sendMessage(bot.messages[i].chat_id, "Введите spX");
      // botState = BotState::ENTER_CITY_SPX_ADD;
      add_city();
      bot.sendMessage(bot.messages[i].chat_id, "Город успешно добавлен!");
      botState = BotState::NONE;
      return;
    }

    if (botState == BotState::ENTER_CITY_SPX_ADD) {
      currently_addable_city.spX = text.toInt();
      bot.sendMessage(bot.messages[i].chat_id, "Введите spY");
      botState = BotState::ENTER_CITY_SPY_ADD;
      return;
    }

    if (botState == BotState::ENTER_CITY_SPY_ADD) {
      currently_addable_city.spY = text.toInt();
      bot.sendMessage(bot.messages[i].chat_id, "Город успешно добавлен!");
      botState = BotState::NONE;
      currently_addable_city.isFilled = true;
      add_city();
      return;
    }

    if (botState == BotState::ENTER_CITY_NAME_RM) {
      for (int i = 0; i < MAX_CITY_AMOUNT; i++) {
        if (city_list[i].name == text) {
          city_list[i].name = "";
          city_list[i].spX = 0;
          city_list[i].spY = 0;
          city_list[i].isFilled = false;
          break;
        }
      }
      EEPROM.put(11, city_list);
      EEPROM.commit();
      bot.sendMessage(bot.messages[i].chat_id, "Город успешно удалён!");
      return;
    }

    if (botState == BotState::ENTER_DEGS_PER_JOY) {
      degs_per_joy = text.toInt();
      bot.sendMessage(bot.messages[i].chat_id, "Чувствительность успешно установлена");
      botState = BotState::NONE;
    }

    if (botState == BotState::ENTER_CITY_NAME_SHOW) {
      show_city(text);
      bot.sendMessage(bot.messages[i].chat_id, "Внимание на карту!");
      botState = BotState::NONE;
      return;    
    }

    EEPROM.put(11, city_list);
    EEPROM.commit();
    return;
  }
}

void add_city() {
  for (int i = 0; i < MAX_CITY_AMOUNT; i++) {
    if (!city_list[i].isFilled) {
      city_list[i] = currently_addable_city;
      break;
    }
  }

  currently_addable_city.name = "";
  currently_addable_city.spX = 0;
  currently_addable_city.spY = 0;
  currently_addable_city.isFilled = false;

  EEPROM.put(11, city_list);
  EEPROM.commit();
}

void joystick(int i) {
  bot.sendMessage(bot.messages[i].chat_id, "#####/up######\n##############\n#/left###/right###\n##############\n####/down#####");
}
void left(int i) {
  writeServoH(readServoH() - degs_per_joy);
  joystick(i);
}

void right(int i) {
  writeServoH(readServoH() + degs_per_joy);
  joystick(i);
}

void up(int i) {
  writeServoV(readServoV() - degs_per_joy);
  joystick(i);
}

void down(int i) {

  writeServoV(readServoV() + degs_per_joy);
  joystick(i);
}

void writeServoH(int value) {
  digitalWrite(relepin, 0);
  delay(100);
  servoH.write(value);
}

int readServoH() {
  digitalWrite(relepin, 0);
  delay(100);
  return servoH.read();
}

void writeServoV(int value) {
  digitalWrite(relepin, 1);
  delay(100);
  servoV.write(value);
}

int readServoV() {
  digitalWrite(relepin, 1);
  delay(100);
  return servoV.read();
}

int calcSpX() {
  return (max_right - max_left) / accuracy;
}

int calcSpY() {
  return (max_bottom - max_top) / accuracy;
}

void show_city(String name) {
  for (int i = 0; i < MAX_CITY_AMOUNT; i++) {
    if (city_list[i].isFilled && city_list[i].name == name) {
      // int spX = city_list[i].spX;      
      // int spY = city_list[i].spY;
      // // formula1: city_list[i].spX = round(spX * readServoH())
      // // formula2: spX = city_list[i].spX / readServoH()s
      // // int degX = calcSpX() * spX;
      // // int degY = calcSpY() * spY;
      // int degX = spX;
      // int degY = spY;

      writeServoH(city_list[i].spX);
      delay(500);
      writeServoV(city_list[i].spY);
      delay(500);
    }
  }
}