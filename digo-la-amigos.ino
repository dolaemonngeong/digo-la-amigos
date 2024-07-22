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
#include <NTPClient.h>
#include <WiFiUdp.h>

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String formattedDate;
String dayStamp;
String timeStamp;

JsonDocument doc;

Supabase db;

// Put your supabase URL and Anon key here...
// Because Login already implemented, there's no need to use secretrole key
String supabase_url = "https://aluowkxhbjklwfuqnxcc.supabase.co";
String anon_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImFsdW93a3hoYmprbHdmdXFueGNjIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MjA5NzI1MzMsImV4cCI6MjAzNjU0ODUzM30.riqECUMyUCZsokphF-vjJUOIxL8uqguRklypLQU5RBY";

// put your WiFi credentials (SSID and Password) here
const char *ssid = "iPhonenya Sidi";
const char *psswd = "123456789";

// Put Supabase account credentials here
const String email = "";
const String password = "";

// Session variables
unsigned long sessionPrevMillis = 0;
const long sessionInterval = 60000;

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

  timeClient.begin();
  timeClient.setTimeOffset(0);

  pinMode(2, OUTPUT);
}

void loop()
{
  unsigned long currentMillis = millis();

  if (currentMillis - sessionPrevMillis >= sessionInterval) {
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

    timeClient.update();
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();

    for (JsonObject obj : doc.as<JsonArray>()) {
      Serial.println(obj);
      const char* timeStr = obj["time"];
      Serial.println(timeStr);
      
      // Parse the time string "2024-07-18T03:45:45+00:00"
      int year, month, day, hour, minute, second;
      sscanf(timeStr, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

      // Compare with current time
      Serial.printf("%d:%d | %d:%d\n", hour, minute, currentHour, currentMinute);
      if (currentHour == hour && currentMinute == minute) {
        digitalWrite(2, HIGH); // Turn on LED
        Serial.println("Time same!");
        break;
      }
    }
  }
}