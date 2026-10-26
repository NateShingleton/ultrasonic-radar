//ultrasonic radar

#include <Servo.h>

const int TRIG_PIN = 7;
const int ECHO_PIN = 8;

const int SERVO_PIN = 9;
const int MIN = 15; //minimum angle
const int MAX = 165; //max angle
const int STEP = 3; //step size

Servo servo; //delares name of servo

void setup() {
  Serial.begin(115200); //initialize baud rate
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  servo.attach(SERVO_PIN);
  servo.write(15); //start at min
  delay(100);

  Serial.print("Starting");
}

void loop() {
  for (int angle = MIN; angle <= MAX; angle += STEP) { //sweep up
    measureAt(angle);
  }

  for (int angle = MAX; angle >= MIN; angle -= STEP) { //sweep down
    measureAt(angle);
  }

  delay(100); // short delay between pings
}

float readDistanceCm() {
  //start pin low
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
//send 10 microsecond trigger
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  //Measure how long till echo (stops when pin goes low)
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000); //give up after 30 ms

  if (duration == 0) { return 0; } //no distance found
  return duration * 0.0343 / 2; //speed of sound (343 m/s)
}

void measureAt(int angle) {
  servo.write(angle);
  delay(60); //60 ms delay
  float cm = readDistanceCm();
  Serial.print(angle);
  Serial.print(',');
  Serial.println(cm, 1); //distance to 1 decimal place
}
