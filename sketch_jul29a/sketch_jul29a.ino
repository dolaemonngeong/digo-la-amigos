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
String supabase_url = "https://orvtswcxgbxqrxgmvirc.supabase.co";
String anon_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9ydnRzd2N4Z2J4cXJ4Z212aXJjIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MjI0MTcwNjUsImV4cCI6MjAzNzk5MzA2NX0.LnOc-2kyNlnxPrAMAP8-Slr9O-gb3POyDs49BUhZMzQ";

// id device
String DEVICE_ID = "a61454a3-52b5-4389-bd83-ed21c6aa930c";
String OTHER_DEVICE_ID = "e1f1cd94-33e6-4d51-8c0b-c44404f474c2";

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
unsigned long comePrevMillis = 0;
const long comeInterval = 600000;
const long dispensingWaitTime = 5000;
unsigned long feedPrevMillis = 0;
const long feedInterval = 300000;
unsigned long photoPrevMillis = 0;
const long photoInterval = 1000;

// Session variables
String curSessionID = "";
bool isRinging = false;
bool isFeeding = false;
bool isWaiting = true;
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
  config.grab_mode = CAMERA_GRAB_LATEST;

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

  if (WiFi.status() != WL_CONNECTED) {
    stopRinging();
    isFeeding = false;
    Serial.println("Wi-fi Disconnected");
    return;
  }

  if (currentMillis - photoPrevMillis >= photoInterval) {
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
  }
  
  if (isWaiting) {
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
      curSessionID = array[0]["current_session"].as<String>();
      if (curSessionID != " ") {
        // get history ID
        String historyRead = db.from("history").select("id").order("created_at", "desc", true).limit(1).doSelect();
        Serial.println(historyRead);
        db.urlQuery_reset();

        StaticJsonDocument<200> historyDoc;
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