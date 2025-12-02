const int irPin1 = 35;  // Front
const int irPin2 = 39;  // IR Left
const int irPin3 = 34;  // IR Right

int irThreshold1 = 700;
int irThreshold2 = 700;
int irThreshold3 = 700;

bool sensor1Detected = false;
bool sensor2Detected = false;
bool sensor3Detected = false;

void setupDistance() {
  pinMode(irPin1, INPUT);
  pinMode(irPin2, INPUT);
  pinMode(irPin3, INPUT);
  Serial.println("IR sensors initialized.");
}

// === Update Sensor States ===
void updateDistanceSensors() {
  int val1 = analogRead(irPin1);
  int val2 = analogRead(irPin2);
  int val3 = analogRead(irPin3);

  // Update detection flags
  sensor1Detected = (val1 < irThreshold1);  // Object
  sensor2Detected = (val2 < irThreshold2);  // Left edge
  sensor3Detected = (val3 < irThreshold3);  // Right edge

  // For debugging
  Serial.print("IR1: "); Serial.print(val1);
  Serial.print(" | IR2: "); Serial.print(val2);
  Serial.print(" | IR3: "); Serial.println(val3);
}

// Front Object Detection
bool objectDetected() {
  return sensor3Detected;
}

//Edge Detection
bool edgeDetected() {
  return (!sensor2Detected && !sensor1Detected);
}

//Any Edge Triggered (Debug)
bool anyEdgeDetected() {
  return (!sensor2Detected || !sensor1Detected);
}