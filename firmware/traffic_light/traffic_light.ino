// Pin definitions for 4 sets of traffic lights
const int greenLEDs[4]  = {2, 5, 8, 11};
const int yellowLEDs[4] = {3, 6, 9, 12};
const int redLEDs[4]    = {4, 7, 10, 13};

// Timing in milliseconds
const int greenTime  = 5000; // 5 seconds
const int yellowTime = 2000; // 2 seconds

void setup() {
  // Initialize all LEDs as outputs
  for (int i = 0; i < 4; i++) {
    pinMode(greenLEDs[i], OUTPUT);
    pinMode(yellowLEDs[i], OUTPUT);
    pinMode(redLEDs[i], OUTPUT);
  }
  
  // Initially, all reds ON
  for (int i = 0; i < 4; i++) {
    digitalWrite(redLEDs[i], HIGH);
    digitalWrite(greenLEDs[i], LOW);
    digitalWrite(yellowLEDs[i], LOW);
  }
}

void loop() {
  for (int i = 0; i < 4; i++) {
    // Turn current set GREEN
    digitalWrite(greenLEDs[i], HIGH);
    digitalWrite(redLEDs[i], LOW);
    delay(greenTime);

    // Turn current set YELLOW
    digitalWrite(greenLEDs[i], LOW);
    digitalWrite(yellowLEDs[i], HIGH);
    delay(yellowTime);

    // Turn current set RED
    digitalWrite(yellowLEDs[i], LOW);
    digitalWrite(redLEDs[i], HIGH);
  }
}

