// ESP32 wheel-speed measurement with period filtering

#include <Arduino.h>
#include <math.h>

// -------------------------
// Configuration
// -------------------------

const int SENSOR_PIN = 27;
const int PULSES_PER_REVOLUTION = 1;
const float FILTER_TIME_CONSTANT = 1.0f;
const uint32_t MIN_EDGE_SPACING_US = 0;

// Used only to declare zero speed after pulses stop.
// Choose this longer than the longest pulse interval expected
// during normal operation.
const uint32_t STOP_TIMEOUT_US = 5000000UL; // 5 seconds

// -------------------------
// Shared interrupt variables
// -------------------------

volatile uint32_t lastAcceptedEdgeTimeUs = 0;
volatile uint32_t latestPeriodUs = 0;
volatile uint32_t latestPulseTimeUs = 0;
volatile uint32_t measurementNumber = 0;

// -------------------------
// Filter variables
// -------------------------

float filteredPeriodSeconds = 0.0f;
float filteredRPM = 0.0f;

uint32_t lastProcessedMeasurement = 0;
uint32_t previousMeasurementTimeUs = 0;

bool filterInitialized = false;
bool wheelStopped = false;

// -------------------------
// Sensor interrupt
// -------------------------

void IRAM_ATTR sensorISR()
{
    uint32_t currentTimeUs = micros();

    // The first accepted edge establishes a timing reference.
    if (lastAcceptedEdgeTimeUs == 0) {
        lastAcceptedEdgeTimeUs = currentTimeUs;
        return;
    }

    uint32_t periodUs =
        currentTimeUs - lastAcceptedEdgeTimeUs;
    if (MIN_EDGE_SPACING_US > 0 &&
        periodUs < MIN_EDGE_SPACING_US) {
        return;
    }

    lastAcceptedEdgeTimeUs = currentTimeUs;

    latestPeriodUs = periodUs;
    latestPulseTimeUs = currentTimeUs;
    measurementNumber++;
}

// -------------------------
// Setup
// -------------------------

void setup()
{
    Serial.begin(115200);

    pinMode(SENSOR_PIN, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(SENSOR_PIN),
        sensorISR,
        FALLING
    );

    Serial.println("RPM measurement started");
}

// -------------------------
// Main loop
// -------------------------

void loop()
{
    uint32_t periodUs;
    uint32_t pulseTimeUs;
    uint32_t measurementCount;
    uint32_t lastEdgeUs;

    // Safely copy values updated by the interrupt.
    noInterrupts();

    periodUs = latestPeriodUs;
    pulseTimeUs = latestPulseTimeUs;
    measurementCount = measurementNumber;
    lastEdgeUs = lastAcceptedEdgeTimeUs;

    interrupts();

    // Process each new period measurement only once.
    if (measurementCount != lastProcessedMeasurement) {
        lastProcessedMeasurement = measurementCount;

        float measuredPeriodSeconds =
            periodUs / 1000000.0f;

        // After a stop, the first new valid period starts a new
        // filter sequence instead of blending with an old value.
        if (!filterInitialized || wheelStopped) {
            filteredPeriodSeconds = measuredPeriodSeconds;
            previousMeasurementTimeUs = pulseTimeUs;
            filterInitialized = true;
            wheelStopped = false;
        }
        else {
            // Time since the previous accepted measurement.
            uint32_t elapsedUs =
                pulseTimeUs - previousMeasurementTimeUs;

            float elapsedSeconds =
                elapsedUs / 1000000.0f;

            previousMeasurementTimeUs = pulseTimeUs;

            // Calculate alpha from the actual elapsed time.
            float alpha =
                1.0f - expf(
                    -elapsedSeconds /
                    FILTER_TIME_CONSTANT
                );

            // First-order IIR low-pass filter on period.
            filteredPeriodSeconds +=
                alpha *
                (measuredPeriodSeconds -
                 filteredPeriodSeconds);
        }

        // Calculate RPM from the filtered period.
        if (filteredPeriodSeconds > 0.0f) {
            filteredRPM =
                60.0f /
                (PULSES_PER_REVOLUTION *
                 filteredPeriodSeconds);
        }

        Serial.print("Measured period: ");
        Serial.print(measuredPeriodSeconds, 4);

        Serial.print(" s, Filtered period: ");
        Serial.print(filteredPeriodSeconds, 4);

        Serial.print(" s, Filtered RPM: ");
        Serial.println(filteredRPM, 2);
    }

    // Detect zero speed or a stall using an independent timeout.
    // This is not derived from an assumed RPM range.
    uint32_t currentTimeUs = micros();

    if (lastEdgeUs != 0 &&
        (uint32_t)(currentTimeUs - lastEdgeUs) > STOP_TIMEOUT_US) {

        if (!wheelStopped) {
            wheelStopped = true;
            filteredRPM = 0.0f;

            Serial.println("No pulse timeout: wheel stopped or sensor inactive");
        }
    }

    delay(20);
}
