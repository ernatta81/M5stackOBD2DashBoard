#ifndef DISPLAY_FUNCTIONS_H
#define DISPLAY_FUNCTIONS_H

#include <Arduino.h>

void initializeDisplay();
void updateDisplayValues();
void displayMessage(const char* message);
void displayError(const char* errorMsg);
void plotLinear(const String &label, int value, int x, int y, int w, int h);

#endif
