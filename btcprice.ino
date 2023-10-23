#include "WiFiS3.h"
#include "WiFiSSLClient.h"
#include "IPAddress.h"
#include <ArduinoJson.h>
#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;
WiFiSSLClient client;

#define SAMPLE_TIME 300// seconds
#define CANDLE_STYLE 2 // 0= closing only 1= opening only 2= movement filled 3 = open en close 4 = movement filled last candle live

long timeStamp;
long prevTimeStamp;
uint32_t priceOpen;
uint32_t priceHigh;
uint32_t priceLow;
uint32_t priceLast;
uint32_t priceBid;
uint32_t priceAsk;
uint32_t rangeHigh;
uint32_t rangeLow;

char ssid[] = "";        // your network SSID (name)
char pass[] = "";        // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;

char server[] = "www.bitstamp.net";    // broker server
int port = 443; // broker ssl port

uint32_t prevPriceBTC[12];

byte frame[8][12] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

void setup(){
  Serial.begin(9600);
  pinMode(13, OUTPUT);

  while (!Serial){
    ; // wait for serial port to connect. Needed for native USB port only
  }

  matrix.begin();

  if (WiFi.status() == WL_NO_MODULE){
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }
  
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION){
    Serial.println("Please upgrade the firmware");
  }
  
  while (status != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
     
    delay(10000);
  }
  
  printWifiStatus();
 
  Serial.println("\nStarting connection to server...");
  
  if (client.connect(server, port)){
    client.println("GET /api/v2/ticker/btcusdt/ HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println("Connection: close");
    client.println();
  }
}

void read_response(){
  char tempFlag[301];
  char jsonFlag[301];

  while (client.available()){
    DynamicJsonDocument doc(1024);
    client.readBytesUntil(0x7B, tempFlag, 300);
    
    for(int i = 0; i < 300; i++){
      tempFlag[i]= 0x00;
    }

    client.readBytesUntil(0x7D, tempFlag, 300);

    for(int i = 0; i < 300; i++){
      jsonFlag[i]= 0x00;
    }

    jsonFlag[0] = 0x7B;
    
    for(int i = 1; i < (296); i++){
      jsonFlag[i]= tempFlag[(i-1)];
    }
      
    jsonFlag[(strlen(tempFlag)+1)] = 0x7D;
    deserializeJson(doc, jsonFlag);
    timeStamp = doc["timestamp"];
    priceOpen = doc["open"];
    priceHigh = doc["high"];
    priceLow = doc["low"];
    priceLast = doc["last"];
    priceBid = doc["bid"];
    priceAsk = doc["ask"];

    if((((priceAsk + priceBid) / 2) > 0)){ 
      Serial.print("Timestamp: ");
      Serial.println(timeStamp);
      Serial.print("Open: ");
      Serial.println(priceOpen);
      Serial.print("High: ");
      Serial.println(priceHigh);
      Serial.print("Low: ");
      Serial.println(priceLow);
      Serial.print("Last: ");
      Serial.println(priceLast);
      Serial.print("Bid: ");
      Serial.println(priceBid);
      Serial.print("Ask: ");
      Serial.println(priceAsk);
      Serial.print("The price of Bitcoin is: ~");
      Serial.print(((priceAsk + priceBid) / 2));
      Serial.println(" Usdt");
    
      if((prevPriceBTC[0] < ((priceAsk + priceBid) / 2))){
        digitalWrite(13, HIGH);
        Serial.println("Bitcoin going up!");
      }
      else if(prevPriceBTC[0] > ((priceAsk + priceBid) / 2)){
        digitalWrite(13, LOW);
        Serial.println("Bitcoin going down!");
      }

      Serial.println("");
    }
  }

  if((((priceAsk + priceBid) / 2) > 0)&& (timeStamp > (prevTimeStamp + SAMPLE_TIME))){
    prevTimeStamp = timeStamp;

    for(int i = 11; i > 0; i--){
      prevPriceBTC[i] = prevPriceBTC[(i-1)];
    }
    prevPriceBTC[0] = ((priceAsk + priceBid) / 2);

    rangeHigh = priceHigh;
    rangeLow = priceLow;

    uint32_t prevLow = 999999999;
    uint32_t prevHigh = 0;
    
    for(int i = 0; i < 12; i++){
      if((prevPriceBTC[i] > prevHigh) && (prevPriceBTC[i] > 0)){
        prevHigh = prevPriceBTC[i];
      }
      if((prevPriceBTC[i] < prevLow)){// && (prevPriceBTC[i] > 0)
        prevLow = prevPriceBTC[i];
      }
    }

    rangeLow = prevLow;
    rangeHigh = prevHigh;

    Serial.print("Current range High: ");
    Serial.println(rangeHigh);
    Serial.print("Current range Low: ");
    Serial.println(rangeLow);
    
    if((rangeHigh != rangeLow) ){
      for(int i = 0; i<12; i++){
        for(int ii = 0; ii < 8; ii++){
          frame[ii][i] = 0;
        }
      }
      for(int i = 0; i<12; i++){
        if(prevPriceBTC[i] == 0){
          frame[0][i] = 1;
        }
        else{
          int index = map(prevPriceBTC[i], rangeLow, rangeHigh, 0, 7);
          if((i > 0) && (CANDLE_STYLE == 2)){
            int starti = map(prevPriceBTC[(i-1)], rangeLow, rangeHigh, 0, 7);
            if (starti > index){
              for(int ii = index; ii <= starti; ii++){
                frame[ii][i] = 1;
              }
            }
            else{
              for(int ii = starti; ii <= index;ii++){
                frame[ii][i] = 1;
              }
            }
          }
          else if((i>0) && (CANDLE_STYLE == 3)){
            int starti = map(prevPriceBTC[(i-1)], rangeLow, rangeHigh, 0, 7);
            frame[starti][i] = 1;
            frame[index][i] = 1;
          }
          else if((i > 0) && (CANDLE_STYLE == 1)){
            int starti = map(prevPriceBTC[(i-1)], rangeLow, rangeHigh, 0, 7);
            frame[starti][i] = 1;
          }
          else{
            frame[index][i] = 1;
          }
        }
      }

      matrix.renderBitmap(frame, 8, 12);
    }
  }  
}

void loop(){
  read_response();

  if(!client.connected()){
    client.stop();
    delay(5000);

    if(client.connect(server, port)){
      client.println("GET /api/v2/ticker/btcusdt/ HTTP/1.1");
      client.print("Host: ");
      client.println(server);
      client.println("Connection: close");
      client.println();
    }
  }

}

void printWifiStatus(){
  // Show the connect SSID
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Show boards local IP
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // SHow signal strength
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
