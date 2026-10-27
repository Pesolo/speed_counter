
const int PULSE_PIN = 27;

// One pulse represents one complete wheel revolution
//const int PPR = 1;

// Change this value to your wheel diameter in meters
const float WHEEL_DIAMETER_M = 0.60;

// RPM calculation update interval
const unsigned long WINDOW_MS = 1000;

volatile unsigned long pulseCount = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

unsigned long lastTime = 0;

void IRAM_ATTR onPulse() {
  portENTER_CRITICAL_ISR(&mux);
  pulseCount++;
  portEXIT_CRITICAL_ISR(&mux);
}

void setup() {
  Serial.begin(115200);

  pinMode(PULSE_PIN, INPUT);

  attachInterrupt(
    digitalPinToInterrupt(PULSE_PIN),
    onPulse,
    RISING
  );

  lastTime = millis();

  Serial.println("Wheel Speed Counter Started");
  Serial.println("RPM and Speed (km/h)");
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastTime;

  if (elapsed >= WINDOW_MS) {
    unsigned long count;

    portENTER_CRITICAL(&mux);
    count = pulseCount;
    pulseCount = 0;
    portEXIT_CRITICAL(&mux);

    // Calculate revolutions per minute
    float rpm = (count * 60000.0) / elapsed;

    // Calculate wheel circumference in meters
    float circumference = PI * WHEEL_DIAMETER_M;

    // Convert RPM to linear speed in km/h
    float speedKmh = rpm * circumference * 60.0 / 1000.0;

    Serial.print("RPM: ");
    Serial.print(rpm, 1);

    Serial.print(" | Speed: ");
    Serial.print(speedKmh, 2);
    Serial.println(" km/h");

    lastTime = now;
  }
}
