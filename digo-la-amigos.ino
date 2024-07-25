// blinkwithoutdelay di example 02.digital
#include <Arduino.h>
#include <ESP32_Supabase.h>
#include "HX711.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

// json
#include <ArduinoJson.h>

// get internet time
#include <WiFiUdp.h>
#include "NTP.h"

// servo
#include <ESP32Servo.h>

// led
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTP ntp(ntpUDP);

JsonDocument doc;

Supabase db;

Servo foodServo;
HX711 scale;

// Put your supabase URL and Anon key here...
// Because Login already implemented, there's no need to use secretrole key
String supabase_url = "https://aluowkxhbjklwfuqnxcc.supabase.co";
String anon_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImFsdW93a3hoYmprbHdmdXFueGNjIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MjA5NzI1MzMsImV4cCI6MjAzNjU0ODUzM30.riqECUMyUCZsokphF-vjJUOIxL8uqguRklypLQU5RBY";

// id device
String DEVICE_ID = "e1f1cd94-33e6-4d51-8c0b-c44404f474c2";

// put your WiFi credentials (SSID and Password) here
const char *ssid = "ipongnya davina";
const char *psswd = "doremonishere";

// Put Supabase account credentials here
const String email = "";
const String password = "";

// Pins
const int BUTTON = 0;
const int SPEAKER = 12;
const int SERVO = 13;
const int LED = 14;
const int DISTANCE_SENSOR = 15;
const int WEIGHT_SENSOR_DOUT = 33;
const int WEIGHT_SENSOR_SCK = 32;

// Session time variables
unsigned long sessionPrevMillis = 60000;
const long sessionInterval = 60000;
unsigned long comePrevMillis = 0;
const long comeInterval = 600000;
const long dispensingWaitTime = 5000;
unsigned long feedPrevMillis = 0;
const long feedInterval = 300000;

// Session variables
bool isRinging = false;
bool isFeeding = false;
int portion = 0;
int deviceRound = 0;
int curRound = 1;
String historyID = "";

// Device variables
String deviceLabel = "";
bool deviceIsEnabled = false;
float deviceLightRed = 0;
float deviceLightGreen = 0;
float deviceLightBlue = 0;
int deviceVolume = 0;
bool deviceIsTakingPhoto = false;

// Servo variables
int servoPos = 0;

// Weight variables
const float CALIBRATION_FACTOR = 696.97;
const int WEIGHT_MINIMUM_TIME = 5;
int weightedCount = 0;

// LED variables
#define NUMPIXELS 16
Adafruit_NeoPixel pixels(NUMPIXELS, LED, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500

// Supabase debugging variables
int code = 0;
DeserializationError error;

void setup() {
  Serial.begin(9600);

  // Connecting to Wi-Fi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, psswd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("Connected!");

  // Beginning Supabase Connection
  db.begin(supabase_url, anon_key);

  ntp.begin();

  pinMode(2, OUTPUT);
  pinMode(DISTANCE_SENSOR, INPUT);

  foodServo.attach(SERVO);
  foodServo.write(servoPos);

  // Weight sensor
  scale.begin(WEIGHT_SENSOR_DOUT, WEIGHT_SENSOR_SCK);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare();

  // Output
  noTone(SPEAKER);
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
  #endif
  pixels.begin();
  pixels.clear();
}

void insertActivity(String label) {
  StaticJsonDocument<200> activityDoc;
  ntp.update();
  activityDoc["time"] = ntp.formattedTime("%Y-%m-%dT%H:%M:%S+00:00");
  activityDoc["label"] = label;
  activityDoc["history_id"] = historyID;
  activityDoc["device_id"] = DEVICE_ID;

  String json;
  serializeJson(activityDoc, json);

  code = db.insert("activity", json, false);
  char buffer[50];
  sprintf(buffer, " Insert Activity: %d\n", code);
  Serial.println(label + String(buffer));
  db.urlQuery_reset();
}

void updateHistoryStatus(String status) {
  StaticJsonDocument<200> historyDoc;
  historyDoc["status"] = status;

  String json;
  serializeJson(historyDoc, json);

  code = db.update("history").eq("id", historyID).doUpdate(json);
  Serial.println(code);
  db.urlQuery_reset();
}

void startRinging() {
  isRinging = true;
  tone(SPEAKER, 200);
  for (int i = 0; i < NUMPIXELS; i++){
    int red = round(deviceLightRed * 255);
    int green = round(deviceLightGreen * 255);
    int blue = round(deviceLightBlue * 255);
    pixels.setPixelColor(i, pixels.Color(red, green, blue));
    pixels.show();
  }
}

void stopRinging() {
  isRinging = false;
  noTone(SPEAKER);
  for (int i = 0; i < NUMPIXELS; i++){
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    pixels.show();
  }
  pixels.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    stopRinging();
    isFeeding = false;
    Serial.println("Wi-fi Disconnected");
  } else if (isRinging) {
    // detect ada pergerakan dengan infrared distance sensor
    int state = digitalRead(DISTANCE_SENSOR);

    if (state == HIGH) {
      stopRinging();

      // Post activity dog come to device
      insertActivity("Your dog has come to " + deviceLabel);

      servoPos = (servoPos == 0 ? 180 : 0);
      Serial.printf("Servo Pos: %d", servoPos);
      foodServo.write(servoPos);
      
      isFeeding = true;
      feedPrevMillis = currentMillis;
    } else if (currentMillis - comePrevMillis >= comeInterval) {
      // Post activity dog didn't come to device
      insertActivity("Your dog didn't come to " + deviceLabel);
      updateHistoryStatus("Unfinished");

      stopRinging();
    }
  } else if (isFeeding) {
    if (currentMillis - feedPrevMillis >= dispensingWaitTime) {
      if (scale.is_ready()) {
        float weight = scale.get_units(10);
        Serial.printf("Weight: %f\n", weight);

        // detect berat timbangan == 0 == habis
        if (weight > -2 && weight < 2) {
          if (++weightedCount > WEIGHT_MINIMUM_TIME) {
            // Post Activity Anjing menghabiskan makanan di device
            insertActivity("Your dog finished the food at " + deviceLabel);

            Serial.printf("%d / %d\n", curRound, deviceRound);
            if (++curRound <= deviceRound) {
              startRinging();
              comePrevMillis = currentMillis;
            } else {
              // Ganti status history ke Finished
              updateHistoryStatus("Finished");
              stopRinging();
            }

            isFeeding = false;
            weightedCount = 0;
          }
        } else if (currentMillis - feedPrevMillis >= feedInterval) {
          // Post Activity Anjing ga habisin makanan di device
          insertActivity("Your dog didn't finish the food at " + deviceLabel);
          updateHistoryStatus("Unfinished");

          isFeeding = false;
        }
      }
    }
  } else if (currentMillis - sessionPrevMillis >= sessionInterval) {
    // get device information
    String deviceRead = db.from("device").select("*").eq("id", DEVICE_ID).doSelect();
    Serial.println(deviceRead);
    db.urlQuery_reset();

    StaticJsonDocument<200> deviceDoc;
    error = deserializeJson(deviceDoc, deviceRead);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    if (deviceDoc.isNull() || deviceDoc.size() == 0) {
      Serial.println("Device is not registered");
      return;
    }

    deviceLabel = deviceDoc[0]["label"].as<String>();
    deviceIsEnabled = deviceDoc[0]["is_enabled"];
    deviceLightRed = deviceDoc[0]["light_red"];
    deviceLightGreen = deviceDoc[0]["light_green"];
    deviceLightBlue = deviceDoc[0]["light_blue"];
    deviceVolume = deviceDoc[0]["volume"];

    sessionPrevMillis = currentMillis;

    // Query Session
    String sessionRead = db.from("session").select("*").eq("is_deleted", "FALSE").order("time", "asc", true).doSelect();
    Serial.println(sessionRead);
    db.urlQuery_reset();

    error = deserializeJson(doc, sessionRead);

    // Test if parsing succeeds
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    ntp.update();
    int currentHour = ntp.hours();
    int currentMinute = ntp.minutes();

    for (JsonObject sessionObj : doc.as<JsonArray>()) {
      Serial.println(sessionObj);
      const char *timeStr = sessionObj["time"];
      Serial.println(timeStr);

      // Parse the time string "2024-07-18T03:45:45+00:00"
      int year, month, day, hour, minute, second;
      sscanf(timeStr, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

      // Compare with current time
      Serial.printf("%d:%d | %d:%d\n", hour, minute, currentHour, currentMinute);
      if (currentHour == hour && currentMinute == minute) {
        portion = sessionObj["portion"];
        deviceRound = sessionObj["round"];
        curRound = 1;

        // insert history
        StaticJsonDocument<200> historyDoc;
        historyDoc["date"] = String(year) + "-" + String(month) + "-" + String(day);
        historyDoc["status"] = "Ongoing";
        historyDoc["session_id"] = sessionObj["id"];

        String json;
        serializeJson(historyDoc, json);

        code = db.insert("history", json, false);
        Serial.printf("Insert History: %d", code);
        db.urlQuery_reset();

        // get history ID
        String historyRead = db.from("history").select("id").order("created_at", "desc", true).limit(1).doSelect();
        Serial.println(historyRead);
        db.urlQuery_reset();

        error = deserializeJson(historyDoc, historyRead);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

        JsonArray array = historyDoc.as<JsonArray>();
        if (!array.isNull() && array.size() > 0) {
          historyID = array[0]["id"].as<String>();
        } else {
          Serial.println("No valid history found.");
        }

        // insert start session activity
        insertActivity("Session started");

        startRinging();
        comePrevMillis = currentMillis;
        break;
      }
    }
  }
}