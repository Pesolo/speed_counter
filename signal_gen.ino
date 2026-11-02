
const int PULSE_PIN = 27;

const float WHEEL_DIAMETER_M = 0.4318;
const int PULSES_PER_REV = 4;

const unsigned long TIMEOUT_MS = 3000;
const unsigned long PRINT_INTERVAL_MS = 500;

volatile unsigned long pulseCount = 0;
volatile unsigned long lastPulseUs = 0;
volatile unsigned long pulsePeriodUs = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

unsigned long lastPrintMs = 0;

void IRAM_ATTR countPulse() {
  unsigned long nowUs = micros();

  portENTER_CRITICAL_ISR(&mux);

  if (lastPulseUs != 0) {
    pulsePeriodUs = nowUs - lastPulseUs;
  }

  lastPulseUs = nowUs;
  pulseCount++;

  portEXIT_CRITICAL_ISR(&mux);
}

void setup() {
  Serial.begin(115200);

  pinMode(PULSE_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PULSE_PIN), countPulse, RISING);

  Serial.println("Speed Counter");
}

void loop() {
  if (millis() - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = millis();

    unsigned long period;
    unsigned long lastPulse;
    unsigned long nowUs = micros();

    portENTER_CRITICAL(&mux);
    period = pulsePeriodUs;
    lastPulse = lastPulseUs;
    portEXIT_CRITICAL(&mux);

    float rpm = 0.0;

    bool stopped = (lastPulse == 0) ||
                   ((unsigned long)(nowUs - lastPulse) >
                    TIMEOUT_MS * 1000UL);

    if (!stopped && period > 0) {
      rpm = 60000000.0 /
            (period * (float)PULSES_PER_REV);
    }

    float speedKmh =
        rpm * PI * WHEEL_DIAMETER_M * 60.0 / 1000.0;

    Serial.printf("RPM: %.1f | Speed: %.2f km/h\n",
                  rpm, speedKmh);
  }
}
