//ultrasonic radar

const int TRIG_PIN = 7;
const int ECHO_PIN = 8;

void setup() {
  Serial.begin(115200); //initialize baud rate
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.print("Starting");
}

void loop() {
  //start pin low
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
//send 10 microsecond trigger
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  //Measure how long till echo (stops when pin goes low)
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000); //give up after 30 ms

  if (duration == 0) {
    Serial.println("No Echo");
  } else {
    float distanceCm = duration * 0.0343 / 2; //speed of sound (343 m/s)
    Serial.print(distanceCm);
    Serial.print("cm ");
  }

  delay(100); // short delay between pings
}
