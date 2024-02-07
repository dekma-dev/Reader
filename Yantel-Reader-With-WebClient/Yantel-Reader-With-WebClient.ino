#include <WiFi.h>

byte bytePower[] = { 0xBB, 0x00, 0x27, 0x00, 0x03, 0x22, 0x27, 0x10, 0x83, 0x7E }; //power bytes code, needed for awake reader.
unsigned long awaking; //awakes the reader.
unsigned long sending; //sends data to the server

const char* ssid = "Patriarche Damir"; //own network's SSID
const char* password = "snrk6276"; //and the password

const char* host = "192.168.53.208"; //server's ip-address
const int httpPort = 8000; //check in the server's settings

WiFiClient client;

void setup() {
  Serial.begin(115200); //COM-port needed only for debugging
  Serial2.begin(115200); //COM-port needed for communications with reader
  delay(10);

  Serial.print("\nConnecting to: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password); //connecting to local network

  while (WiFi.status() != WL_CONNECTED) { //checking connection
    Serial.print(".");
    delay(500);
  }
  
  Serial.print("This is the connected user: ");
  Serial.println(WiFi.localIP());
  
  Serial.print("Connecting to: ");
  Serial.println(host);
  
  sendQuery(bytePower, sizeof(bytePower)); //reader awaking
}

void loop() {
  delay(10000); //needed to avoid microcontroller overload 

  if (!client.connect(host, httpPort)) { //try to move to the setup
    Serial.println("Connection to server failed.");
    return;
  }

  int id_stanok = random(1, 3); //random data needed for request
  int count = 0; //random data needed for request; count of closure

  if (Serial2.available() > 0) {
    byte bts[256]; //256 - too few (mem overflow), 1024 - too slow reading.
    byte i = 0;
    while (Serial2.available()) {
      bts[i] = Serial2.read();
      delay(1); //the argument was equal to 1; increase may help for avoid "mem overflow" exception.
      i++;
      count++;
      if (i == 256) {
        while (Serial2.available()) {
          Serial2.read();
          delay(10);
          Serial.println("Overload"); //never has been seen even bts array had 1024 byte
        } 
      }
    }
    // delay(50); //for avoid wrong reading
    if (i == 24) { //вывод метки, где 8 - "помеха", а 6 последних бита - биты четности скорее всего. 
                   //коэффициент в условии, например, 24 влияет на скорость вывода метки, влияение на дальность сомнительно.
      for (byte a = 9; a < 19; a++) {
        Serial.print(bts[a], HEX); //convertation can be deleted.
        Serial.print("|");
      }
      delay(5);
      Serial.println();
    }
      // delay(150); //for avoid wrong reading
  }

    //url = "http://127.0.0.1:8000/monitoring/sending/?ID_stanok=" + (String)id_stanok + "&RFID=" + RFID + "&Count=" + (String)count + "&State=" + (String)state + "&Purpose=" + (String)purpose + "&Country=" + (String)country;
    
  if ((uint8_t)(millis() - sending) > 4000) { //idk why its doesnt work
    sending = millis();
    Serial.println("sending request...");
    
    String url = "/monitoring/sending/?ID_stanok=124&RFID=SF234AS&Count=232234&State=1&Purpose=wqewqe&Country=qweqwe";

    if (client.connected()) { 
      client.printf("GET" + url + "HTTP/1.1\r\nHost: 192.168.53.208\r\nConnection: close\r\n\r\n");  //sending request to the server
    }
  }

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }

  Serial.println("Closing connection.");

  if (millis() - awaking > 15000) { //can be highlighted to asynchronous method yield(); uint8_t for avoid uncorrect working.
    awaking = millis(); 
    sendQuery(bytePower, sizeof(bytePower));
  } 
}


void sendQuery(byte bytes[], byte len) { 
  Serial.println(len);
  for (byte i = 0; i < len; i++)
    Serial2.write(bytes[i]);
  for (byte i = 0; i < len; i++) {
    Serial.print(bytes[i], HEX);
    Serial.print('\t');
  }
}