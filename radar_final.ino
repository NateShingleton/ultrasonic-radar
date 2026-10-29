// Final radar firmware (Steps 5 and 6): PIR wake-up, proximity buzzer,
// non-blocking timing with millis(), and a checksum on every line.
//
// Output, one line per measurement:   angle,distance_cm,checksum
//   e.g.  90,123.4,201
//   distance 0.0 = no echo
//   checksum = sum of the characters in "angle,distance_cm", mod 256
// Lines starting with '#' are status messages (the display prints them).
//
// Wiring:
//   HC-SR04: Trig -> D7, Echo -> D8, VCC -> 5V, GND -> GND
//   Servo:   signal -> D9, power from separate 5 V supply, shared ground
//   PIR:     OUT -> D2, VCC -> 5V, GND -> GND  (needs 30-60 s to settle after power-up)
//   Buzzer:  + -> D4, - -> GND  (ACTIVE buzzer: beeps on plain 5 V)

#include <Servo.h>

// ---------------- Pins ----------------
const int TRIG_PIN   = 7;
const int ECHO_PIN   = 8;
const int SERVO_PIN  = 9;
const int PIR_PIN    = 2;
const int BUZZER_PIN = 4;

// ---------------- Settings ----------------
const int MIN_ANGLE = 15;
const int MAX_ANGLE = 165;
const int STEP_DEG  = 3;
const int PARK_ANGLE = 90;

const unsigned long SETTLE_MS        = 40;     // servo settle time after each step
const unsigned long PING_INTERVAL_MS = 60;     // HC-SR04 datasheet minimum between pings
const unsigned long ECHO_TIMEOUT_US  = 30000;  // ~5 m round trip
const unsigned long IDLE_TIMEOUT_MS  = 10000;  // stop sweeping after 10 s with no motion
const float         ALERT_CM         = 50.0;   // buzz if anything is closer than this

const bool USE_PIR = true;  // false = sweep all the time (handy while testing)

// ---------------- State ----------------
enum Mode { IDLE, SWEEPING };
Mode mode = IDLE;

Servo servo;
int angle = MIN_ANGLE;
int direction = 1;                 // +1 = increasing angle, -1 = decreasing

unsigned long lastMoveMs   = 0;    // when the servo was last commanded
unsigned long lastPingMs   = 0;    // when the sensor last pinged
unsigned long lastMotionMs = 0;    // when the PIR last saw motion

// ---------------- Sensor ----------------
// Returns distance in cm, or 0 if no echo. Note: pulseIn() still blocks for up
// to ECHO_TIMEOUT_US. Replacing it with Timer1 input capture is a stretch goal.
float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  lastPingMs = millis();
  if (duration == 0) return 0;
  return duration * 0.0343 / 2;
}

// ---------------- Output ----------------
// Sends "angle,distance,checksum". The AVR's sprintf can't print floats, so the
// distance is sent as whole cm + one decimal digit built from integer tenths.
void sendReading(int a, float cm) {
  long tenths = (long)(cm * 10 + 0.5);
  char body[24];
  snprintf(body, sizeof(body), "%d,%ld.%ld", a, tenths / 10, tenths % 10);

  uint8_t checksum = 0;
  for (char* p = body; *p != '\0'; p++) checksum += (uint8_t)*p;  // wraps at 256

  Serial.print(body);
  Serial.print(',');
  Serial.println(checksum);
}

// ---------------- Modes ----------------
void startSweeping() {
  mode = SWEEPING;
  angle = MIN_ANGLE;
  direction = 1;
  servo.write(angle);
  lastMoveMs = millis();
  Serial.println("# sweeping");
}

void goIdle() {
  mode = IDLE;
  digitalWrite(BUZZER_PIN, LOW);
  servo.write(PARK_ANGLE);
  Serial.println("# idle");
}

// One step of the sweep: ping at the current angle, report, move to the next.
void sweepStep() {
  float cm = readDistanceCm();
  sendReading(angle, cm);

  bool tooClose = (cm > 0 && cm < ALERT_CM);
  digitalWrite(BUZZER_PIN, tooClose ? HIGH : LOW);

  int next = angle + direction * STEP_DEG;
  if (next > MAX_ANGLE || next < MIN_ANGLE) {
    direction = -direction;              // reverse at each end
    next = angle + direction * STEP_DEG;
  }
  angle = next;
  servo.write(angle);
  lastMoveMs = millis();
}

// ---------------- Setup / loop ----------------
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  servo.attach(SERVO_PIN);
  servo.write(PARK_ANGLE);
  Serial.println("# radar ready");

  if (USE_PIR) goIdle();
  else startSweeping();
}

void loop() {
  unsigned long now = millis();

  // Job 1: watch for motion. Runs every pass through loop(), never waits.
  if (USE_PIR && digitalRead(PIR_PIN) == HIGH) {
    lastMotionMs = now;
    if (mode == IDLE) startSweeping();
  }

  // Job 2: go back to idle after a quiet period.
  if (USE_PIR && mode == SWEEPING && now - lastMotionMs >= IDLE_TIMEOUT_MS) {
    goIdle();
  }

  // Job 3: take the next measurement once the servo has settled AND the
  // sensor's minimum ping spacing has passed. No delay() anywhere.
  if (mode == SWEEPING &&
      now - lastMoveMs >= SETTLE_MS &&
      now - lastPingMs >= PING_INTERVAL_MS) {
    sweepStep();
  }
}
