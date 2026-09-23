// Espressif ESP32 version 1.06, board Wemos Lolin32 lite

// SBUS
#include "sbus.h"  // https://github.com/bolderflight/sbus/tree/main

bfs::SbusData data1;
bfs::SbusRx sbus_rx1(&Serial1, 34, 0, true);  // ESP32 (true = inverted 3.3V)

bfs::SbusData data2;
bfs::SbusRx sbus_rx2(&Serial2, 35, 0, true);  // ESP32 (true = inverted 3.3V)

uint32_t lastSbus1 = 0;
uint32_t lastSbus2 = 0;

// PWM for servos
#include <ESP32Servo.h>  // https://github.com/madhephaestus/ESP32Servo

uint16_t pwmPin[16] = { 14, 27, 26, 25, 33, 32, 19, 13, 16, 17, 5, 18, 23, 4, 2, 15 };
uint16_t failsafe[16] = { 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500 };

Servo servos[16];
uint16_t channels[16];

#define LED_PIN 22

// Watchdog
#include <esp_task_wdt.h>
#define WDT_TIMEOUT 1  // 1 second WDT

#define FAIL_PIN 12  // push button Failsafe (GPIO 12 to GND when pushed)
bool  saveFailsafe = false;

// WiFi / Bluetooth
#include <WiFi.h>
#include <bt.h>

// Preferences
#include <Preferences.h>
Preferences preferences;

void setup() {
  // Desactivation WiFi and Bluetooth
  WiFi.mode(WIFI_OFF);
  btStop();

  Serial.begin(115200);
  delay(1000);
  Serial.println("Program started");

  pinMode(FAIL_PIN, INPUT_PULLUP);
  

  // Preferences
  preferences.begin("sbusBox", false);
  size_t size = preferences.getBytesLength("failsafe");
  if (size == sizeof(failsafe)) {
    preferences.getBytes("failsafe", failsafe, sizeof(failsafe));
  } else {
    preferences.putBytes("failsafe", failsafe, sizeof(failsafe));
  }

  Serial.println("Read preferences :");
  Serial.print("16 failsafe values : ");
  for (int i = 0; i < 16; i++) {
    Serial.print("\t");
    Serial.print(failsafe[i]);
  }
  Serial.println("");

  // SBUS
  sbus_rx1.Begin();
  sbus_rx2.Begin();

  // Servos PWM
  for (int i = 0; i < 16; i++) {
    servos[i].attach(pwmPin[i]);
  }

  // Watchdog
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.println("Prog ready, watchdog set, servos started");

  //failsafe button presses ?
  if (digitalRead(FAIL_PIN) == LOW) 
  {
    saveFailsafe = true;
    Serial.println("will record failsafe values when button released");
  }
}

void loop() {
  esp_task_wdt_reset();

  bool newFrame1 = sbus_rx1.Read();
  if (newFrame1) {
    data1 = sbus_rx1.data();
    lastSbus1 = millis();
  }

  bool newFrame2 = sbus_rx2.Read();
  if (newFrame2) {
    data2 = sbus_rx2.data();
    lastSbus2 = millis();
  }

  // Verification de l'etat des deux voies SBUS
  bool sbus1_ok = ((millis() - lastSbus1) < 100) && !data1.failsafe && !data1.lost_frame;
  bool sbus2_ok = ((millis() - lastSbus2) < 100) && !data2.failsafe && !data2.lost_frame;

  // Mise a jour des servos uniquement lors de la reception d'une nouvelle trame
  if (newFrame1 || newFrame2) {
    if (sbus1_ok) {
      for (int i = 0; i < 16; i++) {
        int pulseWidth = map(data1.ch[i], 172, 1811, 1000, 2000);
        servos[i].write(pulseWidth);
        channels[i] = pulseWidth;
      }
      digitalWrite(LED_PIN, (millis() / 100) % 2 == 0 ? LOW : HIGH);

    } else if (sbus2_ok) {
      for (int i = 0; i < 16; i++) {
        int pulseWidth = map(data2.ch[i], 172, 1811, 1000, 2000);
        servos[i].write(pulseWidth);
        channels[i] = pulseWidth;
      }
      digitalWrite(LED_PIN, (millis() / 500) % 2 == 0 ? LOW : HIGH);

    } else {
      for (int i = 0; i < 16; i++) {
        servos[i].write(failsafe[i]);
      }
      digitalWrite(LED_PIN, LOW);
    }
  }

  // Sauvegarde Failsafe via bouton poussoir
  if ((digitalRead(FAIL_PIN) == HIGH) && (saveFailsafe)) {  // button was released

    Serial.println("failsafe values saved into preferences");

    for (int i = 0; i < 16; i++) {
      failsafe[i] = channels[i];
    }
    preferences.putBytes("failsafe", failsafe, sizeof(failsafe));

    // Clignotement rapide de la LED pour confirmer l'enregistrement
    for (int k = 0; k < 6; k++) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(100);
    }
    saveFailsafe = false;
  }
  delay(1);
}

