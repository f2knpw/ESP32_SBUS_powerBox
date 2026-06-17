// Espressif ESP32 version 1.06, board Wemos Lolin32 lite

//SBUS
#include "sbus.h"  //https://github.com/bolderflight/sbus/tree/main

// two SBUS object, which are on hardware serial ports 1 and 2
bfs::SbusData data1;
bfs::SbusRx sbus_rx1(&Serial1, 34, 0, true);  //ESP32  (true = inverted 3.3V from FSIA6B SBUS pin)

bfs::SbusData data2;
bfs::SbusRx sbus_rx2(&Serial2, 35, 0, true);  //ESP32  (true = inverted 3.3V from FSIA6B SBUS pin)

long lastSbus1;
long lastSbus2;


//PWM for servos
#include <ESP32Servo.h>                                                                                                      //https://github.com/madhephaestus/ESP32Servo
uint16_t pwmPin[16] = { 14, 27, 26, 25, 33, 32, 19, 13, 16, 17, 18, 5, 23, 4, 2, 15 };                                       //contains pins for PWM output for each servo
uint16_t failsafe[16] = { 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500 };  //failsafe servos values for each 16 channels in µs

Servo servos[16];
uint16_t channels[16];

#define LED_PIN 22

//watchdog
#include <esp_task_wdt.h>
#define WDT_TIMEOUT 1  //1 seconds WDT

//touchpad
touch_pad_t touchPin;
int threshold = 35;  //Threshold value for touchpads pins
long lastTouch;

#define FAIL_PIN 12

//wifi/bluetooth (just to switch them Off !)
#include <WiFi.h>
#include <bt.h>)

//Preferences
#include <Preferences.h>
Preferences preferences;

//#define DEBUG  //uncomment to get debug messages
//#define DEBUG_FRAME


void setup() {
  //stop Wifi and Bluetooth
  WiFi.mode(WIFI_OFF);
  btStop();

  Serial.begin(115200);
  delay(1000);
  Serial.println("program started");

  //Preferences
  preferences.begin("sbusBox", false);
  size_t size = preferences.getBytesLength("failsafe");  //check if failsafe is already saved
  if (size == sizeof(failsafe)) {
    preferences.getBytes("failsafe", failsafe, sizeof(failsafe));  //get values
  } else {
    preferences.putBytes("failsafe", failsafe, sizeof(failsafe));  // if not already done, save failsafe array
  }
  //preferences.clear();              // Remove all preferences under the opened namespace
  //preferences.remove("counter");   // remove the counter key only


  Serial.println("read preferences :");
  Serial.print("16 failsafe values :  ");
  for (int i = 0; i < 16; i++) {
    Serial.print("\t");
    Serial.print(failsafe[i]);
  }
  Serial.println("");



  //preferences.end();  // Close the Preferences


  //SBUS
  sbus_rx1.Begin();
  sbus_rx2.Begin();

  //servos PWM
  for (int i = 0; i < 16; i++) {
    servos[i].attach(pwmPin[i]);  // each servo is attached to a "Ledc channel"
  }

  //start watchdog (will be reseted into the loop)
  esp_task_wdt_init(WDT_TIMEOUT, true);  //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                //add current thread to WDT watch

  Serial.println("prog ready, watchdog set, servos started");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  esp_task_wdt_reset();  //reset the watchdog

  if (sbus_rx1.Read()) {  // if something on SBUS1
    data1 = sbus_rx1.data();
    lastSbus1 = millis();
#ifdef DEBUG_FRAME
    Serial.print("Sbus1 \t");
    for (int8_t i = 0; i < data1.NUM_CH; i++) {
      Serial.print(data1.ch[i]);
      Serial.print("\t");
    }
    Serial.print(data1.lost_frame);
    Serial.print("\t");
    Serial.println(data1.failsafe);
#endif
  }

  if (sbus_rx2.Read()) {  //if something on  SBUS2
    data2 = sbus_rx2.data();
    lastSbus2 = millis();
#ifdef DEBUG_FRAME
    Serial.print("Sbus2 \t");
    for (int8_t i = 0; i < data2.NUM_CH; i++) {
      Serial.print(data2.ch[i]);
      Serial.print("\t");
    }
    /* Display lost frames and failsafe data */
    Serial.print(data2.lost_frame);
    Serial.print("\t");
    Serial.println(data2.failsafe);
#endif
  }

  //map SBUS to servos PWM
  //if (!data1.lost_frame && !data1.failsafe) {  // if SBUS1 is "clean" use it !
  if ((millis()-lastSbus1) < 40) { // if SBUS1 is "clean" use it !
#ifdef DEBUG
    Serial.println("use Sbus\t 1");
#endif
    for (int i = 0; i < 16; i++) {
      int pulseWidth = map(data1.ch[i], 172, 1811, 1000, 2000);
      servos[i].write(pulseWidth);  //write directly pulsewidth to the library
      channels[i] = pulseWidth;
    }
    digitalWrite(LED_PIN, (millis() / 100) % 2 == 0 ? LOW : HIGH);  // RX1 OK : blink LED
  } else if ((millis()-lastSbus2) < 40) {                           // else if SBUS2 is clean use it                                //if (!data2.lost_frame && !data2.failsafe) {               
#ifdef DEBUG
    Serial.println("use Sbus\t 2");
#endif
    for (int i = 0; i < 16; i++) {
      int pulseWidth = map(data2.ch[i], 172, 1811, 1000, 2000);
      servos[i].write(pulseWidth);  //write directly pulsewidth to the library
      channels[i] = pulseWidth;
    }
digitalWrite(LED_PIN, (millis() / 500) % 2 == 0 ? LOW : HIGH);  
    //digitalWrite(LED_PIN, LOW);  // RX2 ok : LED on
  } else                         //apply failsafe
#ifdef DEBUG
    Serial.println("use failsafe \t 3");
#endif
  {
    for (int i = 0; i < 16; i++) {
      servos[i].write(failsafe[i]);
    }
    digitalWrite(LED_PIN, LOW);  // LED on in failsafe
  }

  //check if should save failsafe
  if ((millis() - lastTouch) > 1000) {  //avoid bouncing on touchpad
    if (ftouchRead(FAIL_PIN) < threshold) {
      Serial.println("touchpad - detected : save faisafe");
      lastTouch = millis();
      for (int i = 0; i < 16; i++) {
        failsafe[i] = channels[i];  //save current position into failsafe array
      }
      preferences.putBytes("failsafe", failsafe, sizeof(failsafe));  // if not already done, save failsafe array into preferences
    }
  }
}

int ftouchRead(int gpio)  // this will filter false readings of touchRead() function...
{
  int val = 0;
  int readVal;
  for (int i = 0; i < 10; i++) {  //perform 10 readings and keep the max
    readVal = touchRead(gpio);
    val = max(val, readVal);
  }
  return val;
}
