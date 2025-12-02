#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <JPEGDecoder.h>
#include "time.h"
#include <motor.h>
#include <IR.h>
#include "driver/i2s_std.h"


// =================== CONFIG ===================
#define I2S_LR  LOW
#define I2S_WS  25
#define I2S_SD  32
#define I2S_SCK 13
#define SD_CS   15
#define LED_PIN 2

#define AUDIO_FILE        "/recording1.wav"
#define SAMPLE_RATE       16000
#define BITS_PER_SAMPLE   16
#define GAIN_BOOSTER_I2S  32

const char* ssid ="iPhone (1)"; // <- hotspot info
const char* password = "*********";
const char* serverURL = "http://***********:5000/upload"; // <- hotspot info

WebServer server(80);
bool flag = false;
bool trashWasPresent = false;
int sweepCnt = 0;
int Lcnt = 0;
int Rcnt = 0;

unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

// OpenWeatherMap API
const char* city = "Lubbock";
const char* apiKey = "**********************";

// Time Setup
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600;
const int daylightOffset_sec = 3600;

// ======== Screen setup ========
TFT_eSPI tft = TFT_eSPI();
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

const char* FACE_IDLE       = "/idle.jpg";
const char* FACE_LISTENING  = "/listening.jpg";
const char* FACE_THINKING   = "/thinking.jpg";
const char* FACE_CORRECT    = "/right_ans.jpg";
const char* FACE_QUESTION   = "/question.jpg";
const char* FACE_QUESTION1  = "/question1.jpg";
const char* FACE_QUESTION2  = "/question2.jpg";
const char* FACE_TRASH      = "/trash.jpg";

// ======== Weather data ========
String weatherDesc = "";
float weatherTemp = 0;
String weatherMain = "";

// Debug
#ifndef DEBUG
#define DEBUG true
#define DebugPrint(x)     if(DEBUG) Serial.print(x)
#define DebugPrintln(x)   if(DEBUG) Serial.println(x)
#endif

// =================== I2S CONFIG ===================
i2s_std_config_t std_cfg = {
  .clk_cfg = {.sample_rate_hz = SAMPLE_RATE, .clk_src = I2S_CLK_SRC_DEFAULT, .mclk_multiple = I2S_MCLK_MULTIPLE_256},
  .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
  .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = (gpio_num_t) I2S_SCK, .ws = (gpio_num_t) I2S_WS,
               .dout = I2S_GPIO_UNUSED, .din = (gpio_num_t) I2S_SD,
               .invert_flags = {.mclk_inv=false, .bclk_inv=false, .ws_inv=false}}
};

i2s_chan_handle_t rx_handle;

// =================== WAV HEADER ===================
struct WAV_HEADER {
  char riff[4]          = {'R','I','F','F'};
  long flength          = 0;
  char wave[4]          = {'W','A','V','E'};
  char fmt[4]           = {'f','m','t',' '};
  long chunk_size       = 16;
  short format_tag      = 1;
  short num_chans       = 1;
  long srate            = SAMPLE_RATE;
  long bytes_per_sec    = SAMPLE_RATE * (BITS_PER_SAMPLE/8);
  short bytes_per_samp  = (BITS_PER_SAMPLE/8);
  short bits_per_samp   = BITS_PER_SAMPLE;
  char dat[4]           = {'d','a','t','a'};
  long dlength          = 0;
} myWAV_header;

// =================== FLAGS ===================
bool flg_is_recording = false;
bool flg_I2S_initialized = false;

// =================== FUNCTIONS ===================
bool I2S_Recording_Init() {
  if(I2S_LR==HIGH) std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
  else std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx_handle);
  i2s_channel_init_std_mode(rx_handle, &std_cfg);
  i2s_channel_enable(rx_handle);

  if(!SD.begin(SD_CS, tft.getSPIinstance())) {
    Serial.println("SD mount failed!");
    return false;
  }
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  flg_I2S_initialized = true;
  return true;
}

bool Recording_Loop() {
  if(!flg_I2S_initialized) return false;

  if(!flg_is_recording) {
    flg_is_recording = true;
    if(SD.exists(AUDIO_FILE)) SD.remove(AUDIO_FILE);
    File audio_file = SD.open(AUDIO_FILE, FILE_WRITE);
    audio_file.write((uint8_t*)&myWAV_header, 44);
    audio_file.close();
    DebugPrintln("> WAV header written, recording started...");
  }

  int16_t audio_buffer[1024];
  size_t bytes_read = 0;
  i2s_channel_read(rx_handle, audio_buffer, sizeof(audio_buffer), &bytes_read, portMAX_DELAY);
  size_t values_recorded = bytes_read / 2;

  for(size_t i=0;i<values_recorded;i++) audio_buffer[i]*=GAIN_BOOSTER_I2S;

  File audio_file = SD.open(AUDIO_FILE, FILE_APPEND);
  audio_file.write((uint8_t*)audio_buffer, bytes_read);
  audio_file.close();

  return true;
}

bool Recording_Stop(String* audio_filename, long* audiolength_bytes, float* audiolength_sec) {
  if(!flg_is_recording || !flg_I2S_initialized) return false;

  flg_is_recording = false;

  File audio_file = SD.open(AUDIO_FILE, "r+");
  long filesize = audio_file.size();
  audio_file.seek(0);
  myWAV_header.flength = filesize-8;
  myWAV_header.dlength = filesize-44;
  audio_file.write((uint8_t*)&myWAV_header,44);
  audio_file.close();

  *audio_filename = AUDIO_FILE;
  *audiolength_bytes = filesize;
  *audiolength_sec = (float)((filesize-44) / SAMPLE_RATE * BITS_PER_SAMPLE/8);

  DebugPrintln("> Recording finished. Bytes: "+String(*audiolength_bytes)+", sec: "+String(*audiolength_sec/4));
  return true;
}

String Send_WAV(const String& filename) {
  if(WiFi.status()!=WL_CONNECTED) return "";

  File audioFile = SD.open(filename);
  if(!audioFile) return "";

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type","audio/wav");

  int httpResponseCode = http.sendRequest("POST",&audioFile,audioFile.size());
  String serverResponse = "";

  if(httpResponseCode > 0) {
    serverResponse = http.getString();
    DebugPrintln("HTTP Response: " + String(httpResponseCode));
    DebugPrintln("Server says: " + serverResponse);
  } else {
    DebugPrintln("Error sending file: " + http.errorToString(httpResponseCode));
  }

  http.end();
  audioFile.close();
  return serverResponse;
}


// ================================================================
//                      TIME FUNCTIONS
// ================================================================
void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time Error";
  }
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%I:%M", &timeinfo);
  return String(buffer);
}

// ================================================================
//                      WEATHER FUNCTIONS
// ================================================================
void getWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    weatherDesc = "No WiFi";
    weatherTemp = 0;
    weatherMain = "";
    return;
  }

  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + String(city) +
               "&appid=" + String(apiKey) + "&units=imperial";

  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET();
  Serial.println("HTTP code: " + String(httpCode));

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("JSON Error: ");
      Serial.println(error.c_str());
      weatherDesc = "Parse Error";
      return;
    }

    weatherDesc = doc["weather"][0]["description"].as<String>();
    weatherMain = doc["weather"][0]["main"].as<String>();
    weatherTemp = doc["main"]["temp"].as<float>();
  } else {
    weatherDesc = "HTTP Error " + String(httpCode);
  }
  http.end();
}

// ================================================================
//                      JPEG RENDERING
// ================================================================
void drawSdJpeg(const char *filename, int xpos, int ypos) {
  File jpegFile = SD.open(filename, FILE_READ);
  if (!jpegFile) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println("\" not found!");
    return;
  }

  bool decoded = JpegDec.decodeSdFile(jpegFile);
  if (decoded) {
    jpegRender(xpos, ypos);
  } else {
    Serial.println("Jpeg file format not supported!");
  }
}

void jpegRender(int xpos, int ypos) {
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);

  uint32_t min_w = mcu_w;
  uint32_t min_h = mcu_h;

  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  max_x += xpos;
  max_y += ypos;

  while (JpegDec.read()) {
    pImg = JpegDec.pImage;
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;

    if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ((mcu_y + win_h) >= tft.height())
      JpegDec.abort();
  }

  tft.setSwapBytes(swapBytes);
}

// ================================================================
//                      DISPLAY SCREENS
// ================================================================
void drawTimeScreen() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(6);

  String now = getTimeString();
  tft.drawString(now, tft.width() / 2, tft.height() / 2);
}

const char* getIconForWeather(String mainWeather) {
  mainWeather.toLowerCase();
  if (mainWeather.indexOf("rain") >= 0) return "/rainy.jpg";
  if (mainWeather.indexOf("cloud") >= 0) return "/cloudy.jpg";
  if (mainWeather.indexOf("snow") >= 0) return "/snowy.jpg";
  if (mainWeather.indexOf("clear") >= 0) return "/sunny.jpg";
  if (mainWeather.indexOf("thunderstorm") >= 0) return "/thunderstorm.jpg";
  return "/sunny.jpg"; // default
}

void drawWeatherScreen() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);

  // Temperature
  tft.setTextSize(6);
  String tempStr = String((int)weatherTemp) + "F";
  int tempX = 110;
  int tempY = tft.height() / 2 - 20;
  tft.drawString(tempStr, tempX, tempY);

  // Description
  tft.setTextSize(3);
  tft.drawString(weatherMain, tft.width() / 2, tft.height() / 2 + 40);

  const char* iconFile = getIconForWeather(weatherMain);
  int iconW = 128;
  int iconH = 128;

  // ======== ICON LOCATION ========
  int iconX = (tft.width() - iconW - 8);   // horizontal centering
  int iconY = 12;                          // distance from top
  drawSdJpeg(getIconForWeather(weatherMain), iconX, iconY);
}

// Faces
void drawFaceScreen(const char* faceFile) {
  tft.fillScreen(TFT_WHITE);

  File jpegFile = SD.open(faceFile, FILE_READ);
  if (!jpegFile) {
    Serial.print("ERROR FACE NOT FOUND");
    Serial.println(faceFile);
    return;
  }

  if (JpegDec.decodeSdFile(jpegFile)) {
    int16_t imgW = JpegDec.width;
    int16_t imgH = JpegDec.height;

    int16_t xpos = (tft.width() - imgW) / 2;
    int16_t ypos = (tft.height() - imgH) / 2;

    jpegRender(xpos, ypos);
  } else {
    Serial.println("JPEG DECODE FAIL");
  }

  jpegFile.close();
}

void handleFlagOn() {
  flag = true;
  server.send(200, "text/plain", "Flag set ON");
  Serial.println("LEARNING MODE ON");
}

void handleFlagOff() {
  flag = false;
  server.send(200, "text/plain", "Flag set OFF");
  Serial.println("LEARNING MODE OFF");
}

// =================== STATES ==================
enum State {
  IDLE,
  LISTEN,
  WEATHER,
  TIME,
  CALENDAR,
  SEARCH,
  TRASH,
  EDGE,
  STOP,
  QUESTION
};

State currentState = IDLE;
int lastState = -1;


// =================== SETUP ===================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  //pinMode(IN4, OUTPUT);
  //digitalWrite(IN4, LOW);
  Serial.println("MOTORS STOPPED");
  setupDistance();
  setupMotor();
  stopMotors();
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX2 = 16, TX2 = 17
  Serial.println("DEV READY: Listening...");

  WiFi.begin(ssid,password);
  Serial.println("Connecting to Wi-Fi...");
  while(WiFi.status()!=WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.println(WiFi.localIP());

  server.on("/flag/on", handleFlagOn);
  server.on("/flag/off", handleFlagOff);
  server.begin();
  Serial.println("Server started");

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);
  //tft.setTextColor(TFT_BLACK, TFT_WHITE);

  if(!I2S_Recording_Init()) {
    Serial.println("I2S initialization failed!");
    while(true);
  }

  initTime();
  getWeather();
}

void loop() {
  server.handleClient();
  updateDistanceSensors();

  if (currentState != lastState) {
    tft.fillScreen(TFT_WHITE);

    switch (currentState) {
      case IDLE:
      {
        drawFaceScreen(FACE_IDLE);
        break;
      }
      case WEATHER:
      {
        drawWeatherScreen();
        break;
      }
      case TIME:
      {
        drawTimeScreen();
        break;
      }
      case SEARCH:
      {
        drawFaceScreen(FACE_TRASH);
        break;
      }
      case TRASH:
      {
        drawFaceScreen(FACE_TRASH);
        break;
      }
      case QUESTION:
      {
        drawFaceScreen(FACE_QUESTION);
        break;
      }
    }
    lastState = currentState;
  }
  switch (currentState) {
    case IDLE:
    {
      if (!flag) currentState = LISTEN;
      break;
    }
    case LISTEN: 
    {  
      if (flag) {
        currentState = IDLE;
        break;
      }

      sweepCnt = 0;
      Lcnt = 0;
      Rcnt = 0;
      drawFaceScreen(FACE_LISTENING);

      unsigned long startMillis = millis();
      while (millis() - startMillis < 3000) {
        Recording_Loop();
      }

      // Prevent corruption
      String filename;
      long bytes;
      float secs;
      Recording_Stop(&filename, &bytes, &secs);

      // Send for recognition
      if (bytes > 96300) {
        drawFaceScreen(FACE_THINKING);
        String recognized = Send_WAV(filename);
        recognized.toLowerCase();

        Serial.println("Recognized: " + recognized);

        // State stuff
        if      (recognized.indexOf("weather")  >= 0) currentState = WEATHER;
        else if (recognized.indexOf("time")     >= 0) currentState = TIME;
        else if (recognized.indexOf("calendar") >= 0) currentState = CALENDAR;
        else if (recognized.indexOf("trash")    >= 0) currentState = SEARCH;
        else if (recognized.indexOf("garbage")  >= 0) currentState = SEARCH;
        else if (recognized.indexOf("question") >= 0) currentState = QUESTION;
        else currentState = LISTEN;
      }
      break;
    }

    case WEATHER: 
    {
      Serial.println("Displaying weather...");
      getWeather();
      delay(5000);
      currentState = LISTEN;
      break;
    }

    case CALENDAR:
    {
      Serial.println("Displaying calendar...");
      delay(5000);
      currentState = LISTEN;
      break;
    }

    case TIME:
    {
      Serial.println("Displaying time...");
      delay(5000);
      currentState = LISTEN;
      break;
    }

    case SEARCH:
    {
      /* Add trash code in here 
         Extra movement states etc can be spread around
         Make sure that it can come out */
      if (Serial2.available()) {
        String msg = Serial2.readStringUntil('\n');
        msg.trim();
        Serial.print("FROM CAM: "); 
        Serial.println(msg);
        if (msg.startsWith("{\"trash\":")) {
            digitalWrite(LED_PIN, HIGH);
            Serial.println("TRASH APPEARED");
            //spinServoForward();

            int start = msg.indexOf(":") + 1;
            int end = msg.indexOf("}");
            int x = msg.substring(start, end).toInt();

            Serial.println("START MOTORS");
            currentState = TRASH;
        }
        else{
            digitalWrite(LED_PIN, LOW);
            // Skip both if Lcnt > 3 and Rcnt > 5
            // Turn left 3 times
            if (Lcnt < 4 && Rcnt == 0) {
              Serial.println("TURNING LEFT");
              rotateMotorsL();
              delay(250);
              stopMotors();
              Lcnt++; // Increment turn left count
              currentState = SEARCH;
            }
            // Turn right 5 times
            else if (Lcnt == 3 && Rcnt < 6) {
              Serial.println("TURNING RIGHT");
              rotateMotorsR();
              delay(250);
              stopMotors();
              Rcnt++; // Increment turn right count
              currentState = SEARCH;
            }
            // This might break here...
            // Increment sweep only once Lcnt = 3 and Rcnt = 5 meaning full sweep
            else if (Lcnt == 3 && Rcnt == 5) {
              Rcnt = 0;
              Lcnt = 0;
              sweepCnt++;
            }

            else if (sweepCnt == 5) {
              Rcnt = 0;
              Lcnt = 0;
              currentState = STOP;
            }
            else {
              // Reset position 
              rotateMotorsL();
              delay(500);
              stopMotors();
              currentState = SEARCH;
            }
        }
        break;
      }
    }

    case TRASH:
    {
      moveForward();
      
      Serial.println("GETTING TRASH...");
      // Go forward until object detected
      if (objectDetected()){
        currentState = EDGE;
      }
      // If no trash keep going forward
      else {
        currentState = TRASH;
      }
      break;
    }

    case EDGE:
    {
      moveForward();
      // If any edge detected stop
      if (anyEdgeDetected()){
        currentState = STOP;
      }
      else {
        currentState = EDGE;
      }
      break;
    }
    case STOP:
    {
      stopMotors();
      currentState = LISTEN;
      break;
    }

    case QUESTION:
    {
      unsigned long startMillis = millis();
      while (millis() - startMillis < 5000) {
        Recording_Loop();
      }

      // Prevent corruption
      String filename;
      long bytes;
      float secs;
      Recording_Stop(&filename, &bytes, &secs);
      drawFaceScreen(FACE_QUESTION1);

      // Send for Gemini
      String question = Send_WAV(filename);

      Serial.println("Question recieved, answering...");
      drawFaceScreen(FACE_QUESTION2);
      delay(5000);
      // Return to listening
      currentState = LISTEN;
      break;
    }
  }
}





