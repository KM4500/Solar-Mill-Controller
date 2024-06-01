#include <Wire.h>
#include "DS3231.h"
#include <EEPROM.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include <LiquidCrystal_I2C.h>
//lcd settings
#define LCD_CHARS   16
#define LCD_LINES    2
LiquidCrystal_I2C lcd(0x27,PC4,PC5); //address, sda, scl (27,28) in Atmega8
//encoder settings
ClickEncoder *encoder;
int16_t last, value;
int menuShift = 100;
//rtc
DS3231 rtc;
//relay settings
const int RELAY_PIN = 4;
bool relayOn = false;
unsigned long relayStartTime = 0;
int countdown;
//time settings
int hour, minute, second;
int duration = 0;
int current = 0;
int voltage = 0;
//EEPROM settings
const int startAddr = 0;

void timerIsr() {  //updates the encoder's state regularly
  encoder->service();
}

//set-up functions
void setup() {
  Serial.begin(9600);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  rtc.begin();

  encoder = new ClickEncoder(2, 3, 4);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  last = -1;

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  EEPROM.get(startAddr, hour);
  EEPROM.get(startAddr + sizeof(int), minute);
  EEPROM.get(startAddr + 2 * sizeof(int), duration);
  EEPROM.get(startAddr + 3 * sizeof(int), current);
  EEPROM.get(startAddr + 4 * sizeof(int), voltage);

  displayDefault();
}

void loop() {
  findTime();
  ClickEncoder::Button b = encoder->getButton();

  if (b == ClickEncoder::Clicked) {
    relayOn = !relayOn;
    if (relayOn) {
      digitalWrite(RELAY_PIN, HIGH);
      relayStartTime = millis();
      countdown = duration * 60;
    } else {
      digitalWrite(RELAY_PIN, LOW);
    }
    displayDefault();
  }

  if (relayOn) {
    if (millis() - relayStartTime >= 60000) {
      countdown--;
      relayStartTime += 60000;
      if (countdown <= 0) {
        relayOn = false;
        digitalWrite(RELAY_PIN, LOW);
      }
    }
    displayRelayOn();
  } else {
    displayDefault();
  }
}

void findTime() { //reads the current time and updates
  DateTime now = rtc.now();
  hour = now.hour();
  minute = now.minute();
  second = now.second();
}

void displayDefault() {  //displays current and duration on lcd
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  if (hour < 10) lcd.print('0');
  lcd.print(hour);
  lcd.print(':');
  if (minute < 10) lcd.print('0');
  lcd.print(minute);

  lcd.setCursor(8, 0);
  lcd.print("Dur: ");
  lcd.print(duration);
  lcd.print(" min");
  lcd.print(" C: ");
  lcd.print(current);
  lcd.print(" V: ");
  lcd.print(voltage);
}

void displayRelayOn() {  //relay status and countdown on lcd
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Motor On");

  lcd.setCursor(0, 1);
  lcd.print("Time Left: ");
  lcd.print(countdown);
  lcd.print(" sec");
  lcd.print(" C: ");
  lcd.print(current);
  lcd.print(" V: ");
  lcd.print(voltage);
}

 void enterSettings() {
  lcd.clear();
  int settingsShift = 0;

  while (true) {
    ClickEncoder::Button b = encoder->getButton();
    if (b == ClickEncoder::Clicked) {
      if (settingsShift == 2) {
        saveSettings();
        displayDefault();
        return;
      }
      settingsShift = (settingsShift + 1) % 3;
    }

    value += encoder->getValue();
    if (value != last) {
      last = value;
      if (settingsShift == 0) {
        hour = (hour + value) % 24;
      } else if (settingsShift == 1) {
        minute = (minute + value) % 60;
      } else if (settingsShift == 2) {
        duration = (duration + value) % 61;
      }

      lcd.setCursor(0, 0);
      lcd.print("Set Time: ");
      if (hour < 10) lcd.print('0');
      lcd.print(hour);
      lcd.print(':');
      if (minute < 10) lcd.print('0');
      lcd.print(minute);

      lcd.setCursor(8, 0);
      lcd.print("Dur: ");
      lcd.print(duration);
      lcd.print(" min");
    }
  }
}
void saveSettings() {
  EEPROM.put(startAddr, hour);
  EEPROM.put(startAddr + sizeof(int), minute);
  EEPROM.put(startAddr + 2 * sizeof(int), duration);
  EEPROM.get(startAddr + 3 * sizeof(int), current);
  EEPROM.get(startAddr + 4 * sizeof(int), voltage);
}
void powerCut() {
  int previousHour, previousMinute, previousDuration, previousCurrent, previousVoltage;
  EEPROM.get(startAddr, previousHour);
  EEPROM.get(startAddr + sizeof(int), previousMinute);
  EEPROM.get(startAddr + 2 * sizeof(int), previousDuration);
  EEPROM.get(startAddr + 3 * sizeof(int), previousCurrent);
  EEPROM.get(startAddr + 4 * sizeof(int), previousVoltage);

  if (previousDuration > 0) {
    relayOn = true;
    relayStartTime = millis();
    countdown = previousDuration * 60;
    digitalWrite(RELAY_PIN, HIGH);
  }
}




