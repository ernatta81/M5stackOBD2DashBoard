/**
 * @file				BD2.ino
 * @brief				Main thread
 * @copyright		Revised BSD License, see section @ref LICENSE.
 * @code
 * 
 * @endcode
 * @author			Ernesto Attanasio <ernattamaker@gmail.com> ( KiWi )
 * @date				13 Nov 2024

 * @{
 */

#include "BluetoothFunctions.h"
#include "DisplayFunctions.h"
#include "ELM327Functions.h"
#include "Globals.h"
#include <BluetoothSerial.h>

void setup() {
  initializeM5Stack();
  initializeBluetoothConnection();
  autoConfigureELM327();
  initializeDisplay();
}

void loop() {
  requestCoolantTemperature();
  requestOBDVoltage();
  requestRPM();
  updateDisplayValues();
  delay(100);
}





























