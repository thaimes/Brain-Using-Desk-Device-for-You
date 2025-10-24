#define LED_PIN 2
#include<motor.h>

bool trashWasPresent = false;
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  setupMotor();
  Serial.begin(115200);  // USB to PC (uses GPIO1/3 internally)
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // <-- RX2=16, TX2=17
  Serial.println("DEV READY: Listening...");
}
void loop() {
    if (Serial2.available()) {
        String msg = Serial2.readStringUntil('\n');
        msg.trim();
        Serial.print("FROM CAM: "); 
        Serial.println(msg);
        if (msg.startsWith("{\"trash\":")) {
            digitalWrite(LED_PIN, HIGH);
            Serial.println("TRASH APPEARED");
            spinServoForward();

            int start = msg.indexOf(":") + 1;
            int end = msg.indexOf("}");
            int x = msg.substring(start, end).toInt();

            if (x <= 24) {
                Serial.println("STOP MOTOR");
                stopMotors();
            } else {
                Serial.println("START MOTOR");
                moveForward();
            }
        }

        else{
            digitalWrite(LED_PIN, LOW);
            rotateMotors();
        }
    }
}
