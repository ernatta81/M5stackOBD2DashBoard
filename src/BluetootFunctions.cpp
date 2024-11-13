#include "BluetoothFunctions.h"
#include <DisplayFunctions.h>
#include <BluetoothSerial.h>
//#include <M5Stack.h>
#include <Globals.h>

extern BluetoothSerial SerialBT;

void initializeBluetoothConnection() {
  ELM_PORT.begin("M5Stack_OBD", true);
  displayMessage("Connessione...");
  if (!ELM_PORT.connect(BLEAddress)) {
    displayError(" - - BLE NOT FOUND - - ");
    while (1);
  }
  displayMessage(" - - OBD BLE OK! - - ");
  delay(2000);
}

bool testELM327Connection() {
  String response = "";
  unsigned long startTime = millis();
  unsigned long timeout = 5000;

  ELM_PORT.println("0100");  // PID request

  while (millis() - startTime < timeout) {
    if (ELM_PORT.available()) {
      char c = ELM_PORT.read();
      response += c;
    }
    if (response.indexOf("41") != -1) { 
      break;
    }
  }
  
  return response.length() > 0 && response.indexOf("41") != -1;
}

String sendATCommand(const char* command, int delayTime) {
  ELM_PORT.println(command);
  unsigned long startTime = millis();
  while (millis() - startTime < delayTime) {
     if (ELM_PORT.available()) {
      break;
    }
  }
   response = readATResponse();
  return response;
}

String readATResponse() {
   response = "";
  while (ELM_PORT.available()) {
    char c = ELM_PORT.read();
    response += c;
  }
  return response;
}

void displayDebugMessage(const char* command, const String& response) {
  DEBUG_PORT.print("SND>>> ");
  DEBUG_PORT.println(command);
  DEBUG_PORT.print("RCV<<<: ");
  DEBUG_PORT.println(response);

}


void disconnectBluetooth() {
  ELM_PORT.disconnect();
//  displayMessage("Bluetooth disconnesso");
}
