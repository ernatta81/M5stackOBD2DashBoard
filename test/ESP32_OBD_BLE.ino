#include <Arduino.h>
#include <M5Stack.h>
#include <BluetoothSerial.h>

#define BTPORT SerialBT 
#define m5Name "M5Reader"
#define PWRDelay 65
#define MSGDelay 2000
#define DebugDelay 5000

BluetoothSerial SerialBT;
uint8_t LCD_BRIGHTNESS = 50;
uint8_t BLEAddress[6] = {0x00, 0x10, 0xCC, 0x4F, 0x36, 0x03};
char responseBuffer[128];
int responseLength = 0;
char c;

float rpm = 0;
float coolantTemp = 0;
float mafValue = 0;
float obdVoltage = 0;

void receiveELM() {
  responseLength = 0;
  unsigned long startTime = millis();
  Serial.print("Start receiving ELM327 data... ");
  while (millis() - startTime < 2000) {
    while (BTPORT.available()) {
      c = BTPORT.read();
      responseBuffer[responseLength++] = c;
      if (responseLength >= sizeof(responseBuffer) - 1) {
        break;
      }
    }
    if (responseLength > 0) {
      responseBuffer[responseLength] = '\0';
      Serial.println("<--- : " + String(responseBuffer));
      return;
    }
  }
  Serial.println("No response received");
}

bool startUp() {
  M5.Lcd.setBrightness(LCD_BRIGHTNESS);
  M5.Lcd.setTextColor(0xE73C, TFT_BLACK);
  M5.Lcd.setFreeFont(&FreeSans9pt7b);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.drawJpgFile(SD, "/p2.jpg"); // Logo on uSD

  for (int i = 0; i < PWRDelay; i++) {
    M5.Lcd.fillRect(0, 235, i * 5, 5, BLUE);
    delay(150);
  }
  M5.Lcd.setCursor(50, 150);
  M5.Lcd.println(" Waiting OBD device");
  if (!BTPORT.begin(m5Name, true)) {
    Serial.println("Failed to start Bluetooth");
    return false;
  }
  BTPORT.setPin("1234");
  M5.Lcd.setCursor(80, 190);
  M5.Lcd.println(" - Searching - ");
  int k = 0;
  do {
    M5.Lcd.fillRect(0, 235, k * 10, 10, ORANGE);
    k++;
    Serial.print(".");
  } while (!BTPORT.connect(BLEAddress) && k < 20);
  Serial.println();
  if (!BTPORT.connected()) {
    M5.Lcd.setCursor(50, 150);
    M5.Lcd.println("Failed to connect to OBD device");
    Serial.println("Failed to connect to OBD device");
    return false;
  }
  M5.Lcd.setCursor(100, 210);
  M5.Lcd.println("OBD BLE OK!");
  M5.Lcd.setCursor(65, 230);
  M5.Lcd.println("Waiting ECU response");
  delay(DebugDelay);

  // Invia il comando AT SP 6 e verifica la risposta
  BTPORT.print("AT SP 6"); // Imposta protocollo ISO 15765-4 (CAN 11/500)
  Serial.println("SEND AT SP 6 ---> ");
  receiveELM();
  if (responseLength > 0) {
    M5.Lcd.setCursor(50, 130);
    M5.Lcd.println("OBD Protocol detected");
    Serial.println("OBD Protocol detected");
  } else {
    M5.Lcd.setCursor(50, 150);
    M5.Lcd.println("Failed to detect OBD Protocol");
    Serial.println("Failed to detect OBD Protocol");
    return false; // Esci se non viene ricevuta una risposta
  }

  BTPORT.print("AT Z");
  Serial.println("SEND AT Z ---> ");
  receiveELM();
  delay(MSGDelay);
  M5.Lcd.clearDisplay();
  M5.Lcd.setCursor(50, 130);
  if (responseLength > 0) {
    M5.Lcd.println("ECU OK");
    Serial.println("ECU OK");
    delay(DebugDelay);
    return true;
  } else {
    M5.Lcd.println("No ECU response");
    Serial.println("No ECU response");
    return false;
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);
  Serial.println("System initializing...");
  if (startUp()) {
    Serial.println("System startup successful");
    BTPORT.print("AT @1");
    Serial.println("SEND AT @1 --->");
    receiveELM();
    delay(50);
  } else {
    Serial.println("System startup failed");
  }
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    Serial.println("BT1 pressed - Disconnecting Bluetooth");
    M5.Lcd.clearDisplay();
    M5.Lcd.setCursor(50, 130);
    M5.Lcd.println("Disconnecting Bluetooth...");
    BTPORT.disconnect();
    M5.Lcd.println("Bluetooth disconnected");
    Serial.println("Bluetooth disconnected");
    delay(2000);
    ESP.restart(); // Riavvia per garantire che tutto sia ripristinato
  }

  // Leggi i dati dall'ELM327
  BTPORT.print("010C"); // Richiedi RPM
  Serial.println("Requesting RPM...");
  receiveELM();
  if (responseLength > 0) {
    String rpmResponse = String(responseBuffer).substring(6);
    rpm = strtol(rpmResponse.c_str(), NULL, 16) / 4;
    Serial.println("RPM: " + String(rpm));
  } else {
    Serial.println("Failed to get RPM");
  }

  BTPORT.print("0105"); // Richiedi temperatura del liquido refrigerante
  Serial.println("Requesting coolant temperature...");
  receiveELM();
  if (responseLength > 0) {
    String tempResponse = String(responseBuffer).substring(6);
    coolantTemp = strtol(tempResponse.c_str(), NULL, 16) - 40;
    Serial.println("Coolant Temp: " + String(coolantTemp) + " C");
  } else {
    Serial.println("Failed to get coolant temperature");
  }

  BTPORT.print("0110"); // Richiedi valore MAF
  Serial.println("Requesting MAF value...");
  receiveELM();
  if (responseLength > 0) {
    String mafResponse = String(responseBuffer).substring(6);
    mafValue = strtol(mafResponse.c_str(), NULL, 16) / 100.0;
    Serial.println("MAF: " + String(mafValue) + " g/s");
  } else {
    Serial.println("Failed to get MAF value");
  }

  M5.Lcd.clearDisplay();
  M5.Lcd.setCursor(10, 0);
  M5.Lcd.println("RPM: " + String(rpm));
  M5.Lcd.println("Coolant Temp: " + String(coolantTemp) + " C");
  M5.Lcd.println("MAF: " + String(mafValue) + " g/s");

  BTPORT.print("ATRV");
  Serial.println("SEND ATRV --->");
  receiveELM();
  if (responseLength > 0) {
    obdVoltage = atof(responseBuffer);
    M5.Lcd.println("OBD Voltage: " + String(obdVoltage) + " V");
    Serial.println("OBD Voltage: " + String(obdVoltage) + " V");
  } else {
    Serial.println("Failed to get OBD voltage");
  }

  delay(DebugDelay);
}
