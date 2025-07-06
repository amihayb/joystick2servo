#include <Servo.h>
#include <AccelStepper.h>
#include <TimerOne.h>

#define MAXSPEED 500
#define ACCELERATION 1000000

Servo myservo;  // create servo object to control a servo
// twelve servo objects can be created on most boards

const int maxValues = 17;
float parsedValues[maxValues];

// const int MAXSPEED = 50000;
// const int ACCELERATION = 1000000;

// Stepper pins for 6 steppers
const int STEP_PIN[6] = {22, 24, 26, 28, 30, 32};
const int DIR_PIN[6]  = {23, 25, 27, 29, 31, 33};
const int ENABLE_PIN[6] = {34, 35, 36, 37, 38, 39};

// Create AccelStepper objects as an array
AccelStepper stepper[6] = {
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[0], DIR_PIN[0]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[1], DIR_PIN[1]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[2], DIR_PIN[2]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[3], DIR_PIN[3]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[4], DIR_PIN[4]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[5], DIR_PIN[5])
};

float lastCommand[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
long target[6] = {1000000000, 1000000000, 1000000000, 1000000000, 1000000000, 1000000000};

void setup() {
  // pinMode(LED_BUILTIN, OUTPUT);
  // digitalWrite(LED_BUILTIN, LOW);
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  Serial.begin(115200); // initialize serial communication

  myservo.write(0); 

  // Configure steppers
  for (int i = 0; i < 6; i++) {
    // stepper[i].setEnablePin(ENABLE_PIN[i]);
    stepper[i].enableOutputs();
    stepper[i].setAcceleration(ACCELERATION);
    stepper[i].setMaxSpeed(MAXSPEED);
    pinMode(ENABLE_PIN[i], OUTPUT);
  }

  Timer1.initialize(2000);              // 1000 us = 1 ms = 1 kHz
  Timer1.attachInterrupt(stepperISR);   // attach ISR
}

// Reads a line from Serial until '\n' is received, returns the line (or empty string if not complete)
String readSerialLine() {
  static String inputLine = "";
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      String result = inputLine;
      inputLine = "";
      return result;
    } else {
      inputLine += c;
    }
  }
  return "";
}

void loop() {

  static unsigned long lastRun = 0;
  unsigned long now = millis();
  if (now - lastRun >= 50) { 
    lastRun = now;
    String line = readSerialLine();
    if (line.length() > 0) {
      parseLine(line);
    }
    String outLine = String(parsedValues[0], 2) + "," + String(parsedValues[1], 2) + "," + String(parsedValues[2], 2) + "," + String(parsedValues[3], 2) + "," + String(parsedValues[4], 2) + "," + String(parsedValues[5], 2);
    Serial.println(outLine);
    setServoFromFloat(parsedValues[1]);
    for (int i = 0; i < 6; i++) {
      runStepperCommand(stepper[i], parsedValues[i], lastCommand[i], target[i], ENABLE_PIN[i]);
    }
    // for (int i = 0; i < 3; i++) {
    //   runStepperCommand(stepper[i+3], parsedValues[i], lastCommand[i], target[i], ENABLE_PIN[i+3]);
    // }
    // for (int i = 3; i < 6; i++) {
    //   runStepperCommand(stepper[i-3], parsedValues[i], lastCommand[i], target[i], ENABLE_PIN[i-3]);
    // }
  }

  // stepper1.runSpeed();
  //stepper2.runSpeed();

  //delay(50); // small delay to avoid busy loop
}

void stepperISR() {
  for (int i = 0; i < 6; i++) {
    stepper[i].run();
  }
}

void parseLine(String line) {
  // Clean the prefix and suffix if present: b'...'
  line.trim();  // remove whitespace including '\r'
  if (line.startsWith("b'")) {
    line = line.substring(2, line.length() - 1); // remove b' and ending '
  }

  int index = 0;
  int start = 0;

  for (int i = 0; i < line.length(); i++) {
    if (line.charAt(i) == ',' || i == line.length() - 1) {
      String valueStr;
      if (i == line.length() - 1)
        valueStr = line.substring(start); // last value
      else
        valueStr = line.substring(start, i);
        
      parsedValues[index] = valueStr.toFloat();
      index++;
      start = i + 1;

      if (index >= maxValues) break;  // avoid overflow
    }
  }
  // Serial.println(parsedValues[1], 2);
}

void setServoFromFloat(float value) {
  // Clamp value to [-1, 1]
  if (value > 1.0) value = 1.0;
  if (value < -1.0) value = -1.0;

  // Map from [-1, 1] to [0, 180]
  int angle = (int)((value + 1.0) * 90.0);  // (value + 1) in [0, 2] → *90 → [0, 180]

  myservo.write(angle);
}

void runStepperCommand(AccelStepper &stepper, float command, float &lastCommand, long &target, int enablePin) {
  command = command * 0.3 + lastCommand * 0.7; // Simple low-pass filter
  lastCommand = command;

  if (abs(command) < 0.05) {
    stepper.stop();                 // Stop motor
    digitalWrite(enablePin, LOW);   // Disable stepper driver
    return;
  } else {
    digitalWrite(enablePin, HIGH);  // Enable stepper driver
  }

  if (sign(command) != sign(target)) {
    target = abs(target) * sign(command); // Change direction
  }

  if (stepper.distanceToGo() == 0) {
    // If the stepper is not moving, set the target position
    stepper.moveTo(target);
  }

  stepper.setMaxSpeed(abs(command * MAXSPEED));
}

int sign(float x) {
  if (x >= 0) return 1;
  else return -1;
}