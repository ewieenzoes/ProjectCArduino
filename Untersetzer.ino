/**
 * Smart Coaster - Der smarte Untersetzer
 * Master Thesis - HCI | Universität Siegen
 * Robert Fischbach & Enzo Frenker-Hackfort
 * 
 * Tested w/ Adafruit Feather Huzzah
 * 
 * Additional libs used: HX711_ADC by Olav Kallhovd
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>

#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClient.h>

ESP8266WiFiMulti WiFiMulti;

#include <HX711_ADC.h>

#if defined(ESP8266) || defined(ESP32) || defined(AVR)

#include <EEPROM.h>

#endif

//pins:
const int HX711_dout = 13; //mcu > HX711 dout pin
const int HX711_sck = 14; //mcu > HX711 sck pin

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
unsigned long t = 0;

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("untersetzer.digital", "Smart_Coaster_EFHRF");

  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  float calibrationValue; // calibration value (see example file "Calibration.ino")
  calibrationValue = 75.46; // uncomment this if you want to set the calibration value in the sketch
  #if defined(ESP8266) || defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
  #endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom

  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  } else {
    LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
    Serial.println("Startup is complete");
  }

}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 10000; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      int i = LoadCell.getData();
      Serial.print("Load_cell output val: ");
      Serial.println(i);
      if (i > 150) {
      
            // wait for WiFi connection
            if ((WiFiMulti.run() == WL_CONNECTED)) {
      
              WiFiClient client;
      
              HTTPClient http;
      
              Serial.print("[HTTP] begin...\n");
              if (http.begin(client, "http://www.untersetzer.digital/level/01/" + String(i))) { // HTTP
      
                Serial.print("[HTTP] GET...\n");
                // start connection and send HTTP header
                int httpCode = http.GET();
      
                // httpCode will be negative on error
                if (httpCode > 0) {
                  // HTTP header has been send and Server response header has been handled
                  Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      
                  // file found at server
                  if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                    String payload = http.getString();
                    Serial.println(payload);
                  }
                } else {
                  Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
                }
      
                http.end();
              } else {
                Serial.printf("[HTTP} Unable to connect\n");
              }
            }
      } 
      newDataReady = 0;
      t = millis();
    }
  }

  // receive command from serial terminal, send 't' to initiate tare operation:
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  // check if last tare operation is complete:
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }
}
