#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include "config.h"

double lastPrice = -1;
int currentID = 0;

void setup() {
  delay(3000);
  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  delay(500);
  Serial.println(ip);
  getLatestBTCPrice();
}

void getLatestBTCPrice() {
  WiFiClientSecure client;
  HTTPClient http; 
  client.setInsecure();
  
  int count = 0;
  int httpCode = 0;
  while(count < 5 && httpCode != HTTP_CODE_OK){
    http.begin(client, endpointLatest);
    httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);

      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      currentID = doc["id"]; 
      String priceStr = doc["price"].as<String>(); 
      priceStr.trim();

      char priceCharArray[20];
      priceStr.toCharArray(priceCharArray, 20);
      lastPrice = atof(priceCharArray); 

      Serial.print("ID: ");
      Serial.println(currentID);
      Serial.print("Price: ");
      Serial.println(lastPrice, 2);
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    count++;
  }

  http.end();
}

double getBTCPrice() {
  WiFiClientSecure client;
  client.setInsecure();
  int retryCount = 0;
  if (!client.connect("rest.coinapi.io", 443) && retryCount < 5) {
    Serial.println("Connection to rest.coinapi.io failed");
    delay(2000); // Wait for 2 seconds before retrying
    retryCount++;
    if (retryCount > 5){
      return -1;
    }
  }
 
  client.println("GET /v1/exchangerate/BTC/USD?apikey=" + String(key) + " HTTP/1.1");
  client.println("Host: rest.coinapi.io");
  client.println("Connection: close");
  client.println();

  while (!client.available()) {
    delay(100);
  }

  boolean jsonStartFound = false;
  String response = "";
  char c;
  while (client.available()) {
    c = client.read();
    if (!jsonStartFound) {
      if (c == '{') { 
        jsonStartFound = true;
        response += c;
      }
    } else {
      response += c;
      if (c == '}') { 
        break; 
      }
    }
  }

  client.stop();

  if (!jsonStartFound) {
    Serial.println("Failed to find the JSON start in response.");
  }

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, response);
  float rate = doc["rate"];

  return rate;
}

void postBTCPrice(int id, double price) {
    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();  
  
    String priceStr = String(price, 2);

    DynamicJsonDocument doc(1024);
    doc["id"] = id;
    doc["price"] = priceStr;  

    String payload;
    serializeJson(doc, payload);  

    http.begin(client, endpointPOST);
    http.addHeader("Content-Type", "application/json");

    Serial.print("Posting data to ");
    Serial.println(endpointPOST);

    int httpCode = http.POST(payload); 

    if (httpCode > 0) {
        String response = http.getString();  
        Serial.println("Received response:");
        Serial.println(response);
    } else {
        Serial.print("Failed to send POST request. Error: ");
        Serial.println(http.errorToString(httpCode));  
    }

    http.end();
}

void loop() {
  double currentPrice = getBTCPrice();
  Serial.print("Current BTC Price: ");
  Serial.println(String(currentPrice, 6));

  if(currentPrice < lastPrice){
    Serial.println("Downwards trend");
  }else if(currentPrice > lastPrice){
    Serial.println("Upwards trend");
  }else{
    Serial.println("Same as previous");
  }

  postBTCPrice(++currentID, currentPrice);
  lastPrice = currentPrice;

  delay(1000 * 60 * 5); 
}
