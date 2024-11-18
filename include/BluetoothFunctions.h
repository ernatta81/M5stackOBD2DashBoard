#ifndef BLUETOOTH_FUNCTIONS_H
#define BLUETOOTH_FUNCTIONS_H

#include <Arduino.h>  // Questa libreria è per usare String

void initializeM5Stack(); 
void initializeBluetoothConnection();
bool testELM327Connection();
String sendATCommand(const char* command, int delayTime);
String readATResponse();
void displayDebugMessage(const char* command, const String& response);
void disconnectBluetooth();

#endif
