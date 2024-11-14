#include <Arduino.h>
#include <M5Stack.h>
#include <BluetoothSerial.h>
#include "Free_Fonts.h"

BluetoothSerial SerialBT;

#define ELM_PORT   SerialBT
#define m5Name "M5Reader"
#define PWRDelay 65
#define MSGDelay 2000

uint8_t LCD_BRIGHTNESS = 50;
uint8_t BLEAddress[6] = {0x00, 0x10, 0xCC, 0x4F, 0x36, 0x03};
const int numProtocols = 10;
const char* protocols[numProtocols] = {
  "ATSP0",  // Auto protocol (viene scelto automaticamente dal veicolo)
  "ATSP3",  // ISO 15765-4 (CAN 11bit/500Kbps)
  "ATSP6",  // SAE J1850 PWM
  "ATSP1",  // ISO 9141-2 (europei veicoli vecchi)
  "ATSP2",  // ISO 14230-4 KWP (Keyword Protocol)
  "ATSP4",  // ISO 14230-4 KWP Fast Init
  "ATSP5",  // ISO 14230-4 KWP Slow Init
  "ATSP7",  // SAE J1850 VPW
  "ATSP8",  // CAN (11 bit ID)
  "ATSP9"   // CAN (29 bit ID)
};

void sendCommand(const char* cmd) {
  Serial.println("Sending Command: " + String(cmd)); // Aggiunto per debug
  ELM_PORT.print(cmd);
  ELM_PORT.print("\r");
}

String readResponse(long timeout = 2000) {
  String response = "";
  long endTime = millis() + timeout;
  while (millis() < endTime) {
    if (ELM_PORT.available()) {
      char c = ELM_PORT.read();
      response += c;
      if (c == '>') break; // Fine della risposta
    }
  }

  if (response.isEmpty()) {
    Serial.println("No response from OBD");
    return "";
  } else {
    Serial.println("Response: " + response);
  }
  return response;
}

bool checkResponse(const String& response, const String& pattern) {
  return response.indexOf(pattern) > -1;
}

void sendCommandAndWait(const char* cmd, const String& expectedResponse = "") {
  sendCommand(cmd);
  String response = readResponse();

  if (expectedResponse.isEmpty() || checkResponse(response, expectedResponse)) {
    M5.Lcd.println("Command successful: " + String(cmd));
  } else {
    M5.Lcd.println("Command failed: " + String(cmd));
    M5.Lcd.println("Response: " + response);
  }
}

void displayProgress(int attempt, const String& protocol) {
  M5.Lcd.fillRect(0, 235, attempt * 10, 5, ORANGE);
  M5.Lcd.setCursor(50, 150);
  M5.Lcd.printf("Attempt %d with %s: In progress", attempt + 1, protocol);
}

bool detectProtocol() {
  bool protocolDetected = false;

  // Prova i protocolli, prima quelli più comuni
  for (int i = 0; i < numProtocols; i++) {
    sendCommand(protocols[i]);
    M5.Lcd.setCursor(50, 150);
    M5.Lcd.print("Trying protocol: ");
    M5.Lcd.println(protocols[i]);
    delay(500);  // Piccolo delay tra i tentativi

    for (int j = 0; j < 5; j++) {  // Prova fino a 5 volte per ogni protocollo
      sendCommandAndWait("010C", "41 0C");  // Comando RPM, per testare la connessione
      String response = readResponse();
      
      if (checkResponse(response, "41 0C")) {
        M5.Lcd.setCursor(50, 200);
        M5.Lcd.println("Protocol detected: " + String(protocols[i]));
        protocolDetected = true;
        break;  // Esci dal ciclo se il protocollo è stato trovato
      } else {
        M5.Lcd.setCursor(50, 180);
        M5.Lcd.printf("Attempt %d with %s: Failed\n", j + 1, protocols[i]);
        delay(1000);  // Un breve delay tra i tentativi
      }
    }

    if (protocolDetected) {
      break;  // Esci dal ciclo principale se il protocollo è stato trovato
    }
  }

  return protocolDetected;
}

bool startUp() {
  M5.Lcd.setBrightness(LCD_BRIGHTNESS);
  M5.Lcd.setTextColor(0xE73C, TFT_BLACK);
  M5.Lcd.setFreeFont(FF1);

  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 20);

  if (!SD.begin()) {
    M5.Lcd.println("SD card not found");  // Errore SD
    return false;
  }

  M5.Lcd.drawJpgFile(SD, "/p2.jpg"); // Logo su SD

  for (int i = 0; i < PWRDelay; i++) {
    M5.Lcd.fillRect(0, 235, i * 5, 5, BLUE);
    delay(50);
  }

  M5.Lcd.setCursor(50, 150);
  M5.Lcd.println(" Waiting OBD device");

  ELM_PORT.begin(m5Name, true);
  ELM_PORT.setPin("1234");

  M5.Lcd.setCursor(80, 190);
  M5.Lcd.println(" - Searching - ");
  int attemptCount = 0;
  do {
    Serial.println(attemptCount);
    M5.Lcd.fillRect(0, 235, attemptCount * 10, 10, ORANGE);
    attemptCount++;
  } while (!ELM_PORT.connect(BLEAddress) && attemptCount < 10);

  if (attemptCount == 10) {
    M5.Lcd.setCursor(50, 230);
    M5.Lcd.println("Failed to connect to OBD device.");
    return false;
  }

  M5.Lcd.setCursor(100, 210);
  M5.Lcd.println("OBD BLE OK!");
  M5.Lcd.setCursor(65, 230);
  M5.Lcd.println("Waiting ECU response");

  delay(MSGDelay);
  M5.Lcd.clearDisplay();
  M5.Lcd.setCursor(50, 130);

  return true;
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  if (!startUp()) {
    return;
  }

  // Inizializzazione ELM327
  sendCommandAndWait("ATZ"); // Reset
  sendCommandAndWait("ATE0"); // Echo off
  sendCommandAndWait("ATL0"); // Linefeed off
  sendCommandAndWait("ATS0"); // Spaces off
  sendCommandAndWait("ATH0"); // Headers off

  // Tentativo di rilevamento del protocollo OBD
  bool protocolDetected = detectProtocol();

  if (!protocolDetected) {
    M5.Lcd.setCursor(50, 230);
    M5.Lcd.println("Failed to detect OBD protocol after multiple attempts");
    delay(2000);  // Un po' di tempo per leggere il messaggio
    return;
  }

  M5.Lcd.setCursor(50, 230);
  M5.Lcd.println("Protocol detected and connected!");
  delay(2000);  // Mostra il messaggio di successo prima di continuare
}

void loop() {
  // Lettura della temperatura del liquido di raffreddamento
  sendCommandAndWait("0105");
  String tempResponse = readResponse();
  int coolantTemp = -40;
  if (tempResponse.indexOf("41 05") > -1) {
    coolantTemp = strtol(tempResponse.substring(6, 8).c_str(), NULL, 16) - 40;
  } else {
    M5.Lcd.println("Error reading coolant temp");
  }

  // Lettura del flusso d'aria di massa
  sendCommandAndWait("0110");
  String mafResponse = readResponse();
  float maf = 0.0;
  if (mafResponse.indexOf("41 10") > -1) {
    maf = (strtol(mafResponse.substring(6, 8).c_str(), NULL, 16) * 256 + strtol(mafResponse.substring(9, 11).c_str(), NULL, 16)) / 100.0;
  } else {
    M5.Lcd.println("Error reading MAF");
  }

  // Lettura del carico del motore
  sendCommandAndWait("0104");
  String loadResponse = readResponse();
  int engineLoad = 0;
  if (loadResponse.indexOf("41 04") > -1) {
    engineLoad = strtol(loadResponse.substring(6, 8).c_str(), NULL, 16) * 100 / 255;
  } else {
    M5.Lcd.println("Error reading engine load");
  }

  // Visualizza i dati sullo schermo LCD
  M5.Lcd.clearDisplay();
  M5.Lcd.setCursor(50, 30);
  M5.Lcd.printf("Coolant Temp: %d C", coolantTemp);
  M5.Lcd.setCursor(50, 50);
  M5.Lcd.printf("MAF Flow: %.2f g/s", maf);
  M5.Lcd.setCursor(50, 70);
  M5.Lcd.printf("Engine Load: %d %%", engineLoad);

  delay(2000); // Un po' di ritardo tra le letture
}
