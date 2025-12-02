#include <ESP32Servo.h>

//Motor + Servo Pins
const int ENA = 25;
const int IN1 = 26;
const int IN2 = 27;
const int ENB = 32;
const int IN3 = 33;
const int IN4 = 14;
const int servoPinTrash = 4;

// Servo Objects
Servo servoTrash;

//Setup Motors & Servos
void setupMotor() {
  pinMode(ENA, OUTPUT); 
  pinMode(IN1, OUTPUT); 
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); 
  pinMode(IN3, OUTPUT); 
  pinMode(IN4, OUTPUT);
  servoTrash.attach(servoPinTrash);
}

//DC Motors
void moveBackward() {
  digitalWrite(IN1, HIGH); 
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); 
  digitalWrite(IN4, LOW);
  digitalWrite(ENA, HIGH);
   digitalWrite(ENB, HIGH);
}
void moveForward() {

  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); 
  digitalWrite(IN4, HIGH);
  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);
}
void turnRight() {
  digitalWrite(IN1, HIGH); 
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); 
  digitalWrite(IN4, HIGH);
  digitalWrite(ENA, HIGH); 
  digitalWrite(ENB, HIGH);
}
void turnLeft() {
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); 
  digitalWrite(IN4, LOW);
  digitalWrite(ENA, HIGH); 
  digitalWrite(ENB, HIGH);
}
void stopMotors() {
  digitalWrite(ENA, LOW); 
  digitalWrite(ENB, LOW);
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); 
  digitalWrite(IN4, LOW);
}

void spinServoForward() {
    servoTrash.write(90);   // adjust angle as needed
}

void spinServoBackward() {
    servoTrash.write(0);    // return to home
}

void rotateMotorsR() {
    // turn in place while searching for trash
    digitalWrite(IN1, HIGH); 
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); 
    digitalWrite(IN4, HIGH);
    digitalWrite(ENA, HIGH);
    digitalWrite(ENB, HIGH); 
}

void rotateMotorsL() {
    // turn in place while searching for trash
    digitalWrite(IN1, LOW); 
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH); 
    digitalWrite(IN4, LOW);
    digitalWrite(ENA, HIGH);
    digitalWrite(ENB, HIGH);
    //delay(1000);
    
}