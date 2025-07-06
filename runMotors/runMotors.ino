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
  Serial.begin(115200); // Add this line to initialize Serial
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object

  myservo.write(0); 

  // Configure steppers
  for (int i = 0; i < 6; i++) {
    stepper[i].enableOutputs();
    stepper[i].setAcceleration(ACCELERATION);
    stepper[i].setMaxSpeed(MAXSPEED);
    pinMode(ENABLE_PIN[i], OUTPUT);
  }

  Timer1.initialize(2000);              // 1000 us = 1 ms = 1 kHz
  Timer1.attachInterrupt(stepperISR);   // attach ISR
}

void loop() {
  static unsigned long lastRun = 0;
  static int motorIdx = 0;
  static int phase = 0; // 0: +1, 1: -1, 2: 0
  static unsigned long phaseStart = 0;
  static int lastMotorIdx = -1; // Track last motor for printing
  static bool running = false;  // Control flag
  static bool sequenceMode = false; // true: sequence, false: single motor
  static int singleMotorIdx = 0;
  static bool allMode = false; // New: all motors together

  // --- Serial command handling ---
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("o")) {
      running = true;
      sequenceMode = true;
      allMode = false;
      Serial.println("Started sequence mode.");
      phaseStart = millis(); // Reset timing
    } else if (cmd.equalsIgnoreCase("a")) {
      running = true;
      sequenceMode = false;
      allMode = true;
      Serial.println("Started ALL motors mode.");
      phaseStart = millis();
      phase = 0;
    } else if (cmd.length() > 0 && isDigit(cmd[0])) {
      int idx = cmd.toInt();
      if (idx >= 0 && idx < 6) {
        running = true;
        sequenceMode = false;
        allMode = false;
        singleMotorIdx = idx;
        Serial.print("Running single motor: ");
        Serial.println(singleMotorIdx);
        phaseStart = millis();
        phase = 0;
      } else {
        Serial.println("Invalid motor index.");
      }
    } else if (cmd.equalsIgnoreCase("n")) {
      // running = false;
      Serial.println("Stopped motor sequence.");
      // Optionally stop all motors immediately
      for (int i = 0; i < 6; i++) {
        parsedValues[i] = 0;
        runStepperCommand(stepper[i], 0, lastCommand[i], target[i], ENABLE_PIN[i]);
      }
    }
  }

  if (!running) {
    // If not running, do nothing
    return;
  }

  unsigned long now = millis();

  // Set phase durations (in ms)
  const unsigned long phaseDuration[3] = {4000, 4000, 500}; // +1, -1, 0

  // Only update every 50ms
  if (now - lastRun >= 50) { 
    lastRun = now;

    // Print to serial if motor changed (for sequence mode)
    if (sequenceMode && motorIdx != lastMotorIdx) {
      Serial.print("Working on motor: ");
      Serial.println(motorIdx);
      lastMotorIdx = motorIdx;
    }
    // Print to serial if single motor mode
    if (!sequenceMode && !allMode && singleMotorIdx != lastMotorIdx) {
      Serial.print("Working on motor: ");
      Serial.println(singleMotorIdx);
      lastMotorIdx = singleMotorIdx;
    }
    // Print to serial for all mode
    if (allMode && lastMotorIdx != -2) {
      Serial.println("Working on ALL motors");
      lastMotorIdx = -2;
    }

    // Clear all parsedValues
    for (int i = 0; i < 6; i++) parsedValues[i] = 0;

    if (allMode) {
      // All motors together
      float val = 0.0;
      if (phase == 0) val = 1.0;
      else if (phase == 1) val = -1.0;
      else val = 0.0;
      for (int i = 0; i < 6; i++) parsedValues[i] = val;
    } else {
      int activeIdx = sequenceMode ? motorIdx : singleMotorIdx;
      // Set current motor value based on phase
      if (phase == 0) parsedValues[activeIdx] = 1.0;
      else if (phase == 1) parsedValues[activeIdx] = -1.0;
      else parsedValues[activeIdx] = 0.0;
    }

    setServoFromFloat(parsedValues[1]);
    for (int i = 0; i < 6; i++) {
      runStepperCommand(stepper[i], parsedValues[i], lastCommand[i], target[i], ENABLE_PIN[i]);
    }

    // Handle phase transitions
    if (now - phaseStart >= phaseDuration[phase]) {
      phase++;
      phaseStart = now;
      if (phase > 2) {
        phase = 0;
        if (sequenceMode) {
          motorIdx++;
          if (motorIdx >= 6) motorIdx = 0;
        }
      }
    }
  }
}

void stepperISR() {
  for (int i = 0; i < 6; i++) {
    stepper[i].run();
  }
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