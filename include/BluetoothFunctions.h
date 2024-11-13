#ifndef BLUETOOTH_FUNCTIONS_H
#define BLUETOOTH_FUNCTIONS_H

#include <Arduino.h>  // Include questa libreria per usare String

void initializeM5Stack(); // Aggiungi questa linea
void initializeBluetoothConnection();
bool testELM327Connection();
String sendATCommand(const char* command, int delayTime);
String readATResponse();
void displayDebugMessage(const char* command, const String& response);
void disconnectBluetooth();

#endif
