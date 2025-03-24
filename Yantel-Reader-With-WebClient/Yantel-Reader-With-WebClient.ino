#include <WiFi.h> // necessary lib

/*
  Documentation
  
  1. All Serial print's may be deleted, cause they are needed only for debugging.
  2. Reader have to have full-time working. Means that reading should not have delays.
  3. Client should not spam the server with requests, delays are needed here.
  4. For some reason asynchronous method yield() doesn't work in ESP32.

*/

byte bytePower[] = { 0xBB, 0x00, 0x27, 0x00, 0x03, 0x22, 0x27, 0x10, 0x83, 0x7E }; //power bytes code, needed for awake reader.

unsigned long awaking; //awakes the reader.
unsigned long sending; //sends data to the server
unsigned long buttonTimer; //helps avoid reading interference
unsigned long reading; //timer for choppy reading
unsigned long reloadConnect; //reload the microcontroller system for re-connect to network
unsigned long worktimeTimer; //needs for mark's work time calculation 

const char* ssid = "Patriarche"; //own network's SSID
const char* password = "snrk727776"; //and the password

const char* host = "192.168.198.208"; //server's ip-address
const int httpPort = 80; //check in the server's settings

struct Button {
  const uint8_t pin;
  uint32_t closingCount;
  bool pressed;
};

Button closingButton = {18, 1, false};
String request = "", currentMark = "", leftMark = "";
unsigned long worktime = 0;
WiFiClient client;

void setup() {
  pinMode(closingButton.pin, INPUT_PULLUP);
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH);

  Serial.begin(115200); //COM-port needed only for debugging
  Serial2.begin(115200); //COM-port needed for communications with reader
  delay(10);

  Serial.print("\nConnecting to: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password); //connecting to local network

  while (WiFi.status() != WL_CONNECTED) { //checking connection
    Serial.print(".");
    delay(1000); 

    if (millis() - reloadConnect > 10000) { 
      WiFi.disconnect();
      Serial.println("Connecting...");
      WiFi.begin(ssid, password, 0, NULL, false);
      WiFi.reconnect();
      
      reloadConnect = millis();
    }
  }
  Serial.println();
  
  Serial.print("This is the connected user: ");
  Serial.println(WiFi.localIP());
  
  Serial.print("Connecting to: ");
  Serial.println(host);
  
  sendQuery(bytePower, sizeof(bytePower)); //reader awaking

  attachInterrupt(closingButton.pin, calculateClosing, HIGH);
}

void loop() {
  while (WiFi.status() != WL_CONNECTED) { //checking connection
    Serial.print(".");
    delay(1000); 

    if (millis() - reloadConnect > 10000) { 
      WiFi.disconnect();
      Serial.println("Connecting...");
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

  request = "GET /monitoring/sending/?RFID=";
  leftMark = currentMark;

  // if (millis() - reading > 5000) {
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
      // delay(50);
      if (i == 24) { //вывод метки, где 8 - "помеха", а 6 последних бита - биты четности скорее всего. 
                    //коэффициент в условии, например, 24 влияет на скорость вывода метки, влияение на дальность сомнительно.
        for (byte a = 9, index = 0; a < 19; a++, index++) {
          request += String(bts[a], HEX);
          currentMark += String(bts[a], HEX);
        }

        if (millis() - sending > 60000) {
          sending = millis(); 
          sendHttpRequest(request);
        }
      }
    }       

  if (millis() - awaking > 70000) {
    awaking = millis();   
    sendQuery(bytePower, sizeof(bytePower));
  }

  if (closingButton.pressed && !digitalRead(closingButton.pin) && millis() - buttonTimer > 50) {
    closingButton.pressed = false;
    buttonTimer = millis();
  }
}

void calculateClosing() {
  if (!closingButton.pressed && millis() - buttonTimer > 50) {
    closingButton.closingCount++;
    closingButton.pressed = true;
    buttonTimer = millis();
  }
}

void sendHttpRequest(String request) {
  Serial.printf("This is mark's working time: %d", worktime);

  if (!client.connect(host, httpPort)) { //try to move to the setup
    Serial.println("Connection to server failed.");
    return;
  }

  int id_stanok = 3; //random data needed for request


  request += "&ID_stanok=" + String(id_stanok);
  request += "&Count=" + String(closingButton.closingCount);
  request += "&WorkTime=" + String(worktime);
  request += "&State=Установлена&Purpose=None&Country=None HTTP/1.1\r\nHost: 192.168.198.208\r\nConnection: close\r\n\r\n"; //State должно быть строкой, как в бд

  Serial.println("sending request...");
  Serial.println(request);


  if (client.connected()) { 
    client.print(request);  //sending request to the server
  }

  closingButton.closingCount = 1;
 
  Serial.println("Closing connection.");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }
}

void sendQuery(byte bytes[], byte len) { 
  Serial.println(len);
  for (byte i = 0; i < len; i++)
    Serial2.write(bytes[i]);
  for (byte i = 0; i < len; i++) { //just for debugging
    Serial.print(bytes[i], HEX);
    Serial.print('\t');
  }
}