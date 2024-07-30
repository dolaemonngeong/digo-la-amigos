// blinkwithoutdelay di example 02.digital
#include <Arduino.h>
#include <ESPSupabase.h>
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
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <SPIFFS.h>
#include <FS.h>
#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
#define CAMERA_MODEL_ESP32_CAM_BOARD
// #define PWDN_GPIO_NUM     32
// #define RESET_GPIO_NUM    -1
// #define XCLK_GPIO_NUM      0
// #define SIOD_GPIO_NUM     26
// #define SIOC_GPIO_NUM     27
// #define Y9_GPIO_NUM       35
// #define Y8_GPIO_NUM       34
// #define Y7_GPIO_NUM       39
// #define Y6_GPIO_NUM       36
// #define Y5_GPIO_NUM       21
// #define Y4_GPIO_NUM       19
// #define Y3_GPIO_NUM       18
// #define Y2_GPIO_NUM        5
// #define VSYNC_GPIO_NUM    25
// #define HREF_GPIO_NUM     23
// #define PCLK_GPIO_NUM     22

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 21
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 19
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 5
#define Y2_GPIO_NUM 4
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

camera_config_t config;


// #include "camera_pins.h"
// using eloq::camera;
// bool takeNewPhoto = false;
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
String DEVICE_ID = "e1f1cd94-33e6-4d51-8c0b-c44404f474c2";
String OTHER_DEVICE_ID = "a61454a3-52b5-4389-bd83-ed21c6aa930c";

// put your WiFi credentials (SSID and Password) here
const char *ssid = "ipongnya davina";
const char *psswd = "doremonishere";

// Put Supabase account credentials here
const String serviceEmail = "sidiganteng@gmail.com";
const String servicePassword = "12345678";

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

  int loginResponse = db.login_email(serviceEmail, servicePassword);
  if (loginResponse != 200)
  {
    Serial.printf("Login failed with code: %d.\n\r", loginResponse);
    return;
  } else {
    Serial.printf("Login Success!");
  }

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
// Print ESP32 Local IP Address
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  initCamera();

  // // delay(3000);
  // //   Serial.begin(115200);
  // Serial.println("___SAVE PIC TO SPIFFS___");

  //   // camera settings
  //   // replace with your own model!
  
  // camera.pinout.wroom_s3();
  // camera.brownout.disable();
  // camera.resolution.vga();
  // camera.quality.high();

  // // init camera
  // while (!camera.begin().isOk())
  //   Serial.println(camera.exception.toString());

  //   // init SPIFFS
  // while (!spiffs.begin().isOk())
  //   Serial.println(spiffs.exception.toString());

  // Serial.println("Camera OK");
  // Serial.println("SPIFFS OK");
  // Serial.println("Enter 'capture' to capture a new picture and save to SPIFFS");
}

void initCamera() {
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
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

void takenDevicePhoto() {
  StaticJsonDocument<200> devicePhotoDoc;
  devicePhotoDoc["is_taking_photo"] = false;

  String json;
  serializeJson(devicePhotoDoc, json);

  code = db.update("device").eq("id", DEVICE_ID).doUpdate(json);
  Serial.println(code);
  db.urlQuery_reset();
}

void assignTurn(String sessionID) {
  // Current device update
  StaticJsonDocument<200> device1Doc;
  device1Doc["current_session"] = sessionID;

  String json;
  serializeJson(device1Doc, json);

  code = db.update("device").eq("id", DEVICE_ID).doUpdate(json);
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
// void captureImage(){
//   if (!Serial.available())
//     return;

//   if (Serial.readStringUntil('\n') != "capture") {
//     Serial.println("I only understand 'capture' (without quotes)");
//     return;
//   }

//     // capture picture
//   if (!camera.capture().isOk()) {
//     Serial.println(camera.exception.toString());
//     return;
//   }

//     // save under root folder
//   if (spiffs.save(camera.frame).to("", "jpg").isOk()) {
//     Serial.print("File written to ");
//     String file = spiffs.session.lastFilename;
//     Serial.println(file);
//     saveImageToSupabase(file.toString());
//   }
//   else Serial.println(spiffs.session.exception.toString());

//     // restart the loop
//   Serial.println("Enter another filename");
// }

bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // pointer

  // Take a photo with the camera
  Serial.println("Taking a photo...");

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  saveImageToSupabase(DEVICE_ID + ".jpg", fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void saveImageToSupabase(String filename, uint8_t *buffer, size_t len){
  String bucket = "amidogs-cam";
  String mime_type = "image/jpg";

  int saveSupabase = db.uploadWithUpsert(bucket, filename, mime_type, buffer, len, true);
  Serial.println(saveSupabase);
}

void loop() {
  unsigned long currentMillis = millis();

  String deviceCaptureRead = db.from("device").select("is_taking_photo").eq("id", DEVICE_ID).limit(1).doSelect();
  // Serial.println(deviceCaptureRead);
  db.urlQuery_reset();

  StaticJsonDocument<200> deviceCaptureDoc;
  error = deserializeJson(deviceCaptureDoc, deviceCaptureRead);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  JsonArray array = deviceCaptureDoc.as<JsonArray>();
  if (!array.isNull() && array.size() > 0) {
    if (array[0]["is_taking_photo"].as<bool>()) {
      // Take photo and upload to storage
      capturePhotoSaveSpiffs();
      takenDevicePhoto();
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    stopRinging();
    isFeeding = false;
    Serial.println("Wi-fi Disconnected");
  } else if (isWaiting) {
    String deviceRead = db.from("device").select("current_session").eq("id", DEVICE_ID).limit(1).doSelect();
    Serial.println(deviceRead);
    db.urlQuery_reset();

    StaticJsonDocument<200> deviceDoc;
    error = deserializeJson(deviceDoc, deviceRead);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    JsonArray array = deviceDoc.as<JsonArray>();
    if (!array.isNull() && array.size() > 0) {
      if (array[0]["current_session"].as<String>() == curSessionID) {
        if (--deviceRound <= 0) {
          // check status if not Unfinished
          String sessionStatusRead = db.from("session").select("status").eq("id", curSessionID).limit(1).doSelect();
          Serial.println(sessionStatusRead);
          db.urlQuery_reset();

          StaticJsonDocument<200> sessionStatusDoc;
          error = deserializeJson(sessionStatusDoc, sessionStatusRead);
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
          }

          JsonArray sessionStatusArray = deviceDoc.as<JsonArray>();
          if (!sessionStatusArray.isNull() && sessionStatusArray.size() > 0) {
            if (array[0]["status"].as<String>() != "Unfinished") {
              finishSession();
            }
          } else {
            insertActivity("Session is stopped unexpectedly at " + deviceLabel);
            updateHistoryStatus("Unfinished");
          }
        } else {
          startRinging();
          comePrevMillis = currentMillis;
        }

        isWaiting = false;
      }
    } else {
      Serial.println("No valid device found.");
    }
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

            if (--deviceRound > 0) {
              // Update session current_device
              passSessionTurnToOther(curSessionID);
            } else {
              finishSession();
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
    String sessionRead = db.from("session").select("*").eq("is_deleted", "FALSE").eq("is_enabled", "TRUE").order("time", "asc", true).doSelect();
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
        curSessionID = sessionObj["id"].as<String>();

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

        assignTurn(curSessionID);
        startRinging();
        comePrevMillis = currentMillis;
        break;
      }
    }
  }
}