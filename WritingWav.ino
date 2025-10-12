#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include "driver/i2s_std.h"

// =================== CONFIG ===================
#define I2S_LR  LOW
#define I2S_WS  25
#define I2S_SD  32
#define I2S_SCK 13
#define SD_CS   15
#define TRASH   2

#define AUDIO_FILE        "/recording.wav"
#define SAMPLE_RATE       16000
#define BITS_PER_SAMPLE   16
#define GAIN_BOOSTER_I2S  32

const char* ssid = "IgLights";
const char* password = "PolarBow";
const char* serverURL = "http://192.168.0.170:5000/upload";

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

  if(!SD.begin(SD_CS)) {
    Serial.println("SD mount failed!");
    return false;
  }
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

  DebugPrintln("> Recording finished. Bytes: "+String(*audiolength_bytes)+", sec: "+String(*audiolength_sec));
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


// =================== SETUP ===================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TRASH, OUTPUT);
  digitalWrite(TRASH, LOW);
  WiFi.begin(ssid,password);
  Serial.println("Connecting to Wi-Fi...");
  while(WiFi.status()!=WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected: "+String(WiFi.localIP()));

  if(!I2S_Recording_Init()) {
    Serial.println("I2S initialization failed!");
    while(true);
  }
}

void loop() {
  while(true) {
    // --- 5-second recording ---
    unsigned long startMillis = millis();
    while(millis() - startMillis < 5000) {
      Recording_Loop();
    }

    // --- stop & finalize WAV ---
    String filename;
    long bytes;
    float secs;
    Recording_Stop(&filename, &bytes, &secs);

    // --- send file and get recognized text ---
    String recognized = Send_WAV(filename); // modify Send_WAV to return server response as String

    recognized.toLowerCase(); // lowercase for easier comparison
    //digitalWrite(TRASH, LOW);
    // --- check for keywords ---
    if(recognized.indexOf("weather") >= 0 ||
       recognized.indexOf("time") >= 0 ||
       recognized.indexOf("calendar") >= 0) {

      digitalWrite(TRASH, LOW);
      Serial.println("Keyword detected: " + recognized);
      Serial.println("Pausing 10 seconds before next recording...");
      delay(10000);  // pause 10 seconds
    } else if (recognized.indexOf("trash") >= 0) {
      Serial.println("Gathering TRASH");
      digitalWrite(TRASH, HIGH);
      delay(10000);
      digitalWrite(TRASH, LOW);
    } else {
      digitalWrite(TRASH, LOW);
      Serial.println("No keyword detected. Continuing...\n");
    }
  }
}




