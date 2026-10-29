#include <Servo.h>

const int TRIG_PIN  = 7;
const int ECHO_PIN  = 8;
const int SERVO_PIN = 9;

const int MIN_ANGLE = 15;
const int MAX_ANGLE = 165;
const int STEP_DEG  = 3;
const int SETTLE_MS = 60;   // also covers the sensor's 60 ms minimum between pings

const unsigned long ECHO_TIMEOUT_US = 30000;  // ~5 m round trip; longer = no echo

Servo servo;

// Returns distance in cm, or 0 if no echo came back.
float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) return 0;
  return duration * 0.0343 / 2;   // 343 m/s = 0.0343 cm/us; /2 for out and back
}

void measureAt(int angle) {
  servo.write(angle);
  delay(SETTLE_MS);
  float cm = readDistanceCm();
  Serial.print(angle);
  Serial.print(',');
  Serial.println(cm, 1);   // one decimal place
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  servo.attach(SERVO_PIN);
  servo.write(MIN_ANGLE);
  delay(500);
}

void loop() {
  for (int angle = MIN_ANGLE; angle <= MAX_ANGLE; angle += STEP_DEG) measureAt(angle);
  for (int angle = MAX_ANGLE; angle >= MIN_ANGLE; angle -= STEP_DEG) measureAt(angle);
}
