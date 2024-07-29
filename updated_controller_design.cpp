#include <Wire.h>
#include "DS3231.h"
#include <EEPROM.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include <LiquidCrystal_I2C.h>

// LCD settings
#define LCD_CHARS   16
#define LCD_LINES   2
LiquidCrystal_I2C lcd(0x27, PC4, PC5); // address, sda, scl (27,28) in Atmega8

// Encoder settings
ClickEncoder *encoder;
int16_t last, value;
int menuShift = 100;

// RTC
DS3231 rtc;

// Relay settings
const int RELAY_PIN = 4;
const int EMERGENCY_BUTTON_PIN = 5;
bool relayOn = false;
unsigned long relayStartTime = 0;
int countdown;
int duration = 20; // 20 minutes operation time

// Time settings
int hour, minute, second;
int current = 0;
int voltage = 0;

// EEPROM settings
const int startAddr = 0;

// Settings menu state
enum class SettingsState { TIME, DURATION, SAVE, CANCEL, DELETE_ALL };
SettingsState settingsState = SettingsState::TIME; // keeps track of the current state

// ISR for the encoder
void timerIsr() {
  encoder->service();
}

// Setup function
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
  pinMode(EMERGENCY_BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, LOW);

  EEPROM.get(startAddr, hour);
  EEPROM.get(startAddr + sizeof(int), minute);
  EEPROM.get(startAddr + 2 * sizeof(int), duration);
  EEPROM.get(startAddr + 3 * sizeof(int), current);
  EEPROM.get(startAddr + 4 * sizeof(int), voltage);

  displayDefault();
}

// Loop function
void loop() {
  findTime();
  ClickEncoder::Button b = encoder->getButton();

  if (b == ClickEncoder::Clicked) {
    if (settingsState == SettingsState::SAVE) {
      saveSettings();
      displayDefault();
    } else if (settingsState == SettingsState::CANCEL) {
      displayDefault();
    } else if (settingsState == SettingsState::DELETE_ALL) {
      deleteAllSettings();
      displayDefault();
    } else {
      settingsState = static_cast<SettingsState>((static_cast<int>(settingsState) + 1) % 5);
    }
  } else if (b == ClickEncoder::DoubleClicked) {
    enterSettings();
  } else if (b == ClickEncoder::Held) {
    deleteAllSettings();
  }

  if (digitalRead(EMERGENCY_BUTTON_PIN) == LOW) {
    emergencyShutdown();
  }

  if (relayOn) {
    if (millis() - relayStartTime >= 60000) {
      countdown--;
      relayStartTime += 60000;
      if (countdown <= 0 || current > 15) {
        relayOn = false;
        digitalWrite(RELAY_PIN, LOW);
      }
    }
  } else {
    checkStartupConditions();
    displayDefault();
  }

  // Turn on the relay automatically at 8 AM
  if (hour == 8 && minute == 0 && second == 0) {
    turnOnRelay();
  }

  updateDisplay();
}

// Function to read the current time
void findTime() {
  DateTime now = rtc.now();
  hour = now.hour();
  minute = now.minute();
  second = now.second();
}

// Function to display the default information
void displayDefault() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (hour < 10) lcd.print('0');
  lcd.print(hour);
  lcd.print(':');
  if (minute < 10) lcd.print('0');
  lcd.print(minute);
  lcd.print("     |   ");
  lcd.print(voltage);
  lcd.print(" V");
  lcd.setCursor(0, 1);
  lcd.print(duration);
  lcd.print(" min   |   ");
  lcd.print(current);
  lcd.print(" A");
}

// Function to display the confirmation options
void displayConfirmation() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (settingsState == SettingsState::SAVE) {
    lcd.print("SAVE        ✓");
  } else {
    lcd.print("SAVE         ");
  }
  lcd.setCursor(0, 1);
  if (settingsState == SettingsState::CANCEL) {
    lcd.print("SETTINGS     X");
  } else {
    lcd.print("SETTINGS      ");
  }

  // Display the delete all button
  if (settingsState == SettingsState::DELETE_ALL) {
    lcd.setCursor(0, 1);
    lcd.print("DELETE ALL    ✓");
  }
}

// Function to display the settings
void displaySettings() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TIME     |    DUR");
  lcd.setCursor(0, 1);
  if (settingsState == SettingsState::TIME) {
    lcd.print(">");
  } else {
    lcd.print(" ");
  }
  if (hour < 10) lcd.print('0');
  lcd.print(hour);
  lcd.print(':');
  if (minute < 10) lcd.print('0');
  lcd.print(minute);
  lcd.print("    |    ");
  lcd.print(duration);
  lcd.print(" min");
  displayConfirmation();
}

// Function to enter the settings menu
void enterSettings() {
  lcd.clear();
  settingsState = SettingsState::TIME;

  while (true) {
    displaySettings();

    ClickEncoder::Button b = encoder->getButton();
    if (b == ClickEncoder::Clicked) {
      if (settingsState == SettingsState::SAVE) {
        saveSettings();
        displayDefault();
        return;
      } else if (settingsState == SettingsState::CANCEL) {
        displayDefault();
        return;
      } else if (settingsState == SettingsState::DELETE_ALL) {
        deleteAllSettings();
        displayDefault();
        return;
      } else {
        settingsState = static_cast<SettingsState>((static_cast<int>(settingsState) + 1) % 5);
      }
    } else if (b == ClickEncoder::DoubleClicked) {
      if (settingsState == SettingsState::DELETE_ALL) {
        deleteAllSettings();
        displayDefault();
        return;
      } else {
        saveSettings();
        displayDefault();
        return;
      }
    }

    value += encoder->getValue();
    if (value != last) {
      last = value;
      if (settingsState == SettingsState::TIME) {
        hour = (hour + value) % 24;
        minute = (minute + value) % 60;
      } else if (settingsState == SettingsState::DURATION) {
        duration = (duration + value) % 61;
      }
    }
  }
}

// Function to delete all settings
void deleteAllSettings() {
  EEPROM.put(startAddr, 0);
  EEPROM.put(startAddr + sizeof(int), 0);
  EEPROM.put(startAddr + 2 * sizeof(int), 0); // Reset duration
  EEPROM.put(startAddr + 3 * sizeof(int), 0); // Reset current
  EEPROM.put(startAddr + 4 * sizeof(int), 0); // Reset voltage

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("All settings");
  lcd.setCursor(0, 1);
  lcd.print("deleted.");
  delay(2000);
}

// Function to save the settings
void saveSettings() {
  EEPROM.put(startAddr, hour);
  EEPROM.put(startAddr + sizeof(int), minute);
  EEPROM.put(startAddr + 2 * sizeof(int), duration);
  EEPROM.put(startAddr + 3 * sizeof(int), current);
  EEPROM.put(startAddr + 4 * sizeof(int), voltage);
}

// Function to turn on the relay
void turnOnRelay() {
  if (voltage >= 50 && current >= 4) {
    relayOn = true;
    relayStartTime = millis();
    countdown = duration * 60; // convert to seconds
    digitalWrite(RELAY_PIN, HIGH);
  }
}

// Function to check startup conditions
void checkStartupConditions() {
  if (voltage >= 50 && current >= 4) {
    turnOnRelay();
  }
}

// Function for emergency shutdown
void emergencyShutdown() {
  relayOn = false;
  digitalWrite(RELAY_PIN, LOW);
}

// Function to handle power cut
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
