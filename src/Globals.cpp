#include "Globals.h"

BluetoothSerial SerialBT;
M5GFX display;

uint8_t BLEAddress[6] = {0x00, 0x10, 0xCC, 0x4F, 0x36, 0x03}; // Address of Bluetooth adapter ELM327 OBDII
float coolantTemp = 0.0;
float obdVoltage = 0.0;
float rpm = 0.0;
String response;
//const char* protocols[] = {"ATSP0", "ATSP1", "ATSP2", "ATSP3", "ATSP4", "ATSP5", "ATSP6", "ATSP7", "ATSP8", "ATSP9"};
const char* protocols[] = {"ATSP0", "ATSP6", "ATSP7", "ATSP8", "ATSP9", "ATSP5" "ATSP4", "ATSP3", "ATSP2", "ATSP1"};
const int numProtocols = sizeof(protocols) / sizeof(protocols[0]);

String PID[] = {"COOLANT", "MAF", "VOLT", "RPM"};
int values[4] = {0, 0, 0, 0};
