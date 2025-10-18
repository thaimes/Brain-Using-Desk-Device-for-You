#include <driver/i2s.h>

// ==== Pin mapping (ESP32 <-> INMP441) ====
// WS/LRCLK -> GPIO25
// BCK/SCK  -> GPIO26
// SD       -> GPIO33
// L/R      -> GND  (left channel)
// VDD      -> 3.3V
// GND      -> GND

// COM3 for programming COM5 for testing

#define I2S_PORT      I2S_NUM_0
#define PIN_I2S_WS    25
#define PIN_I2S_BCK   26
#define PIN_I2S_SD    33

#define SAMPLE_RATE   16000
#define BITS_PER_SAMP I2S_BITS_PER_SAMPLE_32BIT
#define BUFFER_LEN    256

// ==== Audio processing parameters ====
float GAIN = 1.5f;  
float HPF_COEFF = 0.998f; // High-pass filter strength
float LPF_COEFF = 0.5f;

// State for high-pass filter
float prev_sample = 0.0f;
float prev_filtered = 0.0f;
float prev_lpf = 0.0f;

void setup() {
  Serial.begin(921600);
  delay(1000);

  // I2S configuration
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = BITS_PER_SAMP,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_I2S_BCK,
    .ws_io_num = PIN_I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PIN_I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_set_clk(I2S_PORT, SAMPLE_RATE, BITS_PER_SAMP, I2S_CHANNEL_MONO);

  Serial.println("Streaming INMP441 audio...");
}

void loop() {
  int32_t buffer[BUFFER_LEN];
  size_t bytes_read = 0;

  i2s_read(I2S_PORT, (void*)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
  int count = bytes_read / sizeof(int32_t);

  for (int i = 0; i < count; i++) {
    // Convert 24-bit in 32-bit word to float
    float sample = (float)(buffer[i] >> 8);

    // ==== High-pass filter ====
    float filtered = HPF_COEFF * (prev_filtered + sample - prev_sample);
    prev_sample = sample;
    prev_filtered = filtered;

    // ==== Low-pass filter ====
    filtered = LPF_COEFF * filtered + (1.0f - LPF_COEFF) * prev_lpf;
    prev_lpf = filtered;

    // Apply gain
    filtered *= GAIN;

    // Clip to 16-bit
    if (filtered > 32767.0f) filtered = 32767.0f;
    if (filtered < -32768.0f) filtered = -32768.0f;

    int16_t out_sample = (int16_t)filtered;

    // Send as little-endian PCM
    Serial.write((uint8_t*)&out_sample, 2);
  }
}
