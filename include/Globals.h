#ifndef GLOBALS_H
#define GLOBALS_H

#include <BluetoothSerial.h>
#include <M5GFX.h>

extern BluetoothSerial SerialBT;
extern M5GFX display;

#define ELM_PORT SerialBT
#define DEBUG_PORT Serial

extern uint8_t BLEAddress[6];
extern float coolantTemp;
extern float obdVoltage;
extern float rpm;
extern const char* protocols[];
extern const int numProtocols;
static constexpr int TFT_GREY = 0x5AEB;

extern String PID[];
extern int values[4];

#endif
