#include "BluetoothSerial.h"
#include <M5Stack.h>
#include <M5GFX.h>

BluetoothSerial SerialBT;
M5GFX display;

#define ELM_PORT SerialBT
#define DEBUG_PORT Serial

uint8_t BLEAddress[6] = {0x00, 0x10, 0xCC, 0x4F, 0x36, 0x03};
float coolantTemp = 0.0;
float obdVoltage = 0.0;
float rpm = 0.0; // Aggiunto il valore RPM
const char* protocols[] = {"ATSP0", "ATSP1", "ATSP2", "ATSP3", "ATSP4", "ATSP5", "ATSP6", "ATSP7", "ATSP8", "ATSP9"};
const int numProtocols = sizeof(protocols) / sizeof(protocols[0]);
static constexpr int TFT_GREY = 0x5AEB;

String PID[] = {"Coolant", "MAF", "Load", "RPM"};
int values[4] = {0, 0, 0, 0};

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
  delay(1000); // Attende 1 secondo prima di aggiornare nuovamente
}

void initializeM5Stack() {
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillScreen(BLACK);
  DEBUG_PORT.begin(115200);
}

void initializeBluetoothConnection() {
  ELM_PORT.begin("M5Stack_OBD", true);
  displayMessage("Connessione...");
  if (!ELM_PORT.connect(BLEAddress)) {
    displayError("BLE KO");
    while (1);
  }
  displayMessage("OBD BLE OK!");
  delay(2000);
}

void autoConfigureELM327() {
  sendATCommand("ATZ", 1000);  // Reset
  sendATCommand("ATE0", 1000);  // Disabilita eco
  sendATCommand("ATL0", 1000);  // Disabilita line feed
  sendATCommand("ATH1", 1000);  // Abilita header

  bool connected = false;
  for (int i = 0; i < numProtocols; i++) {
    sendATCommand(protocols[i], 3000); // Seleziona protocollo e attendi 100 ms
    if (testELM327Connection()) {
      displayMessage(("Connesso a ELM327 con protocollo: " + String(protocols[i])).c_str());
      connected = true;
      break;
    }
  }
  if (!connected) {
    displayError("Errore protocollo ELM327");
    while (1);
  }

  unsigned long waitTime = 2000;
  unsigned long startTime = millis();
  while (millis() - startTime < waitTime) {
    // Funzione di attesa non bloccante
  }
}

bool testELM327Connection() {
  String response = "";
  unsigned long startTime = millis();
  unsigned long timeout = 5000;  // Timeout di 5 secondi

  ELM_PORT.println("0100");  // PID request

  while (millis() - startTime < timeout) {
    if (ELM_PORT.available()) {
      char c = ELM_PORT.read();
      response += c;
    }
    if (response.indexOf("41") != -1) { // Verifica se la risposta contiene "41"
      break;
    }
  }
  
  return response.length() > 0 && response.indexOf("41") != -1;
}

String sendATCommand(const char* command, int delayTime) {
  ELM_PORT.println(command);
  unsigned long startTime = millis();
  while (millis() - startTime < delayTime) {
    // Funzione di attesa non bloccante
    if (ELM_PORT.available()) {
      break;
    }
  }
  String response = readATResponse();

  displayDebugMessage(command, response);
  return response;
}

String readATResponse() {
  String response = "";
  while (ELM_PORT.available()) {
    char c = ELM_PORT.read();
    response += c;
  }
  return response;
}

void displayDebugMessage(const char* command, const String& response) {
  DEBUG_PORT.print("SND>>> ");
  DEBUG_PORT.println(command);
  DEBUG_PORT.print("RCV<<< ");
  DEBUG_PORT.println(response);
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
    delay(100); // Aggiungi delay
  }
  
  displayDebugMessage("0105", response);
  
  int value = strtol(response.substring(4, 6).c_str(), NULL, 16);
  coolantTemp = value - 40;

  if (coolantTemp > 0) {
    // Display della temperatura su seriale
    DEBUG_PORT.print("Temperatura (°): ");
    DEBUG_PORT.println(coolantTemp);
  } else {
    displayError("Errore Temperatura");
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
    delay(100); // Aggiungi delay
  }

  displayDebugMessage("ATRV", response);

  int startIdx = response.indexOf("V");
  if (startIdx > 0) {
    obdVoltage = response.substring(0, startIdx).toFloat(); // Converti la risposta in un numero float
    if (obdVoltage > 0) {
      // Display della tensione su seriale
      DEBUG_PORT.print("Tensione (V): ");
      DEBUG_PORT.println(obdVoltage);
    }
  } else {
    displayError("Errore V");
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
    delay(100); // Aggiungi delay
  }
  
  displayDebugMessage("010C", response);
  
  int value = strtol(response.substring(4, 8).c_str(), NULL, 16);
  rpm = value / 4.0;

  if (rpm > 0) {
    // Display degli RPM su seriale
    DEBUG_PORT.print("RPM: ");
    DEBUG_PORT.println(rpm);
  } else {
    displayError("Errore RPM");
  }
}

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
  // Aggiorna i valori delle variabili
  values[0] = (int)coolantTemp;
  values[1] = 100; // Aggiorna con il valore MAF
  values[2] = 10; // Aggiorna con il valore di carico motore
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
  display.setFont(&fonts::Font4); // Imposta un font più grande e in grassetto
  display.setTextDatum(textdatum_t::middle_center);
  display.drawString(String(value), x + w / 2, y + h / 2); // Posiziona il valore al centro della parte bianca
}

void displayMessage(const char* message) {
  DEBUG_PORT.println(message);
}

void displayError(const char* errorMsg) {
  DEBUG_PORT.println(errorMsg);
}

void displayRPM() {
  DEBUG_PORT.print("RPM: ");
  DEBUG_PORT.println(rpm);
}
