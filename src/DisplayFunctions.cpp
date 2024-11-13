#include "DisplayFunctions.h"
#include <M5Stack.h>
#include <M5GFX.h>
#include <Globals.h>

extern M5GFX display;

void initializeDisplay() {
  display.begin();
  display.setEpdMode(epd_mode_t::epd_fastest);

  if (display.width() > display.height()) {
    display.setRotation(display.getRotation() ^ 1);
  }

  display.startWrite();
  display.fillScreen(TFT_BLACK);

  int w = display.width() / 4;
  int h = display.height() * 3 / 5;
  int y = display.height() * 2 / 5;

  // Disegna le 4 barre grafiche con le etichette PID
  for (int i = 0; i < 4; i++) {
    plotLinear(PID[i], values[i], display.width() * i / 4, y, w, h);
  }

  display.endWrite();
}

void updateDisplayValues() {
  values[0] = (int)coolantTemp;
  values[1] = 100; // Aggiorna con il valore MAF
  values[2] = obdVoltage; //
  values[3] = (int)rpm;

  // Ridisegna le barre con i nuovi valori
  display.startWrite();
  int w = display.width() / 4;
  int h = display.height() * 3 / 5;
  int y = display.height() * 2 / 5;
  for (int i = 0; i < 4; i++) {
    plotLinear(PID[i], values[i], display.width() * i / 4, y, w, h);
  }
  display.endWrite();
}

void displayMessage(const char* message) {
  DEBUG_PORT.println(message);
  M5.Lcd.fillRect(0, 0, 320, 20, BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print(message);
}

void displayError(const char* errorMsg) {
  DEBUG_PORT.println(errorMsg);
  M5.Lcd.fillRect(0, 200, 320, 20, BLACK);
  M5.Lcd.setCursor(0, 200);
  M5.Lcd.print(errorMsg);
}
void plotLinear(const String &label, int value, int x, int y, int w, int h) {
  display.drawRect(x, y, w, h, TFT_GREY);
  display.fillRect(x + 2, y + 18, w - 3, h - 36, TFT_WHITE);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setTextDatum(textdatum_t::middle_center);
  display.setFont(&fonts::Font2);
  display.setTextPadding(display.textWidth("100"));
  display.drawString(label, x + w / 2, y + 10);

  int plot_height = h - (19 * 2);

  // Disegna l'indicatore in base al valore
  int indicator_y = y + 19 + (value * plot_height / 100); // Supponendo che il valore sia tra 0 e 100
  display.fillRect(x + 2, indicator_y, w - 3, 10, TFT_RED); // Indicatore rosso

  // Aggiungi il valore al centro della parte bianca con un font in grassetto
  display.setTextColor(TFT_BLACK, TFT_WHITE);
  display.setFont(&fonts::Font4); 
  display.setTextDatum(textdatum_t::middle_center);
  display.drawString(String(value), x + w / 2, y + h / 2); // Posiziona il valore al centro della parte bianca
}