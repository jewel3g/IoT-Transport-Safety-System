#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include <MFRC522.h>

// ---------------- Pins ----------------
SoftwareSerial sim800(7, 8);  // RX, TX
SoftwareSerial gpsSerial(4, 3); // GPS RX, TX
TinyGPSPlus gps;

#define RST_PIN 9
#define SS_PIN 10
MFRC522 rfid(SS_PIN, RST_PIN);

// ---------------- Sensors ----------------
#define accidentPin 2
#define overspeedThreshold 60   // km/h
#define firePin A0
#define harassmentPin 5
#define alcoholPin A1

// ---------------- Network ----------------
String apn = "internet";           // SIM APN
String server = "yourserver.com";  // IoT server URL
String url = "/api/data";
String user = "";
String pass = "";

// ---------------- Runtime ----------------
String driverID = "Unknown";

// ---------------- Setup ----------------
void setup() {
  Serial.begin(9600);
  sim800.begin(9600);
  gpsSerial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(accidentPin, INPUT);
  pinMode(firePin, INPUT);
  pinMode(harassmentPin, INPUT);
  pinMode(alcoholPin, INPUT);

  Serial.println("System Starting...");
  initSIM800();
}

// ---------------- Main Loop ----------------
void loop() {
  // Read GPS
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  // RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    driverID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      driverID += String(rfid.uid.uidByte[i], HEX);
    }
    driverID.toUpperCase();
    Serial.println("Driver ID: " + driverID);
    rfid.PICC_HaltA();
  }

  // Sensors
  int accident = digitalRead(accidentPin);
  int fire = analogRead(firePin) > 500 ? 1 : 0;
  int harassment = digitalRead(harassmentPin);
  int alcohol = analogRead(alcoholPin) > 400 ? 1 : 0;

  // GPS Data
  double lat = gps.location.isValid() ? gps.location.lat() : 0;
  double lng = gps.location.isValid() ? gps.location.lng() : 0;
  double speed = gps.speed.kmph();

  int overspeed = (speed > overspeedThreshold) ? 1 : 0;

  // JSON Payload
  String payload = "{";
  payload += "\"driver\":\"" + driverID + "\",";
  payload += "\"accident\":" + String(accident) + ",";
  payload += "\"overspeed\":" + String(overspeed) + ",";
  payload += "\"fire\":" + String(fire) + ",";
  payload += "\"harassment\":" + String(harassment) + ",";
  payload += "\"alcohol\":" + String(alcohol) + ",";
  payload += "\"lat\":" + String(lat, 6) + ",";
  payload += "\"lng\":" + String(lng, 6) + ",";
  payload += "\"speed\":" + String(speed, 2);
  payload += "}";

  Serial.println("Sending: " + payload);

  // Send to server
  sendData(payload);

  delay(10000); // every 10s
}

// ---------------- GPRS Setup ----------------
void initSIM800() {
  sendCommand("AT");
  sendCommand("AT+CPIN?");
  sendCommand("AT+CREG?");
  sendCommand("AT+CGATT?");
  sendCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  sendCommand("AT+SAPBR=3,1,\"APN\",\"" + apn + "\"");
  if (user != "") sendCommand("AT+SAPBR=3,1,\"USER\",\"" + user + "\"");
  if (pass != "") sendCommand("AT+SAPBR=3,1,\"PWD\",\"" + pass + "\"");
  sendCommand("AT+SAPBR=1,1");
  sendCommand("AT+SAPBR=2,1");
}

// ---------------- HTTP Send ----------------
void sendData(String json) {
  sendCommand("AT+HTTPTERM"); // reset
  sendCommand("AT+HTTPINIT");
  sendCommand("AT+HTTPPARA=\"CID\",1");
  sendCommand("AT+HTTPPARA=\"URL\",\"" + server + url + "\"");
  sendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  
  sim800.println("AT+HTTPDATA=" + String(json.length()) + ",10000");
  delay(100);
  sim800.println(json);
  delay(500);

  sendCommand("AT+HTTPACTION=1"); // POST
  delay(3000);
  sendCommand("AT+HTTPREAD");
  sendCommand("AT+HTTPTERM");
}

// ---------------- AT Command Helper ----------------
void sendCommand(String cmd) {
  sim800.println(cmd);
  delay(500);
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
}
