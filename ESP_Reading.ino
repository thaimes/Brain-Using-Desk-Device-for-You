#define LED_PIN 2  // built-in LED on most ESP32 Dev boards

void setup() {

  Serial.begin(115200);  // USB to computer
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX2=16, TX2=17
  Serial.println("DEV READY: listening for CAM...");
}

void loop() {
 if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();
    Serial.print("FROM CAM X: ");
    Serial.println(msg);
    
    int x = msg.toInt();   // convert to integer
    // you can now use x position for control logic
}
}

