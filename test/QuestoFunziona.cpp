#define DEBUG

#include <M5Stack.h>
#include <BluetoothSerial.h>
#define m5Name "M5Stack_OBD"

BluetoothSerial ELM_PORT;

// Dichiarazione funzioni
bool ELMinit();
bool BTconnect();
bool sendAndReadCommand(const char* cmd, String& response, int delayTime);
void updateDisplay();
void dataRequestOBD();
void displayDebugMessage(const char* message, int x , int y, uint16_t textColour);
void sendOBDCommand(const char* cmd);
void writeToCircularBuffer(char c);
void handleOBDResponse();
String readFromCircularBuffer(int numChars);
String bufferSerialData(int timeout, int numChars);
void parseOBDData(const String& response);
float parseCoolantTemp(const String& response);
float parseRPM(const String& response);
float parseIntakeTemp(const String& response);
float parseOBDVoltage(const String& response);
float parseEngineLoad(const String& response);
float parseMAF(const String& response);

uint8_t BLEAddress[6] = {0x00, 0x10, 0xCC, 0x4F, 0x36, 0x03};  // Indirizzo Bluetooth del modulo ELM327

float coolantTemp = 0.0;
float rpm = 0.0;
float intakeTemp = 0.0;
float obdVoltage = 0.0;
float engineLoad = 0.0;
float MAF = 0.0;
float lastCoolantTemp = -999.0; // Valore iniziale fuori da ogni possibile range di temperatura
float lastIntakeTemp = -999.0; // Valore iniziale fuori da ogni possibile range di temperatura

const unsigned long voltageQueryInterval = 4000; // Intervallo di 4 secondi

unsigned long lastVoltageQueryTime = 0;
unsigned long lastOBDQueryTime = 0;
unsigned long OBDQueryInterval = 1000; // Intervallo di query OBD in millisecondi

const int BUFFER_SIZE = 256;
char circularBuffer[BUFFER_SIZE];
int writeIndex = 0;
int readIndex = 0;

void setup() {
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillScreen(BLACK);
  Serial.begin(115200);
  Serial.println("Setup in corso...");
  
  BTconnect();
  delay(1500);
  ELMinit();
  delay(500);
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastOBDQueryTime >= OBDQueryInterval) {
    lastOBDQueryTime = currentMillis;
    dataRequestOBD();
    handleOBDResponse();
    updateDisplay();
  }
}


bool BTconnect() {
  ELM_PORT.begin(m5Name, true);  // Avvia la connessione Bluetooth
  #ifdef DEBUG
    displayDebugMessage("Connessione BT...", 0, 200, WHITE);
  #endif
  int retries = 0;  // Tentativo connessione bluetooth ELM327 (5 try)
  bool connected = false;
  while (retries < 5 && !connected) {
    connected = ELM_PORT.connect(BLEAddress);
    if (!connected) {
      #ifdef DEBUG
        displayDebugMessage("BT Conn FAIL", 0, 200, WHITE);
      #endif
      retries++;
      delay(500);
    }
  }

  if (!connected) {
    #ifdef DEBUG
      displayDebugMessage("ELM BT NOT FOUND", 0, 200, WHITE);
    #endif
    return false;  // Loop infinito se non riesce a connettersi
  } else {
    #ifdef DEBUG
      displayDebugMessage("Connessione BT OK!", 0, 200, WHITE);
    #endif
    return true;
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
    delay(50); // Breve delay per non sovraccaricare la CPU
  }
  response.trim(); // Rimuove spazi bianchi

  if (response.length() > 0) {
    #ifdef DEBUG
        displayDebugMessage(response.c_str(), 0 , 200, GREEN);
    #endif
  }

  if (response.length() == 0) {
    #ifdef DEBUG
      Serial.println("No RCV");
    #endif
    return false;
  }

  if (response.indexOf("OK") >= 0 || response.length() > 0) {
    return true;
  } else {
    Serial.println("Err: " + response);
    return false;
  }
}

void sendOBDCommand(const char* cmd) {
  ELM_PORT.print(cmd);
  ELM_PORT.print("\r\n");
}

String bufferSerialData(int timeout, int numChars) {
    unsigned long startTime = millis();
    while (millis() - startTime < timeout) {
        while (ELM_PORT.available()) {
            char c = ELM_PORT.read();
            writeToCircularBuffer(c);
        }
        delay(50); // Breve delay per non sovraccaricare la CPU
    }
    return readFromCircularBuffer(numChars);
}

void dataRequestOBD() {
  static unsigned long lastCommandTime = 0;
  static int commandIndex = 0;
  unsigned long currentMillis = millis();

  const char* commands[] = {"0105", "010C", "010F", "0104", "0110"};
  int numCommands = sizeof(commands) / sizeof(commands[0]);

  if (currentMillis - lastCommandTime >= OBDQueryInterval / numCommands) {
    if (commandIndex < numCommands) {
      sendOBDCommand(commands[commandIndex]);
      commandIndex++;
    } else {
      commandIndex = 0;
      if (millis() - lastVoltageQueryTime >= voltageQueryInterval) {
        sendOBDCommand("ATRV");
        lastVoltageQueryTime = millis();
      }
    }
    lastCommandTime = currentMillis;
  }
}

void handleOBDResponse() {
  String response = bufferSerialData(1000, 100);  // Riempimento del buffer
  parseOBDData(response);  // Parsing del buffer
}

void parseOBDData(const String& response) {
  if (response.indexOf("4105") >= 0) {
    coolantTemp = parseCoolantTemp(response);
  } else if (response.indexOf("410C") >= 0) {
    rpm = parseRPM(response);
  } else if (response.indexOf("410F") >= 0) {
    intakeTemp = parseIntakeTemp(response);
  } else if (response.indexOf("V") >= 0) {
    obdVoltage = parseOBDVoltage(response);
  } else if (response.indexOf("4104") >= 0) {
    engineLoad = parseEngineLoad(response);
  } else if (response.indexOf("4110") >= 0) {
    MAF = parseMAF(response);
  }
}

float parseCoolantTemp(const String& response) {
  if (response.length() >= 6 && response.indexOf("4105") == 0) {
    byte tempByte = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    return tempByte - 40;
  }
  return lastCoolantTemp;
}

float parseRPM(const String& response) {
  if (response.length() >= 8 && response.indexOf("410C") >= 0) {
    byte highByte = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    byte lowByte = strtoul(response.substring(6, 8).c_str(), NULL, 16);
    return (highByte * 256 + lowByte) / 4;
  }
  return 0.0;
}

float parseIntakeTemp(const String& response) {
  if (response.length() >= 6 && response.indexOf("410F") == 0) {
    byte tempByte = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    return tempByte - 40;
  }
  return lastIntakeTemp;
}

float parseOBDVoltage(const String& response) {
  int indexV = response.indexOf('V');
  if (indexV >= 0) {
    String voltageStr = response.substring(0, indexV);
    return voltageStr.toFloat();
  }
  return 0.0;
}
float parseEngineLoad(const String& response) {
  if (response.length() >= 8 && response.indexOf("4104") >= 0) {
    byte highByte = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    return (highByte * 100.0) / 255.0;
  }
  return 0.0;
}

float parseMAF(const String& response) {
  if (response.length() >= 6 && response.indexOf("4110") == 0) {
    int A = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    int B = strtoul(response.substring(6, 8).c_str(), NULL, 16);
    return ((A * 256) + B) / 100.0;
  }
  return 0.0;
}

void updateDisplay() {
  M5.Lcd.fillRect(0, 0, 320, 200, BLACK);  // Pulisce la parte inferiore del display

  if (coolantTemp < 50) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(LIGHTGREY);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  } else if (coolantTemp >= 40 && coolantTemp <= 65) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(BLUE);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  } else if (coolantTemp > 65 && coolantTemp <= 80) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(GREENYELLOW);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  }else if (coolantTemp >= 81 && coolantTemp <= 100) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  }  else if (coolantTemp <= 102) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(ORANGE);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  }else if (coolantTemp > 102) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.printf("Coolant Temp: %.1f C\n", coolantTemp);
  }

  if (obdVoltage < 11.8) {
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.printf("OBD Voltage: %.2f V\n", obdVoltage);
  } else if (obdVoltage > 11.8 && obdVoltage <= 12.1) {
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.printf("OBD Voltage: %.2f V\n", obdVoltage);
  }else if (obdVoltage > 12.1 && obdVoltage <= 13.8) {
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.printf("OBD Voltage: %.2f V\n", obdVoltage);
  } else if (obdVoltage > 13.8 && obdVoltage <= 14.5) {
    M5.Lcd.setTextColor(GREENYELLOW);
    M5.Lcd.printf("OBD Voltage: %.2f V\n", obdVoltage);
  } else if (obdVoltage > 14.5 ) {
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

void displayDebugMessage(const char* message, int x , int y, uint16_t textColour) {
  M5.Lcd.setTextColor(textColour);
  M5.Lcd.fillRect(x, y, 320, 40, BLACK);  // Pulisce la parte bassa del display
  M5.Lcd.setCursor(x, y);
  M5.Lcd.print(message);
  Serial.println(message);  // Mostra anche nel monitor seriale
}

bool ELMinit() {
  String response;

  #ifdef DEBUG
    displayDebugMessage("ELM init...", 0 ,200, WHITE);
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

void writeToCircularBuffer(char c) {
    circularBuffer[writeIndex] = c;
    writeIndex = (writeIndex + 1) % BUFFER_SIZE;
    if (writeIndex == readIndex) {
        readIndex = (readIndex + 1) % BUFFER_SIZE; // Sovrascrivi i dati più vecchi
    }
}

String readFromCircularBuffer(int numChars) {
    String result = "";
    while (readIndex != writeIndex) {
        result += circularBuffer[readIndex];
        readIndex = (readIndex + 1) % BUFFER_SIZE;
    }
    return result; // Rimuove spazi bianchi extra
}
