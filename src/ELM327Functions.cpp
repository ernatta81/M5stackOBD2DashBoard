#include "ELM327Functions.h"
#include <BluetoothFunctions.h>
#include <DisplayFunctions.h>
#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <Globals.h>

extern BluetoothSerial SerialBT;

void initializeM5Stack() {
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillScreen(BLACK);
  DEBUG_PORT.begin(115200);
}


void autoConfigureELM327() {
  DEBUG_PORT.println("Autoconfigure ELM");
  sendATCommand("ATZ", 1000);   // Reset
  sendATCommand("AT@", 1000);
  String msg = "Ver: " + response;
  displayMessage(msg.c_str());
  sendATCommand("ATE0", 1000);  // Disabilita eco
  sendATCommand("ATL0", 1000);  // Disabilita line feed
  sendATCommand("ATH1", 1000);  // Abilita header
  
  bool connected = false;
  for (int i = 0; i < numProtocols; i++) {
    sendATCommand(protocols[i], 1500); // Seleziona protocollo
    if (testELM327Connection()) {
      displayMessage(("ELM327Protocol: " + String(protocols[i])).c_str());
      connected = true;
      delay(2000);
      break;
    }
  }
  if (!connected) {
    displayError("ELM327ProtERR");
    delay(1000);
    while (1);
  }
  // Attendi
  unsigned long waitTime = 2000;
  unsigned long startTime = millis();
  while (millis() - startTime < waitTime) {
    // Funzione di attesa non bloccante
  }
}

void requestCoolantTemperature() {
  String response = "";
  while (response.length() == 0 || response == "0105") {
    ELM_PORT.println("0105"); // PID for engine coolant temperature
    unsigned long startTime = millis();
    while (millis() - startTime < 100) {
      if (ELM_PORT.available()) {
        break;
      }
    }
    
    response = readATResponse();
    delay(100);
  }
  
  int value = strtol(response.substring(4, 6).c_str(), NULL, 16);
  coolantTemp = value - 40;

  if (coolantTemp > 0) {

 //   displayCoolantTemperature();
  } else {
 //   displayError("Errore Temperatura");
  }
}

void requestOBDVoltage() {
  String response = "";
  while (response.length() == 0 || response == "ATRV") {
    ELM_PORT.println("ATRV"); // Comando per leggere la tensione OBD
    unsigned long startTime = millis();
    while (millis() - startTime < 100) {
      if (ELM_PORT.available()) {
        break;
      }
    }
    
    response = readATResponse();
    delay(100);
  }

  float startIdx = response.indexOf("V");
  if (startIdx > 0) {
    obdVoltage = response.substring(0, startIdx).toFloat(); // Converti la risposta in un numero float
    if (obdVoltage > 0) {

//      displayOBDVoltage();
    }
  } else {

//    displayError("Errore V");
  }
}

void requestRPM() {
  String response = "";
  while (response.length() == 0 || response == "010C") {
    ELM_PORT.println("010C"); // PID for engine RPM
    unsigned long startTime = millis();
    while (millis() - startTime < 100) {
      if (ELM_PORT.available()) {
        break;
      }
    }
    
    response = readATResponse();
    delay(100);
  }
  
//  displayDebugMessage("010C", response);
  
  int value = strtol(response.substring(4, 8).c_str(), NULL, 16);
  float rpm = value / 4.0;

  if (rpm > 0) {
//    displayRPM(rpm);
  } else {
//    displayError("Errore RPM");
  }
}