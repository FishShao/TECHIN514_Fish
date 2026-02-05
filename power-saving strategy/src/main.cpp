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

const int SLEEP_DURATION = 60;
const float CHANGE_THRESHOLD = 10.0;

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
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  Serial.println("\n=== Motion Detection System ===");
  
  prefs.begin("motion", false);
  float lastDistance = prefs.getFloat("lastDist", 0);
  int wakeCount = prefs.getInt("wakeCount", 0);
  wakeCount++;
  
  Serial.printf("Wake #%d, Last distance: %.2f cm\n", wakeCount, lastDistance);
  
  float currentDistance = readDistance();
  
  if (currentDistance < 0) {
    Serial.println("Sensor error! Going back to sleep...");
    prefs.putInt("wakeCount", wakeCount);
    prefs.end();
    
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION * 1000000ULL);
    esp_deep_sleep_start();
    return;
  }
  
  Serial.printf("Current distance: %.2f cm\n", currentDistance);
  
  float change = abs(currentDistance - lastDistance);
  Serial.printf("Change: %.2f cm\n", change);
  
  bool needSend = (change > CHANGE_THRESHOLD) || (lastDistance == 0);
  
  if (needSend) {
    Serial.println("⚠️ Significant change detected! Sending to Firebase...");
    
    Serial.print("Connecting WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");
      
      ssl_client.setInsecure();
      initializeApp(aClient, app, getAuth(noAuth), processData, "authTask");
      app.getApp<RealtimeDatabase>(Database);
      Database.url(DATABASE_URL);
      
      unsigned long fbStart = millis();
      while (!app.ready() && millis() - fbStart < 10000) {
        app.loop();
        delay(100);
      }
      
      if (app.ready()) {
        char path[100];
        sprintf(path, "/motion/detection_%d/distance", wakeCount);
        Database.set<float>(aClient, path, currentDistance, processData, "distTask");
        
        sprintf(path, "/motion/detection_%d/change", wakeCount);
        Database.set<float>(aClient, path, change, processData, "changeTask");
        
        sprintf(path, "/motion/detection_%d/timestamp", wakeCount);
        Database.set<int>(aClient, path, (int)(millis()/1000), processData, "timeTask");
        
        delay(2000);
        Serial.println("✓ Data sent!");
      } else {
        Serial.println("Firebase not ready");
      }
      
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    } else {
      Serial.println("\nWiFi failed!");
    }
    
    prefs.putFloat("lastDist", currentDistance);
  } else {
    Serial.println("✓ No significant change. Skipping WiFi...");
  }

  prefs.putInt("wakeCount", wakeCount);
  prefs.end();
  
  Serial.printf("Going to sleep for %d seconds...\n", SLEEP_DURATION);
  Serial.flush();
  
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {

}

void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;
  if (aResult.available()) {
    Serial.println("  ✓ OK");
  }
}