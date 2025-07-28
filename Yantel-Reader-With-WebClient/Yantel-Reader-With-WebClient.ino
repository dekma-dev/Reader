#include <WiFi.h> // necessary lib
#include <HTTPClient.h>

/*
  Documentation
  
  1. All Serial print's may be deleted, cause they are needed only for debugging.
  2. Reader have to have full-time working. Means that reading should not have delays.
  3. Client should not spam the server with requests, delays are needed here.
  4. For some reason asynchronous method yield() doesn't work in ESP32.
  5. Needs 5V voltage power supply.
  6. Available to add http-requests within authorization, it's can be useful if we want to filter access through that way penetration.

*/

byte bytePower[] = { 0xBB, 0x00, 0x27, 0x00, 0x03, 0x22, 0x27, 0x10, 0x83, 0x7E }; //power bytes code, needed for awake reader.

unsigned long awaking; //awakes the reader.
unsigned long sending; //sends data to the server
unsigned long closingReadingTimer; //helps avoid reading interference
unsigned long reading; //timer for choppy reading
unsigned long reloadConnect; //reload the microcontroller system for re-connect to network
unsigned long worktimeTimer; //needs for mark's work time calculation 
unsigned long greenIndicatorTimer;

const char* ssid = "DESKTOP-N40TMFN"; //own network's SSID
const char* password = "efrgthyjuk566"; //and the password
const char* serverName = "http://monitoring.rt:80/monitoring/sending";

const char* host = "http://172.20.10.3"; //server's ip-address
const int httpPort = 80; //check in the server's settings

String request = "", currentMark = "", leftMark = "";
int closingButton = 18; //отдельная линия питания. разьем fakra
unsigned long worktime = 0;
uint32_t closingCount = 1;
WiFiClient client;

//Condition sign
//исправить на 32-34 пины, поскольку так правильнее, а на макете возможны только следующие пины задействовать:
int greenLED = 32, yellowLED = 25, redLED = 33;
/*HTTP response code:
 -1/A - waiting / connected to WiFi, but HTTP requests are unsuccessful
 200 - typical OK statement
 408 - no WiFi connection 
*/
int httpResponseCode = -1;

void setup() {
  pinMode(greenLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(redLED, OUTPUT);

  pinMode(closingButton, INPUT);
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH);

  Serial.begin(115200); //COM-port needed only for debugging
  Serial2.begin(115200); //COM-port needed for communications with reader
  delay(10);

  WiFi.begin(ssid, password); //connecting to local network

  while (WiFi.status() != WL_CONNECTED) { //checking connection
    httpResponseCode = 408;
    digitalWrite(redLED, HIGH);
    delay(1000); 
    
    Serial.println(String(WiFi.status()));

    if (millis() - reloadConnect > 10000) { 
      WiFi.disconnect();
      WiFi.begin(ssid, password, 0, NULL, false);
      WiFi.reconnect();
      
      reloadConnect = millis();
    }
  }

  greenIndicatorTimer = millis();
  
  sendQuery(bytePower, sizeof(bytePower)); //reader awaking
}

void loop() {
  // if (millis() - reloadConnect > 1000) Serial.println(httpResponseCode);

  if (WiFi.status() != WL_CONNECTED) httpResponseCode = 408;
  else if (httpResponseCode != 200) httpResponseCode = -1;

  if (httpResponseCode == 408) {
    digitalWrite(greenLED, LOW);
    digitalWrite(yellowLED, LOW);
    digitalWrite(redLED, HIGH);
  } else if (httpResponseCode == 200 && (millis() - greenIndicatorTimer > 250)) {
    digitalWrite(greenLED, digitalRead(greenLED ) ^ 1);
    digitalWrite(yellowLED, LOW);
    digitalWrite(redLED, LOW);
    greenIndicatorTimer = millis();
  } else if (httpResponseCode == -1) {
    digitalWrite(greenLED, LOW);
    digitalWrite(yellowLED, HIGH);
    digitalWrite(redLED, LOW);
  }

  while (WiFi.status() != WL_CONNECTED) { //checking connection
    delay(1000); 

    if (millis() - reloadConnect > 10000) { 
      WiFi.disconnect();
      WiFi.begin(ssid, password, 0, NULL, false);
      WiFi.reconnect();
      
      reloadConnect = millis();
    }
  }

  if (millis() - worktimeTimer > 1000) {
    if (currentMark == leftMark) {
      worktime++;
      worktimeTimer = millis();
    }
    else worktime = 0;
  }

  request = "?RFID=";
  leftMark = currentMark;

  if (Serial2.available()) {
    byte bts[256]; //256 - too  few (mem overflow), 1024 - too slow reading.
    byte i = 0;
    while (Serial2.available()) {
      bts[i] = Serial2.read();
      delay(1); //the argument was equal to 1; increase may help for avoid "mem overflow" exception.
      i++;
      if (i == 256) {
        while (Serial2.available()) {
          Serial2.read();
          delay(10);
          Serial.println("Overload");
        }
      }
    }
    for (byte a = 9, index = 0; a < 19; a++, index++) {
      request += String(bts[a], HEX);
      currentMark += String(bts[a], HEX);
    }

    if (millis() - sending > 29900) {
      sending = millis(); 
      sendGETtHttpRequest(request);
    }
  }       

  //probably should be the same that up there
  if (millis() - awaking > 70000) {
    awaking = millis();  
    sendQuery(bytePower, sizeof(bytePower));
  }

  if (millis() - closingReadingTimer > 50 && digitalRead(closingButton)) {
    closingCount++;
    closingReadingTimer = millis();
  }
}

void sendGETtHttpRequest(String request) {
  Serial.println(request);

   if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;

    request += "&ID_stanok=" + String(id_stanok);
    request += "&Count=" + String(closingCount);
    request += "&WorkTime=" + String(worktime);
    request += "&State=Установлена&Purpose=None&Country=None HTTP/1.1\r\nHost: monitoring.rt\r\nConnection: close\r\n\r\n"; //State должно быть строкой, как в бд

    String serverPath = serverName + request;
    
    http.begin(serverPath.c_str()); 
    httpResponseCode = http.GET();
    http.end();

    Serial.println(serverPath);
    closingCount = 1;

  } else  Serial.println("WiFi Disconnected");
}

void sendQuery(byte bytes[], byte len) { 
  for (byte i = 0; i < len; i++) Serial2.write(bytes[i]);
}