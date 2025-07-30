#include <WiFi.h>
#include <HTTPClient.h>

byte bytePower[] = { 0xBB, 0x00, 0x27, 0x00, 0x03, 0x22, 0x27, 0x10, 0x83, 0x7E }; //power bytes code, needed for awake reader.

unsigned long awaking; //awakes the reader.
unsigned long sending; //sends data to the server
unsigned long reading; //timer for choppy reading
unsigned long reloadConnect; //reload the microcontroller system for re-connect to network
unsigned long worktimeTimer; //needs for mark's work time calculation 
unsigned long greenIndicatorTimer;

const char* ssid = "Patriarche";
const char* password = "snrk727776";
const char* serverName = "http://192.168.198.208:80/monitoring/sending";

const char* host = "192.168.198.208";
const int httpPort = 80;

String request = "", currentMark = "", leftMark = "";
int closingButton = 18; //отдельная линия питания. разьем fakra
unsigned long worktime = 0;
uint32_t closingCount = 0;
WiFiClient client;

//Condition sign
int greenLED = 4, yellowLED = 19, redLED = 21;
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

  while (WiFi.status() != WL_CONNECTED) {
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
    }       

  if (millis() - awaking > 70000) {
    awaking = millis();  
    sendQuery(bytePower, sizeof(bytePower));
  }

  if (millis() - buttonTimer > 50) {
    closingCount++;
    buttonTimer = millis();
  }
}

void sendGETtHttpRequest(String request) {
   if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;

    int id_stanok = random(1, 10); //random data needed for request

    request += "&ID_stanok=" + String(id_stanok);
    request += "&Count=" + String(closingCount);
    request += "&WorkTime=" + String(worktime);
    request += "&State=Установлена&Purpose=None&Country=None HTTP/1.1\r\nHost: 192.168.198.208\r\nConnection: close\r\n\r\n"; //State должно быть строкой, как в бд

    String serverPath = serverName + request;
    
    http.begin(serverPath.c_str());
    
    httpResponseCode = http.GET();
      
    http.end();

    closingCount = 1;

  } else  Serial.println("WiFi Disconnected");
}

void sendQuery(byte bytes[], byte len) { 
  for (byte i = 0; i < len; i++) Serial2.write(bytes[i]);
  for (byte i = 0; i < len; i++) { //just for debugging
  }
}
