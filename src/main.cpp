/**
 * @file				main.cpp
 * @brief				Main
 * @copyright		Revised BSD License, see section @ref LICENSE.
 * @code
 * 
 * @endcode
 * @author			Ernesto Attanasio <ernattamaker@gmail.com>
 * @date				26 Nov 2024
 */
#define DEBUG

#include <M5Stack.h>
#include <BluetoothSerial.h>

BluetoothSerial ELM_PORT;

#define m5Name "M5Stack_OBD"

// Dichiarazione funzioni
void updateDisplay();
float getCoolantTemp();
float getRPM();
float getIntakeTemp();
float getOBDVoltage();
float getEngineLoad();
float getMAF();
void displayDebugMessage(const char* message, int x , int y, uint16_t textColour);
bool sendAndReadCommand(const char* cmd, String& response, int delayTime);
bool ELMinit();
bool BTconnect();
void dataRequestOBD();

uint8_t BLEAddress[6] = {0x00, 0x10, 0xCC, 0x4F, 0x36, 0x03};  // Indirizzo Bluetooth del modulo ELM327
float coolantTemp = 0.0;
float rpm = 0.0;
float intakeTemp = 0.0;
float obdVoltage = 0.0;
float engineLoad = 0.0;
float MAF = 0.0;
unsigned long lastVoltageQueryTime = 0;
const unsigned long voltageQueryInterval = 4000; // Intervallo di 4 secondi
float lastCoolantTemp = -999.0; // Valore iniziale fuori da ogni possibile range di temperatura
float lastIntakeTemp = -999.0; // Valore iniziale fuori da ogni possibile range di temperatura

unsigned long lastOBDQueryTime = 0;
unsigned long OBDQueryInterval = 800; // Intervallo di query OBD in millisecondi

void setup() {
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillScreen(BLACK);
  Serial.begin(115200);
  Serial.println("Setup in corso...");
  
  BTconnect();
  delay(2000);
  ELMinit();
  delay(1000);
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastOBDQueryTime >= OBDQueryInterval) {  // Verifica se è il momento di eseguire una nuova query OBD
    lastOBDQueryTime = currentMillis;
    dataRequestOBD();
    updateDisplay();
  }
}

bool sendAndReadCommand(const char* cmd, String& response, int delayTime) {
  response = "";
  unsigned long startTime = millis();
  ELM_PORT.print(cmd);
  ELM_PORT.print("\r\n");

  while (millis() - startTime < delayTime) {
    if (ELM_PORT.available()) {
      char c = ELM_PORT.read();
      response += c;
    }
    delay(50);
  }
  if (response.length() > 0) {
    M5.Lcd.setCursor(0, 60);
    #ifdef DEBUG
        displayDebugMessage(response.c_str(), 0 , 200, GREEN);
    #endif
  }

  if (response.length() == 0) {
    Serial.println("No RCV");
    return false;
  }

  if (response.indexOf("OK") >= 0 || response.length() > 0) {  // Aggiungi un controllo per la risposta OK se desiderato
    return true;
  } else {
    Serial.println("Err: " + response);
    return false;
  }
}

float getCoolantTemp() {
  static float lastValue = 0.0;
  String response;
  if (sendAndReadCommand("0105", response, 1000)) {
    #ifdef DEBUG
      Serial.println("CT 0105 RAW: " + response);
    #endif
    if (response.length() >= 6 && response.indexOf("4105") == 0) {
      byte tempByte = strtoul(response.substring(4, 6).c_str(), NULL, 16); // Converti il carattere esadecimale in byte
      float temp = tempByte - 40;
      #ifdef DEBUG
        Serial.print("CT 0105 convert: " + String(temp));
      #endif
      lastValue = temp;
      return temp;
    } else {
      #ifdef DEBUG
        Serial.print("CT 0105 IF indexOf 4105 && length >= 6: " + response);
      #endif
      //return 0.0; // Se non c'è risposta valida
    }
  } else {
    #ifdef DEBUG
      Serial.print("CT 0105 TimeOut response");
    #endif
    //return 0.0; // In caso di errore
  }
  return lastValue;
}

float getIntakeTemp() {
  static float lastValue = 0.0;
  String response;
  if (sendAndReadCommand("010F", response, 1000)) {
    #ifdef DEBUG
      Serial.println("INTAKE 010F RAW: " + response);
    #endif
    if (response.length() >= 6 && response.indexOf("410F") == 0) {
      byte tempByte = strtoul(response.substring(4, 6).c_str(), NULL, 16); // Converti il carattere esadecimale in byte
      float iTemp = tempByte - 40; 
      #ifdef DEBUG
        Serial.println("INTAKE 010F convert: " + String(iTemp)); // conversione per stampare il dato in DEBUG
      #endif
      lastValue = iTemp;
      return iTemp;
    } else {
      #ifdef DEBUG
        Serial.println("INTAKE 010F IF Length && indexOf fail: " + response);
      #endif
      //return 0.0; // Se non c'è risposta valida
    }
  } else {
    #ifdef DEBUG
      Serial.print("INTAKE 010F TimeOut response");
    #endif
    //return 0.0; // In caso di errore
  }
 return lastValue;
}

float getRPM() {
  static int lastValue = 0;
  String response;
  if (sendAndReadCommand("010C", response, 1000)) {
    #ifdef DEBUG
      Serial.println("RPM 010C RAW: " + response);
    #endif    
    String printRPM = "";
    if (response.length() >= 8 && response.indexOf("410C") >= 0) {
      byte highByte = strtoul(response.substring(4, 6).c_str(), NULL, 16); // Indici corretti per il primo byte
      byte lowByte = strtoul(response.substring(6, 8).c_str(), NULL, 16);  // Indici corretti per il secondo byte
      int rpm = (highByte * 256 + lowByte) / 4;
      
      #ifdef DEBUG
        printRPM = String(rpm); // Conversione per stampare il dato in DEBUG
        Serial.println("RPM 010C response convert:" + printRPM);
      #endif
      lastValue = rpm;
      return rpm;
    } else {
      #ifdef DEBUG
        Serial.println("RPM 010C IF Length && indexOf fail: " + response);
      #endif
      //return 0.0; // Se non c'è risposta valida
    }
  } else {
    #ifdef DEBUG
      Serial.print("RPM 010C TimeOut response");
    #endif
    //return 0.0; // In caso di errore
  }
 return lastValue;
}

float getEngineLoad() {
  static float lastValue = 0.0;
  String response;
  if (sendAndReadCommand("0104", response, 1000)) {
    #ifdef DEBUG
      Serial.println("EL 0104 RAW: " + response);
    #endif
    if (response.length() >= 8 && response.indexOf("4104") >= 0) {
      byte highByte = strtoul(response.substring(4, 6).c_str(), NULL, 16);
      float load = (highByte * 100.0) / 255.0;
      #ifdef DEBUG
        String eLoad = String(load); // conversione per stampare il dato in DEBUG
        Serial.println("EL 0104 convert: " + eLoad);
      #endif
      lastValue = load;
      return load;
    } else {
      #ifdef DEBUG
        Serial.println("EL 0104 IF Length && indexOf fail: " + response);
      #endif
      //return 0.0; // Se non c'è risposta valida
    }
  } else {
    #ifdef DEBUG
      Serial.print("EL TimeOut response");
    #endif
    //return 0.0; // In caso di errore
  }
  return lastValue;
}

float getOBDVoltage() {
  static int lastValue = 0;
  String response;
  if (sendAndReadCommand("ATRV", response, 1000)) {
    response.trim();      // Rimuove eventuali spazi bianchi all'inizio e alla fine della stringa
    int indexV = response.indexOf('V');
    if (indexV >= 0) {
      String voltageStr = response.substring(0, indexV);
      float voltage = voltageStr.toFloat();
        #ifdef DEBUG
          displayDebugMessage(response.c_str(), 0, 200, PINK);
        #endif
        lastValue = voltage;
      return voltage;
    } else {
        #ifdef DEBUG
          displayDebugMessage("Err Volt", 0 , 200, PINK);
        #endif
      //return 0.0; // In caso di risposta non valida
    }
  } else {
      #ifdef DEBUG
        Serial.print("ATRV TimeOut response");
      #endif
   // return 0.0; // In caso di errore
  }
 return lastValue;
}

float getMAF() {
  static float lastValidMAF = 0.0; // Variabile per conservare l'ultimo valore valido
  String response;

  if (sendAndReadCommand("0110", response, 2000)) {
    #ifdef DEBUG
      Serial.println("MAF 0110 RAW: " + response);
    #endif

    if (response.length() >= 6 && response.indexOf("4110") == 0) {
      int A = strtoul(response.substring(4, 6).c_str(), NULL, 16); // Converti il carattere esadecimale in byte
      int B = strtoul(response.substring(6, 8).c_str(), NULL, 16); // Converti il carattere esadecimale in byte
      float maf = ((A * 256) + B) / 100.0; // Calcolo del MAF in g/s

      #ifdef DEBUG
        Serial.print("MAF 0110 convert: " + String(maf));
      #endif

      lastValidMAF = maf; // Aggiorna l'ultimo valore valido
      return maf;
    } else {
      #ifdef DEBUG
        Serial.print("MAF 0110 IF indexOf 4110 && length >= 6: " + response);
      #endif
    }
  } else {
    #ifdef DEBUG
      Serial.print("MAF 0110 TimeOut response");
    #endif
  }

  return lastValidMAF; // Restituisci l'ultimo valore valido in caso di errore o risposta non valida
}


void updateDisplay() {
  M5.Lcd.fillRect(0, 0, 320, 200, BLACK);  // Pulisce la parte inferiore del display

  if (coolantTemp < 50) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(LIGHTGREY);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  } else if (coolantTemp >= 50 && coolantTemp <= 81) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(ORANGE);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  } else if (coolantTemp >= 81 && coolantTemp <= 97) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  } else if (coolantTemp >= 98) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  }

  if (obdVoltage < 12) {
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.printf("OBD Voltage: %.2f V\n", obdVoltage);
  } else if (obdVoltage > 12 && obdVoltage <= 14) {
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.printf("OBD Voltage: %.2f V\n", obdVoltage);
  } else if (obdVoltage >= 14) {
    M5.Lcd.setTextColor(ORANGE);
    M5.Lcd.printf("OBD Voltage: %.2f V\n", obdVoltage);
  }

  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(0, 40);
  M5.Lcd.printf("RPM: %.0f\n", rpm);
  M5.Lcd.setCursor(0, 60);
  M5.Lcd.printf("Intake Temp: %.1f C\n", intakeTemp);
  M5.Lcd.setCursor(0, 80);
  M5.Lcd.printf("Engine Load: %.1f%%\n", engineLoad);
  M5.Lcd.setCursor(0, 100);
  M5.Lcd.printf("MAF: %.1f%%\n", MAF);
}


void displayDebugMessage(const char* message, int x , int y, uint16_t textcolour)   {
  M5.Lcd.setTextColor(textcolour);
  M5.Lcd.fillRect(x, y, 320, 40, BLACK);  // Pulisce la parte bassa del display
  M5.Lcd.setCursor(x, y);
  M5.Lcd.print(message);
  Serial.println(message);  // Mostra anche nel monitor seriale
}

bool ELMinit() {
  String response;

  #ifdef DEBUG
    displayDebugMessage("ELM init...", 0 ,200, WHITE);
    Serial.println("ELM init...");
  #endif

  if (!sendAndReadCommand("ATZ", response, 1500)) {
    #ifdef DEBUG
      displayDebugMessage("Err ATZ", 0 , 20, WHITE);
    #endif
    return false;
  }
  #ifdef DEBUG
    displayDebugMessage(response.c_str(), 0 , 20, WHITE);
  #endif

  if (!sendAndReadCommand("ATE0", response, 1500)) {
    #ifdef DEBUG
      displayDebugMessage("Err ATE0", 0 , 40, WHITE);
    #endif
    return false;
  }
  #ifdef DEBUG
    displayDebugMessage(response.c_str(), 0 , 40, WHITE);
  #endif

  if (!sendAndReadCommand("ATL0", response, 1500)) {
    #ifdef DEBUG
      displayDebugMessage("Err ATL0", 0 , 60, WHITE);
    #endif
    return false;
  }
  #ifdef DEBUG
    displayDebugMessage(response.c_str(), 0 , 60, WHITE);
  #endif

  if (!sendAndReadCommand("ATS0", response, 1500)) {
    #ifdef DEBUG
      displayDebugMessage("Err ATS0", 0 , 80, WHITE);
    #endif
    return false;
  }
  #ifdef DEBUG
    displayDebugMessage(response.c_str(), 0 , 80, WHITE);
  #endif

  if (!sendAndReadCommand("ATST0A", response, 1500)) {
    #ifdef DEBUG
      displayDebugMessage("Err ATST0A", 0 , 100, WHITE);
    #endif
    return false;
  }
  #ifdef DEBUG
    displayDebugMessage(response.c_str(), 0 , 100, WHITE);
  #endif

  if (!sendAndReadCommand("ATSP0", response, 15000)) {  // Imposta protocollo automatico SP 0 e gestire la risposta "SEARCHING..."
    #ifdef DEBUG
      displayDebugMessage("Err ATSP0", 0 , 120, WHITE);
    #endif
    return false;
  }
  #ifdef DEBUG
    displayDebugMessage(response.c_str(), 0 , 120, WHITE);
  #endif

  return true;
}

bool BTconnect(){
  ELM_PORT.begin(m5Name, true);  // Avvia la connessione Bluetooth
  #ifdef DEBUG
    displayDebugMessage("Connessione BT...", 0 , 200, WHITE);
  #endif
  int retries = 0;  // Tentativo connessione bluetooth ELM327 (5 try)
  bool connected = false;
  while (retries < 5 && !connected) {
    connected = ELM_PORT.connect(BLEAddress);
    if (!connected) {
      #ifdef DEBUG
        displayDebugMessage("BT Conn FAIL", 0 , 200, WHITE);
      #endif
      retries++;
      delay(500);
    }
  }

  if (!connected) {
    #ifdef DEBUG
      displayDebugMessage("ELM BT NOT FOUND", 0 , 200, WHITE);
      Serial.println("ELM BT not found");
    #endif
    return false;  // Loop infinito se non riesce a connettersi
  }
  else {
    #ifdef DEBUG
      displayDebugMessage("Connessione BT OK!", 0 , 200, WHITE);
      Serial.println("BT Conn OK!");
    #endif
    return true;
  }
}

void dataRequestOBD() {
  unsigned long currentMillis = millis();
  float newCoolantTemp = getCoolantTemp();
  if (abs(newCoolantTemp - lastCoolantTemp) >= 0.5) {
    coolantTemp = newCoolantTemp;
    lastCoolantTemp = newCoolantTemp;
  }

  rpm = getRPM();

  float newIntakeTemp = getIntakeTemp();
  if (abs(newIntakeTemp - lastIntakeTemp) >= 0.8) {
    intakeTemp = newIntakeTemp;
    lastIntakeTemp = newIntakeTemp;
  }

  if (currentMillis - lastVoltageQueryTime >= voltageQueryInterval) {
    obdVoltage = getOBDVoltage();
    lastVoltageQueryTime = currentMillis;
  }
  MAF = getMAF();
  engineLoad = getEngineLoad();
}