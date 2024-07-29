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

// camera
#include "esp_camera.h"
#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
#include "camera_pins.h"
bool takeNewPhoto = false;
// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"
void startCameraServer();

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
String DEVICE_ID = "a61454a3-52b5-4389-bd83-ed21c6aa930c";
String OTHER_DEVICE_ID = "e1f1cd94-33e6-4d51-8c0b-c44404f474c2";

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
unsigned long comePrevMillis = 0;
const long comeInterval = 600000;
const long dispensingWaitTime = 5000;
unsigned long feedPrevMillis = 0;
const long feedInterval = 300000;

// Session variables
String curSessionID = "";
bool isRinging = false;
bool isFeeding = false;
bool isWaiting = false;
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

  // Camera

  // delay(3000);
  //   Serial.begin(115200);
  Serial.println("___SAVE PIC TO SPIFFS___");

    // camera settings
    // replace with your own model!
  camera.pinout.wroom_s3();
  camera.brownout.disable();
  camera.resolution.vga();
  camera.quality.high();

  // init camera
  while (!camera.begin().isOk())
    Serial.println(camera.exception.toString());

    // init SPIFFS
  while (!spiffs.begin().isOk())
    Serial.println(spiffs.exception.toString());

  Serial.println("Camera OK");
  Serial.println("SPIFFS OK");
  Serial.println("Enter 'capture' to capture a new picture and save to SPIFFS");
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

void passSessionTurnToOther(String sessionID) {
  // Current device update
  StaticJsonDocument<200> device1Doc;
  device1Doc["current_session"] = " ";

  String json;
  serializeJson(device1Doc, json);

  code = db.update("device").eq("id", DEVICE_ID).doUpdate(json);
  Serial.println(code);
  db.urlQuery_reset();

  // Other device update
  StaticJsonDocument<200> device2Doc;
  device1Doc["current_session"] = sessionID;

  String json2;
  serializeJson(device2Doc, json);

  code = db.update("device").eq("id", OTHER_DEVICE_ID).doUpdate(json2);
  Serial.println(code);
  db.urlQuery_reset();

  isWaiting = true;
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

void finishSession() {
  updateHistoryStatus("Finished");
  stopRinging();
}

// capture image dari example
void captureImage(){
  if (!Serial.available())
    return;

  if (Serial.readStringUntil('\n') != "capture") {
    Serial.println("I only understand 'capture' (without quotes)");
    return;
  }

    // capture picture
  if (!camera.capture().isOk()) {
    Serial.println(camera.exception.toString());
    return;
  }

    // save under root folder
  if (spiffs.save(camera.frame).to("", "jpg").isOk()) {
    Serial.print("File written to ");
    String file = spiffs.session.lastFilename;
    Serial.println(file);
    saveImageToSupabase(file.toString());
  }
  else Serial.println(spiffs.session.exception.toString());

    // restart the loop
  Serial.println("Enter another filename");
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    saveImageToSupabase("picture.jpg", fb->buf, fb->len);
    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}

void saveImageToSupabase(String filename, uint8_t *buffer, size_t len){
  String bucket = "Amidogs";
  String mime_type = "image/jpg";
  // File file = SPIFFS.open(filename, FILE_WRITE);
  // camera_fb_t
  saveSupabase = db.upload(bucket, filename, mime_type, buffer, len);
  Serial.println(saveSupabase)
}

void loop() {
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    stopRinging();
    isFeeding = false;
    Serial.println("Wi-fi Disconnected");
  } else if (isWaiting) {
    String deviceRead = db.from("device").select("current_session").eq("id", DEVICE_ID).limit(1).doSelect();
    Serial.println(deviceRead);
    db.urlQuery_reset();

    error = deserializeJson(deviceDoc, deviceRead);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    JsonArray array = deviceDoc.as<JsonArray>();
    if (!array.isNull() && array.size() > 0) {
      curSessionID = array[0]["current_session"].as<String>()
      if (curSessionID != " ") {
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

        isWaiting = false;
        startRinging();
        comePrevMillis = currentMillis;
      }
    } else {
      Serial.println("No valid session found.");
    }
  } else if (isRinging) {
    // detect ada pergerakan dengan infrared distance sensor
    int state = digitalRead(DISTANCE_SENSOR);

    if (state == HIGH) {
      stopRinging();

      // Post activity dog come to device
      insertActivity("Your dog has come to " + deviceLabel);
      // captureImage();

      servoPos = (servoPos == 0 ? 180 : 0);
      Serial.printf("Servo Pos: %d", servoPos);
      foodServo.write(servoPos);
      
      isFeeding = true;
      feedPrevMillis = currentMillis;
    } else if (currentMillis - comePrevMillis >= comeInterval) {
      // Post activity dog didn't come to device
      insertActivity("Your dog didn't come to " + deviceLabel);
      updateHistoryStatus("Unfinished");
      passSessionTurnToOther(curSessionID);

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
            passSessionTurnToOther(curSessionID);

            isFeeding = false;
            weightedCount = 0;
          }
        } else if (currentMillis - feedPrevMillis >= feedInterval) {
          // Post Activity Anjing ga habisin makanan di device
          insertActivity("Your dog didn't finish the food at " + deviceLabel);
          updateHistoryStatus("Unfinished");
          passSessionTurnToOther(curSessionID);

          isFeeding = false;
        }
      }
    }
  }
}