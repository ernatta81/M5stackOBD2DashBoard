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
#define m5Name "M5Stack_OBD"
#define BUFFER_SIZE 10 //Buffer circolare

#include <M5Stack.h>
#include <BluetoothSerial.h>

BluetoothSerial ELM_PORT;

struct OBDData{
  String response;
  unsigned long timestamp;

};

OBDData buffer[BUFFER_SIZE];
int head = 0;
int tail = 0;

// Dichiarazione funzioni
float getCoolantTemp();
float getRPM();
float getIntakeTemp();
float getOBDVoltage();
float getEngineLoad();
float getMAF();
bool ELMinit();
bool BTconnect();
bool sendAndReadCommand(const char* cmd, String& response, int delayTime);
bool isBufferEmpty();
void enqueue(String response);
void updateDisplay();
void dataRequestOBD();
void displayDebugMessage(const char* message, int x , int y, uint16_t textColour);
void processResponses();

OBDData dequeue();

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
unsigned long OBDQueryInterval = 500; // Intervallo di query OBD in millisecondi

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

  if (response.indexOf("OK") >= 0 || response.length() > 0) {  // Aggiungi un controllo per la risposta OK se desiderato
    return true;
  } else {
    Serial.println("Err: " + response);
    return false;
  }
}

float getCoolantTemp(String response) {
  if (response.length() >= 6 && response.indexOf("4105") == 0) {
    byte tempByte = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    return tempByte - 40;
  }
  return -999.0; // Valore fuori range se non c'è risposta valida
}

float getRPM(String response) {
  if (response.length() >= 8 && response.indexOf("410C") >= 0) {
    byte highByte = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    byte lowByte = strtoul(response.substring(6, 8).c_str(), NULL, 16);
    return (highByte * 256 + lowByte) / 4;
  }
  return -999.0; // Valore fuori range se non c'è risposta valida
}

float getEngineLoad(String response) {
  if (response.length() >= 8 && response.indexOf("4104") >= 0) {
    byte highByte = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    return (highByte * 100.0) / 255.0;
  }
  return -999.0; // Valore fuori range se non c'è risposta valida
}

float getIntakeTemp(String response) {
  if (response.length() >= 6 && response.indexOf("410F") == 0) {
    byte tempByte = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    return tempByte - 40;
  }
  return -999.0; // Valore fuori range se non c'è risposta valida
}

float getOBDVoltage(String response) {
  response.trim();
  int indexV = response.indexOf('V');
  if (indexV >= 0) {
    String voltageStr = response.substring(0, indexV);
    return voltageStr.toFloat();
  }
  return -999.0; // Valore fuori range se non c'è risposta valida
}


float getMAF(String response) {
  if (response.length() >= 6 && response.indexOf("4110") == 0) {
    int A = strtoul(response.substring(4, 6).c_str(), NULL, 16);
    int B = strtoul(response.substring(6, 8).c_str(), NULL, 16);
    return ((A * 256) + B) / 100.0;
  }
  return -999.0; // Valore fuori range se non c'è risposta valida
}


void updateDisplay() {
  M5.Lcd.fillRect(0, 0, 320, 200, BLACK);  // Pulisce la parte inferiore del display

  if (coolantTemp < 50) {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(LIGHTGREY);
//    M5.Lcd.setTextColor(coolantTemp < 50 ? LIGHTGREY : (coolantTemp <= 81 ? ORANGE : (coolantTemp <= 97 ? GREEN : RED)));
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
    #endif
    return false;  // Loop infinito se non riesce a connettersi
  }
  else {
    #ifdef DEBUG
      displayDebugMessage("Connessione BT OK!", 0 , 200, WHITE);
    #endif
    return true;
  }
}

void enqueue(String response){
  buffer[head].response = response;
  buffer[head].timestamp = millis();
  head = (head + 1) % BUFFER_SIZE;
  if (head == tail){
    tail = (tail + 1) % BUFFER_SIZE;
  }
}

OBDData dequeue(){
  OBDData data = buffer[tail];
  tail = (tail + 1) % BUFFER_SIZE;
  return data;
}

bool isBufferEmpty(){
  return head == tail;
}

void processResposes(){
  while(!isBufferEmpty()){
    OBDData data = dequeue();
    #ifdef DEBUG
      Serial.println("Preocessing " + data.response);
    #endif
  }
}

void dataRequestOBD(){
  unsigned long currentMillis = millis();
  static unsigned long lastCoolantRequestTime = 0;

  if ( currentMillis - lastCoolantRequestTime >= OBDQueryInterval){
    lastCoolantRequestTime = currentMillis;
    String response;
    if(sendAndReadCommand("0105", response, 2000)){
      enqueue(response);
    }
  }

  static unsigned long lastIntakeTempRequestTime  = 0;

  if ( currentMillis - lastIntakeTempRequestTime  >= OBDQueryInterval){
    lastIntakeTempRequestTime  = currentMillis;
    String response;
    if(sendAndReadCommand("010F", response, 2000)){
      enqueue(response);
    }
  }

  static unsigned long lastRPMRequestTime   = 0;

  if ( currentMillis - lastRPMRequestTime   >= OBDQueryInterval){
    lastRPMRequestTime   = currentMillis;
    String response;
    if(sendAndReadCommand("010C", response, 2000)){
      enqueue(response);
    }
  }  

  static unsigned long lastEngineLoadRequestTime    = 0;

  if ( currentMillis - lastEngineLoadRequestTime    >= OBDQueryInterval){
    lastEngineLoadRequestTime    = currentMillis;
    String response;
    if(sendAndReadCommand("0104", response, 2000)){
      enqueue(response);
    }
  }  

 if (currentMillis - lastVoltageQueryTime >= voltageQueryInterval) {   // Richiede la tensione ogni 4000 ms 
   String response;
   if (sendAndReadCommand("ATRV", response, 1000)) {
     enqueue(response); // Metti in coda la risposta 
   }
   lastVoltageQueryTime = currentMillis; 
 }

  processResponses();
}

void processResponses(){
  while (!isBufferEmpty()){
    OBDData data = dequeue(); 
    if (data.response.indexOf("4105") >= 0){
      coolantTemp = getCoolantTemp(data.response);
    }else if (data.response.indexOf("410C") >= 0){
      rpm = getRPM(data.response); 
     }else if (data.response.indexOf("410F") >= 0){
       intakeTemp = getIntakeTemp(data.response); 
      }else if (data.response.indexOf("ATRV") >= 0){
        obdVoltage = getOBDVoltage(data.response);
       }else if (data.response.indexOf("4110") >= 0){
         MAF = getMAF(data.response);
        }else if (data.response.indexOf("4104") >= 0){
          engineLoad = getEngineLoad(data.response); 
         }
    }
}