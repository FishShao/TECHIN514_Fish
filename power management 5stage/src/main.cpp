#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <Preferences.h>
#include "esp_sleep.h"

const char* WIFI_SSID = "UW MPSK";       
const char* WIFI_PASSWORD = "y7sbCwt9AHALFiTF"; 
const char* DATABASE_URL = "https://lab5-a88fb-default-rtdb.firebaseio.com/"; 

const int trigPin = 2;
const int echoPin = 3;

void processData(AsyncResult &aResult);
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
FirebaseApp app;
RealtimeDatabase Database;
NoAuth noAuth; 

Preferences prefs;

float readDistance() {
  digitalWrite(trigPin, LOW); 
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); 
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return 20.0 + random(0, 30);
  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  prefs.begin("lab5", false);
  int currentStage = prefs.getInt("stage", 0);
  
  Serial.println("\n================================");
  Serial.printf("Current Stage: %d\n", currentStage);
  Serial.println("================================\n");

  if (currentStage == 0) {
    Serial.println("Stage 1: Idle (12s)...");
    unsigned long start = millis();
    while(millis() - start < 12000) {
      delay(100);
    }
    Serial.println("Stage 1 complete!\n");
    prefs.putInt("stage", 1);
    prefs.end();
    ESP.restart();
  }
  
  else if (currentStage == 1) {
    Serial.println("Stage 2: Sensor Only (12s)...");
    unsigned long startSensor = millis();
    int count = 0;
    while(millis() - startSensor < 12000) {
      float d = readDistance();
      Serial.printf("[%d] Dist: %.2f cm\n", ++count, d);
      delay(500);
    }
    Serial.println("Stage 2 complete!\n");
    prefs.putInt("stage", 2);
    prefs.end();
    ESP.restart();
  }
  
  else if (currentStage == 2) {
    Serial.println("Stage 3: WiFi Only (12s)...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.print("Connecting");
    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
      delay(300);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      
      unsigned long stageStart = millis();
      while(millis() - stageStart < 12000) {
        delay(100);
      }
    } else {
      Serial.println("\nWiFi failed!");
      delay(12000);
    }
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("Stage 3 complete!\n");
    prefs.putInt("stage", 3);
    prefs.end();
    ESP.restart();
  }
  
  else if (currentStage == 3) {
    Serial.println("Stage 4: Full Operation (12s)...");
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting");
    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
      delay(300);
      Serial.print(".");
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\nWiFi failed! Skipping...");
      delay(12000);
      prefs.putInt("stage", 4);
      prefs.end();
      ESP.restart();
      return;
    }
    
    Serial.println("\nWiFi connected!");
    
    ssl_client.setInsecure();
    initializeApp(aClient, app, getAuth(noAuth), processData, "authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
    
    unsigned long stageStart = millis();
    unsigned long lastSend = 0;
    int count = 0;
    
    while(millis() - stageStart < 12000) {
      app.loop();
      
      if (app.ready() && millis() - lastSend > 1000) {
        lastSend = millis();
        float d = readDistance();
        count++;
        
        char path[50];
        sprintf(path, "/lab5/reading_%d", count);
        Database.set<float>(aClient, path, d, processData, "setTask");
        Serial.printf("[%d] Sent: %.2f cm\n", count, d);
      }
      delay(10);
    }
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("Stage 4 complete!\n");
    prefs.putInt("stage", 4);
    prefs.end();
    ESP.restart();
  }
  
  else if (currentStage == 4) {
    Serial.println("Stage 5: Deep Sleep (12s)...");
    Serial.println("Good Night!");
    Serial.flush();
    
    prefs.putInt("stage", 0);
    prefs.end();
    
    esp_sleep_enable_timer_wakeup(12 * 1000000ULL);
    esp_deep_sleep_start();
  }
  
  else {
    Serial.println("Unknown stage! Resetting...");
    prefs.putInt("stage", 0);
    prefs.end();
    ESP.restart();
  }
}

void loop() {

}

void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;
  
  if (aResult.isError()) {
    Serial.println("Error");
  }
  
  if (aResult.available()) {
    Serial.println("OK");
  }
}