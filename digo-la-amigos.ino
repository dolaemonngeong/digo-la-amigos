// blinkwithoutdelay di example 02.digital
#include <Arduino.h>
#include <ESP32_Supabase.h>

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

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTP ntp(ntpUDP);

JsonDocument doc;

Supabase db;

Servo foodServo;

// Put your supabase URL and Anon key here...
// Because Login already implemented, there's no need to use secretrole key
String supabase_url = "https://aluowkxhbjklwfuqnxcc.supabase.co";
String anon_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImFsdW93a3hoYmprbHdmdXFueGNjIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MjA5NzI1MzMsImV4cCI6MjAzNjU0ODUzM30.riqECUMyUCZsokphF-vjJUOIxL8uqguRklypLQU5RBY";

// id device
String DEVICE_ID = "e1f1cd94-33e6-4d51-8c0b-c44404f474c2";

// put your WiFi credentials (SSID and Password) here
const char *ssid = "iPhonenya Sidi";
const char *psswd = "123456789";

// Put Supabase account credentials here
const String email = "";
const String password = "";

// Pins
const int BUTTON = 0;
const int SPEAKER = 12;
const int SERVO = 13;
const int LED = 14;
const int DISTANCE_SENSOR = 15;
const int WEIGHT_SENSOR1 = 32;
const int WEIGHT_SENSOR2 = 33;

// Session time variables
unsigned long sessionPrevMillis = 0;
const long sessionInterval = 60000;
unsigned long comePrevMillis = 0;
const long comeInterval = 600000;
unsigned long feedPrevMillis = 0;
const long feedInterval = 300000;

// Session variables
bool isRinging = false;
bool isFeeding = false;
int portion = 0;
int round = 0;
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
int curPos = 0;

// Supabase debugging variables
int code = 0;

void setup()
{
  Serial.begin(9600);

  // Connecting to Wi-Fi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, psswd);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }
  Serial.println("Connected!");

  // Beginning Supabase Connection
  db.begin(supabase_url, anon_key);

  ntp.begin();

  pinMode(2, OUTPUT);
  pinMode(DISTANCE_SENSOR, INPUT);

  foodServo.attach(SERVO, 1000, 2000);
}

void insertActivity(String label) {
  StaticJsonDocument<200> activityDoc;
  ntp.update()
  activityDoc["time"] = ntp.formattedTime("%Y-%m-%dT%H:%M:%S+00:00");
  activityDoc["label"] = label
  activityDoc["history_id"] = historyID;
  activityDoc["device_id"] = DEVICE_ID;

  serializeJson(activityDoc, json);

  code = db.insert("activity", json, false);
  Serial.printf("Insert Activity: %d", code);
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

void loop()
{
  unsigned long currentMillis = millis();

  if (isRinging) {
    // detect ada pergerakan dengan ultrasonic sensor
    int state = digitalRead(DISTANCE_SENSOR);
    Serial.println(state);

    if (state == LOW) {
      isRinging = false;
      Serial.printLn("Speaker Mati");
      digitalWrite(2, LOW); // Turn on LED

      // Post activity dog come to device
      insertActivity("Your dog has come to " + deviceLabel);

      foodServo.write(curPos == 0 ? 180 : 0); // Turn servo
      isFeeding = true;
      feedPrevMillis = currentMillis;
    } else if (currentMillis - comePrevMillis >= comeInterval) {
      // Post activity dog didn't come to device
      insertActivity("Your dog didn't come to " + deviceLabel);
      updateHistoryStatus("Unfinished");

      isRinging = false;
    }
  } else if (isFeeding) {
    // detect berat timbangan == 0 == habis
    if (1) {
      // Post Activity Anjing menghabiskan makanan di device
      insertActivity("Your dog finished the food at " + deviceLabel);

      if (++curRound <= round) {
        isRinging = true;
      } else {
        // Ganti status history ke Finished
        updateHistoryStatus("Finished");
      }

      isFeeding = false;
    } else if (currentMillis - feedPrevMillis >= feedInterval) {
      // Post Activity Anjing ga habisin makanan di device
      insertActivity("Your dog didn't finish the food at " + deviceLabel);
      updateHistoryStatus("Unfinished");

      isFeeding = false;
    }
  } else if (currentMillis - sessionPrevMillis >= sessionInterval) {
    sessionPrevMillis = currentMillis;

    // Query Session
    String read = db.from("session").select("*").eq("is_deleted", "FALSE").order("time", "asc", true).doSelect();
    Serial.println(read);
    db.urlQuery_reset();

    DeserializationError error = deserializeJson(doc, read);

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
      const char* timeStr = sessionObj["time"];
      Serial.println(timeStr);
      
      // Parse the time string "2024-07-18T03:45:45+00:00"
      int year, month, day, hour, minute, second;
      sscanf(timeStr, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

      // Compare with current time
      Serial.printf("%d:%d | %d:%d\n", hour, minute, currentHour, currentMinute);
      if (currentHour == hour && currentMinute == minute) {
        portion = sessionObj["portion"];
        round = sessionObj["round"];
        curRound = 1;
        isRinging = true;

        Serial.printLn("Speaker Bunyi")
        digitalWrite(2, HIGH); // Turn on LED

        comePrevMillis = currentMillis;
        
        // insert history
        StaticJsonDocument<200> historyDoc;
        historyDoc["date"] = String(year) + "-" + String(month) + "-" + String(date);
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

        DeserializationError error = deserializeJson(historyDoc, historyRead);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

        JsonArray array = historyDoc.as<JsonArray>();
        if (!array.isNull() && array.size() > 0) {
          historyID = array[0]["id"];
        } else {
          Serial.println("No valid history found.");
        }

        // insert start session activity
        insertActivity("Session started");

        // get device information
        String read = db.from("device").select("*").eq("id", DEVICE_ID).doSelect();
        Serial.println(read);
        db.urlQuery_reset();

        StaticJsonDocument<200> deviceDoc;
        DeserializationError error = deserializeJson(deviceDoc, read);

        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

        deviceLabel = deviceDoc[0]["label"];;
        deviceIsEnabled = deviceDoc[0]["is_enabled"];;
        deviceLightRed = deviceDoc[0]["light_red"];;
        deviceLightGreen = deviceDoc[0]["light_green"];;
        deviceLightBlue = deviceDoc[0]["light_blue"];;
        deviceVolume = deviceDoc[0]["volume"];
        break;
      }
    }
  } 
}