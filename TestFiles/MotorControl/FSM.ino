#define LED_PIN 2
#include "motor.h"
#include "ir.h"

bool trashWasPresent = false;

enum mState {
    SEARCH,
    BACK,
    TRASH,
    STOP
};

mState currentState = SEARCH;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  setupMotor();
  setupIR();
  Serial.begin(115200);  // USB to PC (uses GPIO1/3 internally)
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // <-- RX2=16, TX2=17
  Serial.println("DEV READY: Listening...");
}

void loop() {
    switch (currentState) {
        case SEARCH:
        {
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
                        if (trashWasPresent) {
                            Serial.println("Trash Checked");
                            currentState = TRASH;
                        }
                        else {
                            trashWasPresent = true;
                            Serial.println("BACKUP");
                            moveBackward();
                            currentState = BACK;
                        }
                    } 
                    else {
                        Serial.println("START MOTORS");
                        moveForward();
                        delay(1000);
                        currentState = SEARCH;
                    }
                }
                else{
                    digitalWrite(LED_PIN, LOW);
                    rotateMotorsL();
                    delay(1000);
                    rotateMotorsR();
                    currentState = SEARCH;
                }
                break;
            }
        }
        case BACK:
        {
            delay(3000);
            currentState = SEARCH;
            break;
        }

        case TRASH:
        {
            //if (objectdetect)
            moveForward();
            Serial.println("MOVE TO TABLE");
            if(edge1sense &&  edge2sense){
                stopMotors();
                currentState = STOP; //change stop to return when time
            }

            currentState=TRASH;
            
            break;
        }

        case STOP:
        {
            currentState = STOP;
            break;
        }
    }
}
