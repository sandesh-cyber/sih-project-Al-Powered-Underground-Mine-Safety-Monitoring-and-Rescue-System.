// ============================================================
// SMART MINER HELMET - FULL COMBINED PROGRAM
// ============================================================
// ESP32 DevKit V1
//
// CURRENT HARDWARE
//   MQ-4       -> GPIO34 (AO through 10k/10k divider)
//   MQ-7       -> GPIO35 (AO through 10k/10k divider)
//   MAX30100   -> I2C 0x57, SDA 21, SCL 22
//   MPU6050    -> I2C 0x68, SDA 21, SCL 22
//   SOS button -> GPIO27, INPUT_PULLUP
//   Buzzer     -> GPIO25
//   Red LED    -> GPIO26
//   Motor      -> GPIO33 through transistor driver
//   Yellow LED -> GPIO12
//   Green LED  -> GPIO14
//
// BME280 is intentionally not included because it is parked.
// MQ-7 is MONITOR ONLY until its proper heater cycle is added.
// ONLY MQ-4 currently controls the gas WARNING state.
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_BMP085.h>
#include <DHT.h>
#include <WiFi.h>
#include <esp_now.h>

#define MAX30100_ADDR 0x57

// ============================================================
// MAX30100 V3 - STABLE HEART RATE TEST
// ESP32 DevKit V1
//
// SDA = GPIO21
// SCL = GPIO22
// MAX30100 I2C = 0x57
// ============================================================


// MAX30100 Registers
#define REG_INT_STATUS   0x00
#define REG_INT_ENABLE   0x01
#define REG_FIFO_WR_PTR  0x02
#define REG_OVF_COUNTER  0x03
#define REG_FIFO_RD_PTR  0x04
#define REG_FIFO_DATA    0x05
#define REG_MODE_CONFIG  0x06
#define REG_SPO2_CONFIG  0x07
#define REG_LED_CONFIG   0x09
#define REG_REV_ID       0xFE
#define REG_PART_ID      0xFF


// ============================================================
// SETTINGS
// ============================================================

// Finger/skin contact threshold
const uint16_t CONTACT_THRESHOLD = 5000;

// LED current
// 0x44 = relatively low/moderate current
const uint8_t LED_CURRENT = 0x44;

// Sensor stabilization
const unsigned long STABILIZATION_TIME = 5000;

// Physiological IBI limits
// 40 BPM  = 1500 ms
// 180 BPM = 333 ms
const unsigned long MIN_IBI = 333;
const unsigned long MAX_IBI = 1500;

// After this time without a valid beat,
// BPM becomes invalid.
const unsigned long BPM_TIMEOUT = 4000;

// Minimum signal amplitude
const float MIN_SIGNAL = 15.0;

// Good signal amplitude
const float GOOD_SIGNAL = 40.0;

// Maximum allowed sudden signal change
const float ARTIFACT_LIMIT = 1200.0;

// Number of valid beats required for HIGH confidence
const int HIGH_CONFIDENCE_BEATS = 6;


// ============================================================
// SIGNAL VARIABLES
// ============================================================

float dcValue = 0.0;

float filteredSignal = 0.0;
float previousSignal = 0.0;
float previousPreviousSignal = 0.0;

bool filterInitialized = false;


// ============================================================
// BEAT VARIABLES
// ============================================================

unsigned long lastBeatTime = 0;
unsigned long lastValidBeatTime = 0;

float lastPeak = 0.0;

bool beatArmed = true;

int validBeatCount = 0;


// ============================================================
// BPM HISTORY
// ============================================================

const int BPM_HISTORY_SIZE = 8;

float bpmHistory[BPM_HISTORY_SIZE];

int bpmHistoryCount = 0;

float stableBPM = 0.0;


// ============================================================
// CONTACT / STATE
// ============================================================

bool contactPresent = false;

unsigned long contactStartTime = 0;

bool stabilized = false;


// ============================================================
// QUALITY
// ============================================================

enum SignalQuality
{
    NO_CONTACT,
    POOR,
    FAIR,
    GOOD
};

SignalQuality signalQuality = NO_CONTACT;


// ============================================================
// I2C WRITE
// ============================================================

void writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(MAX30100_ADDR);

    Wire.write(reg);
    Wire.write(value);

    Wire.endTransmission();
}


// ============================================================
// I2C READ
// ============================================================

uint8_t readRegister(uint8_t reg)
{
    Wire.beginTransmission(MAX30100_ADDR);

    Wire.write(reg);

    if (Wire.endTransmission(false) != 0)
    {
        return 0;
    }

    Wire.requestFrom((uint8_t)MAX30100_ADDR, (uint8_t)1);

    if (Wire.available())
    {
        return Wire.read();
    }

    return 0;
}


// ============================================================
// READ MAX30100 FIFO
// ============================================================

bool readFIFO(uint16_t &ir, uint16_t &red)
{
    Wire.beginTransmission(MAX30100_ADDR);

    Wire.write(REG_FIFO_DATA);

    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    Wire.requestFrom((uint8_t)MAX30100_ADDR, (uint8_t)4);

    if (Wire.available() < 4)
    {
        return false;
    }

    uint8_t irHigh = Wire.read();
    uint8_t irLow  = Wire.read();

    uint8_t redHigh = Wire.read();
    uint8_t redLow  = Wire.read();

    ir = ((uint16_t)irHigh << 8) | irLow;

    red = ((uint16_t)redHigh << 8) | redLow;

    return true;
}


// ============================================================
// MAX30100 SETUP
// ============================================================

bool setupMAX30100()
{
    uint8_t partID = readRegister(REG_PART_ID);

    Serial.print("Part ID = 0x");
    Serial.println(partID, HEX);

    if (partID != 0x11)
    {
        Serial.println();
        Serial.println("ERROR: MAX30100 NOT DETECTED");
        return false;
    }

    Serial.println("Detected: MAX30100");

    // --------------------------------------------------------
    // RESET
    // --------------------------------------------------------

    writeRegister(REG_MODE_CONFIG, 0x40);

    delay(100);


    // --------------------------------------------------------
    // CLEAR FIFO
    // --------------------------------------------------------

    writeRegister(REG_FIFO_WR_PTR, 0x00);

    writeRegister(REG_OVF_COUNTER, 0x00);

    writeRegister(REG_FIFO_RD_PTR, 0x00);


    // --------------------------------------------------------
    // SPO2 CONFIG
    //
    // 0x47:
    // - High resolution
    // - 100 samples/sec
    // --------------------------------------------------------

    writeRegister(REG_SPO2_CONFIG, 0x47);


    // --------------------------------------------------------
    // LED CURRENT
    // --------------------------------------------------------

    writeRegister(REG_LED_CONFIG, LED_CURRENT);


    // --------------------------------------------------------
    // SPO2 MODE
    // --------------------------------------------------------

    writeRegister(REG_MODE_CONFIG, 0x03);


    // --------------------------------------------------------
    // INTERRUPTS OFF
    // --------------------------------------------------------

    writeRegister(REG_INT_ENABLE, 0x00);

    delay(100);

    Serial.println("MAX30100 configured.");
    Serial.println();

    return true;
}


// ============================================================
// QUALITY TEXT
// ============================================================

const char* qualityText()
{
    switch (signalQuality)
    {
        case GOOD:
            return "GOOD";

        case FAIR:
            return "FAIR";

        case POOR:
            return "POOR";

        default:
            return "NO CONTACT";
    }
}


// ============================================================
// CONFIDENCE
// ============================================================

const char* confidenceText()
{
    if (stableBPM <= 0)
    {
        return "LOW";
    }

    if (signalQuality == POOR ||
        signalQuality == NO_CONTACT)
    {
        return "LOW";
    }

    if (validBeatCount >= HIGH_CONFIDENCE_BEATS &&
        signalQuality == GOOD)
    {
        return "HIGH";
    }

    if (validBeatCount >= 3)
    {
        return "MEDIUM";
    }

    return "LOW";
}


// ============================================================
// CLEAR BPM
// ============================================================

void clearBPM()
{
    stableBPM = 0;

    bpmHistoryCount = 0;

    validBeatCount = 0;

    lastBeatTime = 0;

    lastValidBeatTime = 0;

    lastPeak = 0;

    beatArmed = true;
}


// ============================================================
// RESET SIGNAL PROCESSING
// ============================================================

void resetSignalProcessing()
{
    dcValue = 0;

    filteredSignal = 0;

    previousSignal = 0;

    previousPreviousSignal = 0;

    filterInitialized = false;

    stabilized = false;

    clearBPM();
}


// ============================================================
// ADD BPM TO HISTORY
// ============================================================

void addBPM(float bpm)
{
    if (bpm < 40 || bpm > 180)
    {
        return;
    }

    // --------------------------------------------------------
    // First values
    // --------------------------------------------------------

    if (bpmHistoryCount < BPM_HISTORY_SIZE)
    {
        bpmHistory[bpmHistoryCount] = bpm;

        bpmHistoryCount++;
    }
    else
    {
        // Shift old values
        for (int i = 0; i < BPM_HISTORY_SIZE - 1; i++)
        {
            bpmHistory[i] = bpmHistory[i + 1];
        }

        bpmHistory[BPM_HISTORY_SIZE - 1] = bpm;
    }


    // --------------------------------------------------------
    // Calculate median
    // --------------------------------------------------------

    float sorted[BPM_HISTORY_SIZE];

    for (int i = 0; i < bpmHistoryCount; i++)
    {
        sorted[i] = bpmHistory[i];
    }

    for (int i = 0; i < bpmHistoryCount - 1; i++)
    {
        for (int j = i + 1; j < bpmHistoryCount; j++)
        {
            if (sorted[j] < sorted[i])
            {
                float temp = sorted[i];

                sorted[i] = sorted[j];

                sorted[j] = temp;
            }
        }
    }


    if (bpmHistoryCount % 2 == 1)
    {
        stableBPM = sorted[bpmHistoryCount / 2];
    }
    else
    {
        stableBPM =
            (sorted[bpmHistoryCount / 2 - 1] +
             sorted[bpmHistoryCount / 2]) / 2.0;
    }
}


// ============================================================
// SIGNAL QUALITY
// ============================================================

void calculateSignalQuality(float amplitude)
{
    if (!contactPresent)
    {
        signalQuality = NO_CONTACT;

        return;
    }

    if (amplitude < MIN_SIGNAL)
    {
        signalQuality = POOR;
    }
    else if (amplitude < GOOD_SIGNAL)
    {
        signalQuality = FAIR;
    }
    else
    {
        signalQuality = GOOD;
    }
}


// ============================================================
// BEAT PROCESSING
// ============================================================

void processBeat(float signal, unsigned long now)
{
    // --------------------------------------------------------
    // Calculate local change
    // --------------------------------------------------------

    float difference =
        fabs(signal - previousSignal);


    // --------------------------------------------------------
    // Dynamic noise estimate
    // --------------------------------------------------------

    static float noiseLevel = 20.0;

    noiseLevel =
        0.95 * noiseLevel +
        0.05 * difference;


    // --------------------------------------------------------
    // Dynamic threshold
    // --------------------------------------------------------

    float threshold =
        max(18.0f, noiseLevel * 2.5f);


    // --------------------------------------------------------
    // Local maximum
    // --------------------------------------------------------

    bool localMaximum =
        (previousSignal > previousPreviousSignal) &&
        (previousSignal >= signal);


    // --------------------------------------------------------
    // Detect peak
    // --------------------------------------------------------

    if (localMaximum && beatArmed)
    {
        float peak = previousSignal;


        // Peak must be strong enough
        if (peak > threshold)
        {
            // ------------------------------------------------
            // First beat
            // ------------------------------------------------

            if (lastBeatTime == 0)
            {
                lastBeatTime = now;

                lastPeak = peak;

                beatArmed = false;

                return;
            }


            // ------------------------------------------------
            // Calculate IBI
            // ------------------------------------------------

            unsigned long ibi =
                now - lastBeatTime;


            // ------------------------------------------------
            // Check IBI range
            // ------------------------------------------------

            if (ibi >= MIN_IBI &&
                ibi <= MAX_IBI)
            {
                float bpm =
                    60000.0 / ibi;


                // --------------------------------------------
                // Peak consistency
                // --------------------------------------------

                bool peakValid = true;

                if (lastPeak > 0)
                {
                    float ratio =
                        peak / lastPeak;

                    if (ratio < 0.35 ||
                        ratio > 2.8)
                    {
                        peakValid = false;
                    }
                }


                // --------------------------------------------
                // BPM range
                // --------------------------------------------

                if (bpm < 40 ||
                    bpm > 150)
                {
                    peakValid = false;
                }


                // --------------------------------------------
                // Accept beat
                // --------------------------------------------

                if (peakValid)
                {
                    addBPM(bpm);

                    validBeatCount++;

                    lastValidBeatTime = now;

                    Serial.println();

                    Serial.println("******** BEAT ACCEPTED ********");

                    Serial.print("IBI        : ");
                    Serial.print(ibi);
                    Serial.println(" ms");

                    Serial.print("Instant BPM: ");
                    Serial.println(bpm, 1);

                    Serial.print("Stable BPM : ");
                    Serial.println(stableBPM, 1);

                    Serial.print("Valid beats: ");
                    Serial.println(validBeatCount);

                    Serial.print("Signal     : ");
                    Serial.println(qualityText());

                    Serial.print("Confidence : ");
                    Serial.println(confidenceText());

                    Serial.println("*******************************");
                    Serial.println();


                    lastBeatTime = now;

                    lastPeak = peak;
                }
            }


            // Don't detect same peak again
            beatArmed = false;
        }
    }


    // --------------------------------------------------------
    // Re-arm detector
    // --------------------------------------------------------

    float rearmLevel =
        threshold * 0.35;


    if (signal < rearmLevel)
    {
        beatArmed = true;
    }
}


// ============================================================
// PRINT STATUS
// ============================================================

void printStatus(
    uint16_t ir,
    uint16_t red,
    float signal)
{
    Serial.print("IR=");
    Serial.print(ir);

    Serial.print("  RED=");
    Serial.print(red);

    Serial.print("  Signal=");
    Serial.print(signal, 1);

    Serial.print("  Quality=");
    Serial.print(qualityText());

    Serial.print("  BPM=");

    bool bpmValid =
        stableBPM > 0 &&
        lastValidBeatTime > 0 &&
        millis() - lastValidBeatTime <= BPM_TIMEOUT;

    if (bpmValid)
    {
        Serial.print(stableBPM, 1);
    }
    else
    {
        Serial.print("--");
    }

    Serial.print("  Confidence=");

    if (bpmValid)
    {
        Serial.println(confidenceText());
    }
    else
    {
        Serial.println("LOW");
    }
}



// ============================================================
// SYSTEM PIN DEFINITIONS
// ============================================================
#define SDA_PIN 21
#define SCL_PIN 22

#define MPU6050_ADDR 0x68

#define MQ4_PIN 34
#define MQ7_PIN 35

#define SOS_PIN 27
#define BUZZER_PIN 25
#define RED_LED_PIN 26
#define MOTOR_PIN 33
#define YELLOW_LED_PIN 12
#define GREEN_LED_PIN 14

// BMP180 + DHT11
#define DHT_PIN 4
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_BMP085 bmp;

bool bmp180Present = false;
float helmetTemperature = 0.0f;
float helmetPressure = 0.0f;
float helmetHumidity = 0.0f;

unsigned long lastEnvironmentRead = 0;
const unsigned long ENVIRONMENT_INTERVAL = 2000;

// ============================================================
// MQ SENSOR SETTINGS
// ============================================================
const int MQ_BASELINE_SAMPLES = 100;
const int MQ_FILTER_SAMPLES = 10;

// MQ-4 prototype warning logic
// Positive change only: current reading must rise above baseline.
// WARNING turns ON at +15% and clears at +10% (hysteresis).
const float GAS_CHANGE_THRESHOLD = 15.0f;
const float GAS_CLEAR_THRESHOLD = 10.0f;

// MQ-4 needs continuous heater operation and stabilization.
// This is a startup warm-up for the prototype; it is NOT a
// certified mine-gas calibration procedure.
const unsigned long MQ4_WARMUP_TIME = 60000UL;

float mq4Baseline = 0.0f;
bool mq4WarningActive = false;
int mq4WarningConfirmCount = 0;
const int MQ4_WARNING_CONFIRMATIONS = 3;

float mq7Baseline = 0.0f;

float mq4Raw = 0.0f;
float mq7Raw = 0.0f;
float mq4Voltage = 0.0f;
float mq7Voltage = 0.0f;
float mq4Change = 0.0f;
float mq7Change = 0.0f;

// ============================================================
// MPU6050 REGISTERS / SETTINGS
// ============================================================
#define MPU_PWR_MGMT_1 0x6B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_GYRO_CONFIG 0x1B
#define MPU_WHO_AM_I 0x75
#define MPU_ACCEL_XOUT_H 0x3B

const float FALL_HIGH_ACCEL = 2.20f;
const float FALL_LOW_ACCEL = 0.55f;
const float FALL_GYRO_LIMIT = 180.0f;
const unsigned long FALL_CHECK_TIME = 2000;
const float INACTIVE_ACCEL_MIN = 0.75f;
const float INACTIVE_ACCEL_MAX = 1.25f;
const float INACTIVE_GYRO = 35.0f;

float accelX = 0.0f;
float accelY = 0.0f;
float accelZ = 0.0f;
float gyroX = 0.0f;
float gyroY = 0.0f;
float gyroZ = 0.0f;
float accelMagnitude = 1.0f;
float gyroMagnitude = 0.0f;

bool mpuPresent = false;
bool fallSuspected = false;
bool fallDetected = false;
bool fallEventTriggered = false;
unsigned long fallStartTime = 0;
unsigned long fallDetectedTime = 0;

// ============================================================
// SAFETY STATE
// ============================================================
enum SafetyState
{
    SAFE_STATE,
    WARNING_STATE,
    EMERGENCY_STATE
};

SafetyState safetyState = SAFE_STATE;

// ============================================================
// ROVER HAZARD STATE
// Declared here so safety/status functions can use it.
// ============================================================
uint8_t roverHazardState = 0;
char roverHazardReason[48] = "No rover hazard";
unsigned long roverHazardCommandCount = 0;
unsigned long roverHazardAckCount = 0;

// ============================================================
// SYSTEM TIMING
// ============================================================
unsigned long lastIntegratedPrint = 0;
unsigned long lastGasRead = 0;
unsigned long lastMPURead = 0;
const unsigned long GAS_INTERVAL = 500;
const unsigned long MPU_INTERVAL = 100;

// ============================================================
// GENERIC I2C HELPERS
// ============================================================
uint8_t readDeviceRegister(uint8_t device, uint8_t reg)
{
    Wire.beginTransmission(device);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0)
    {
        return 0;
    }

    Wire.requestFrom((uint8_t)device, (uint8_t)1);

    if (Wire.available())
    {
        return Wire.read();
    }

    return 0;
}

void writeDeviceRegister(uint8_t device, uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(device);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

// ============================================================
// I2C SCANNER
// ============================================================
void scanI2C()
{
    Serial.println();
    Serial.println("==============================================");
    Serial.println("I2C BUS SCAN");
    Serial.println("==============================================");

    int found = 0;

    for (uint8_t address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0)
        {
            Serial.print("I2C device found at 0x");

            if (address < 16)
            {
                Serial.print("0");
            }

            Serial.println(address, HEX);
            found++;
        }
    }

    if (found == 0)
    {
        Serial.println("No I2C devices found.");
    }

    Serial.println("==============================================");
    Serial.println();
}

// ============================================================
// ADC / MQ HELPERS
// ============================================================
float readAveragedADC(uint8_t pin)
{
    long total = 0;

    for (int i = 0; i < MQ_FILTER_SAMPLES; i++)
    {
        total += analogRead(pin);
        delay(5);
    }

    return (float)total / MQ_FILTER_SAMPLES;
}

float calculateGasChange(float raw, float baseline)
{
    if (baseline <= 0.1f)
    {
        return 0.0f;
    }

    return ((raw - baseline) / baseline) * 100.0f;
}

void calibrateMQ4()
{
    Serial.println("==============================================");
    Serial.println("MQ-4 STARTUP WARM-UP");
    Serial.println("==============================================");
    Serial.println("MQ-4 heater: CONTINUOUS OPERATION");
    Serial.println("Keep MQ-4 in normal clean air.");
    Serial.println("Waiting 60 seconds for startup stabilization...");
    Serial.println();

    unsigned long warmupStart = millis();

    while (millis() - warmupStart < MQ4_WARMUP_TIME)
    {
        unsigned long elapsed = millis() - warmupStart;
        unsigned long remaining = (MQ4_WARMUP_TIME - elapsed) / 1000UL;

        if (elapsed % 10000UL < 100UL)
        {
            Serial.print("MQ-4 warm-up remaining: ");
            Serial.print(remaining);
            Serial.println(" s");
        }

        // Keep reading the heater/sensor normally during warm-up.
        // No baseline is calculated during this period.
        analogRead(MQ4_PIN);
        delay(100);
    }

    Serial.println();
    Serial.println("MQ-4 warm-up complete.");
    Serial.println("==============================================");
    Serial.println("MQ-4 BASELINE CALIBRATION");
    Serial.println("==============================================");
    Serial.println("Keep MQ-4 in normal clean air.");
    Serial.println("Collecting 100 baseline samples...");

    long total = 0;

    for (int i = 0; i < MQ_BASELINE_SAMPLES; i++)
    {
        total += analogRead(MQ4_PIN);

        if ((i + 1) % 10 == 0)
        {
            Serial.print("Samples: ");
            Serial.println(i + 1);
        }

        delay(100);
    }

    mq4Baseline = (float)total / MQ_BASELINE_SAMPLES;

    // Start from a known clean-air state after calibration.
    mq4WarningActive = false;
    mq4WarningConfirmCount = 0;
    mq4Change = 0.0f;

    Serial.print("MQ-4 Baseline RAW = ");
    Serial.println(mq4Baseline, 1);
    Serial.println("MQ-4 baseline ready.");
    Serial.println("Warning threshold : +15% delta");
    Serial.println("Clear threshold   : +10% delta");
    Serial.println("Confirmation      : 3 consecutive readings");
    Serial.println("==============================================");
    Serial.println();
}

void calibrateMQ7()
{
    Serial.println("==============================================");
    Serial.println("MQ-7 INITIAL BASELINE");
    Serial.println("==============================================");
    Serial.println("Keep MQ-7 in normal clean air.");
    Serial.println("Collecting 100 baseline samples...");

    long total = 0;

    for (int i = 0; i < MQ_BASELINE_SAMPLES; i++)
    {
        total += analogRead(MQ7_PIN);

        if ((i + 1) % 10 == 0)
        {
            Serial.print("Samples: ");
            Serial.println(i + 1);
        }

        delay(100);
    }

    mq7Baseline = (float)total / MQ_BASELINE_SAMPLES;

    Serial.print("MQ-7 Baseline RAW = ");
    Serial.println(mq7Baseline, 1);
    Serial.println("MQ-7 is MONITOR ONLY.");
    Serial.println("Proper MQ-7 heater cycle will be added later.");
    Serial.println("==============================================");
    Serial.println();
}

void updateGasSensors()
{
    mq4Raw = readAveragedADC(MQ4_PIN);
    mq7Raw = readAveragedADC(MQ7_PIN);

    mq4Voltage = (mq4Raw / 4095.0f) * 3.3f;
    mq7Voltage = (mq7Raw / 4095.0f) * 3.3f;

    mq4Change = calculateGasChange(mq4Raw, mq4Baseline);
    mq7Change = calculateGasChange(mq7Raw, mq7Baseline);

    // --------------------------------------------------------
    // MQ-4 WARNING FILTER
    // --------------------------------------------------------
    // Only a positive rise above baseline can indicate elevated
    // methane in this prototype. Negative drift is not treated
    // as a methane warning.
    //
    // Require 3 consecutive readings above +15% before warning.
    // Once active, clear only below +10% to avoid chatter.
    // --------------------------------------------------------

    if (!mq4WarningActive)
    {
        if (mq4Change >= GAS_CHANGE_THRESHOLD)
        {
            mq4WarningConfirmCount++;

            Serial.print("MQ-4 elevated reading confirmation ");
            Serial.print(mq4WarningConfirmCount);
            Serial.print("/");
            Serial.println(MQ4_WARNING_CONFIRMATIONS);

            if (mq4WarningConfirmCount >= MQ4_WARNING_CONFIRMATIONS)
            {
                mq4WarningActive = true;

                Serial.println("⚠ MQ-4 WARNING CONFIRMED");
                Serial.print("MQ-4 Change = +");
                Serial.print(mq4Change, 1);
                Serial.println("%");
            }
        }
        else
        {
            mq4WarningConfirmCount = 0;
        }
    }
    else
    {
        if (mq4Change <= GAS_CLEAR_THRESHOLD)
        {
            mq4WarningActive = false;
            mq4WarningConfirmCount = 0;

            Serial.println("MQ-4 warning cleared.");
            Serial.print("MQ-4 Change = ");
            Serial.print(mq4Change, 1);
            Serial.println("%");
        }
    }
}

// ============================================================
// BMP180 + DHT11
// BMP180: temperature + pressure on I2C (SDA21/SCL22)
// DHT11: humidity on GPIO4
// ============================================================
void setupEnvironmentSensors()
{
    Serial.println("==============================================");
    Serial.println("ENVIRONMENT SENSOR INITIALIZATION");
    Serial.println("==============================================");

    dht.begin();
    Serial.println("DHT11 initialized on GPIO4.");

    if (bmp.begin())
    {
        bmp180Present = true;
        Serial.println("BMP180 detected.");
    }
    else
    {
        bmp180Present = false;
        Serial.println("BMP180 NOT DETECTED.");
        Serial.println("Check BMP180 VCC/GND/SDA21/SCL22.");
    }

    Serial.println("==============================================");
    Serial.println();
}

void updateEnvironmentSensors()
{
    float h = dht.readHumidity();
    float dhtTemp = dht.readTemperature();

    if (!isnan(h))
        helmetHumidity = h;

    if (bmp180Present)
    {
        helmetTemperature = bmp.readTemperature();
        helmetPressure = bmp.readPressure() / 100.0f;
    }
    else if (!isnan(dhtTemp))
    {
        helmetTemperature = dhtTemp;
    }
}

// ============================================================
// MPU6050 INITIALIZATION
// ============================================================
bool setupMPU6050()
{
    uint8_t who = readDeviceRegister(MPU6050_ADDR, MPU_WHO_AM_I);

    Serial.print("MPU6050 WHO_AM_I = 0x");
    Serial.println(who, HEX);

    if (who != 0x68)
    {
        Serial.println("MPU6050 NOT DETECTED.");
        return false;
    }

    writeDeviceRegister(MPU6050_ADDR, MPU_PWR_MGMT_1, 0x00);
    delay(100);

    writeDeviceRegister(MPU6050_ADDR, MPU_ACCEL_CONFIG, 0x00);
    writeDeviceRegister(MPU6050_ADDR, MPU_GYRO_CONFIG, 0x00);

    Serial.println("MPU6050 initialized at 0x68.");
    return true;
}

// ============================================================
// MPU6050 RAW READ
// ============================================================
bool readMPU6050()
{
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(MPU_ACCEL_XOUT_H);

    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)14);

    if (Wire.available() < 14)
    {
        return false;
    }

    int16_t ax = ((int16_t)Wire.read() << 8) | Wire.read();
    int16_t ay = ((int16_t)Wire.read() << 8) | Wire.read();
    int16_t az = ((int16_t)Wire.read() << 8) | Wire.read();

    // Skip temperature registers.
    Wire.read();
    Wire.read();

    int16_t gx = ((int16_t)Wire.read() << 8) | Wire.read();
    int16_t gy = ((int16_t)Wire.read() << 8) | Wire.read();
    int16_t gz = ((int16_t)Wire.read() << 8) | Wire.read();

    // ±2g = 16384 LSB/g
    accelX = ax / 16384.0f;
    accelY = ay / 16384.0f;
    accelZ = az / 16384.0f;

    // ±250 dps = 131 LSB/(degree/s)
    gyroX = gx / 131.0f;
    gyroY = gy / 131.0f;
    gyroZ = gz / 131.0f;

    accelMagnitude = sqrt(
        accelX * accelX +
        accelY * accelY +
        accelZ * accelZ);

    gyroMagnitude = sqrt(
        gyroX * gyroX +
        gyroY * gyroY +
        gyroZ * gyroZ);

    return true;
}

// ============================================================
// FALL DETECTION
// ============================================================
void resetFallState()
{
    fallSuspected = false;
    fallDetected = false;
    fallEventTriggered = false;
    fallStartTime = 0;
    fallDetectedTime = 0;
}

void processFallDetection()
{
    if (!mpuPresent)
    {
        return;
    }

    unsigned long now = millis();

    if (!readMPU6050())
    {
        return;
    }

    bool strongImpact = accelMagnitude >= FALL_HIGH_ACCEL;
    bool possibleFreeFall = accelMagnitude <= FALL_LOW_ACCEL;
    bool strongRotation = gyroMagnitude >= FALL_GYRO_LIMIT;

    bool abnormalMotion =
        strongImpact ||
        possibleFreeFall ||
        strongRotation;

    if (abnormalMotion && !fallSuspected)
    {
        fallSuspected = true;
        fallStartTime = now;
        fallEventTriggered = false;

        Serial.println();
        Serial.println("⚠ POSSIBLE FALL DETECTED");
        Serial.println("Checking for impact / post-fall inactivity...");
    }

    if (fallSuspected)
    {
        bool minerStill =
            accelMagnitude >= INACTIVE_ACCEL_MIN &&
            accelMagnitude <= INACTIVE_ACCEL_MAX &&
            gyroMagnitude < INACTIVE_GYRO;

        if (minerStill &&
            now - fallStartTime >= FALL_CHECK_TIME)
        {
            if (!fallDetected)
            {
                fallDetected = true;
                fallDetectedTime = now;

                Serial.println();
                Serial.println("==============================================");
                Serial.println("🚨 FALL DETECTED!");
                Serial.println("🚨 MINER MAY NEED HELP!");
                Serial.println("==============================================");
            }
        }

        // If movement resumes after the suspected event, clear it.
        if (!minerStill &&
            now - fallStartTime > FALL_CHECK_TIME)
        {
            if (fallDetected)
            {
                Serial.println("Movement detected.");
                Serial.println("Fall alert reset.");
            }

            resetFallState();
        }
    }
}

// ============================================================
// MOTION TEXT
// ============================================================
const char* motionText()
{
    if (fallDetected)
    {
        return "FALL DETECTED";
    }

    if (fallSuspected)
    {
        return "FALL SUSPECTED";
    }

    // Prototype motion indication.
    if (accelMagnitude < 0.80f || accelMagnitude > 1.20f ||
        gyroMagnitude > 50.0f)
    {
        return "MOVING";
    }

    return "NORMAL";
}

// ============================================================
// SOS INPUT
// ============================================================
bool isSOSActive()
{
    return digitalRead(SOS_PIN) == LOW;
}

// ============================================================
// SAFETY DECISION
// ============================================================
void determineSafetyState()
{
    bool sosActive = isSOSActive();

    // --------------------------------------------------------
    // EMERGENCY HAS HIGHEST PRIORITY
    // --------------------------------------------------------
    if (sosActive || fallDetected)
    {
        safetyState = EMERGENCY_STATE;
        return;
    }

    // ROVER-11.6: local SOS/FALL always outranks Rover environmental state.
    if (roverHazardState == 2)
    {
        safetyState = EMERGENCY_STATE;
        return;
    }

    if (roverHazardState == 1)
    {
        safetyState = WARNING_STATE;
        return;
    }

    // --------------------------------------------------------
    // IMPORTANT MQ-7 FIX
    // --------------------------------------------------------
    // MQ-7 is deliberately NOT included here because the
    // current module is not running its correct heater cycle.
    // A large MQ-7 drift must therefore NOT create WARNING.
    // --------------------------------------------------------
    bool gasWarning = mq4WarningActive;

    if (gasWarning)
    {
        safetyState = WARNING_STATE;
        return;
    }

    safetyState = SAFE_STATE;
}

// ============================================================
// OUTPUT CONTROL
// ============================================================
void allOutputsOff()
{
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(YELLOW_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(MOTOR_PIN, LOW);
}

void updateOutputs()
{
    allOutputsOff();

    if (safetyState == SAFE_STATE)
    {
        digitalWrite(GREEN_LED_PIN, HIGH);
    }
    else if (safetyState == WARNING_STATE)
    {
        // ROVER-11.6 requirement:
        // WARNING = YELLOW + BUZZER + VIBRATION
        digitalWrite(YELLOW_LED_PIN, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);
        digitalWrite(MOTOR_PIN, HIGH);
    }
    else
    {
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);
        digitalWrite(MOTOR_PIN, HIGH);
    }
}

// ============================================================
// SAFETY STATE TEXT
// ============================================================
const char* safetyStateText()
{
    if (safetyState == SAFE_STATE)
    {
        return "SAFE";
    }

    if (safetyState == WARNING_STATE)
    {
        return "WARNING";
    }

    return "EMERGENCY";
}

// ============================================================
// HEART STATUS TEXT
// ============================================================
const char* heartSignalText()
{
    return qualityText();
}

bool heartBPMValid()
{
    return stableBPM > 0 &&
           lastValidBeatTime > 0 &&
           millis() - lastValidBeatTime <= BPM_TIMEOUT;
}

// ============================================================
// COMPLETE STATUS DISPLAY
// ============================================================
void printIntegratedStatus()
{
    Serial.println();
    Serial.println("==============================================");
    Serial.println("          SMART MINER HELMET STATUS");
    Serial.println("==============================================");

    Serial.print("SAFETY STATUS : ");
    Serial.println(safetyStateText());

    Serial.print("MQ-4 RAW      : ");
    Serial.println(mq4Raw, 1);

    Serial.print("MQ-4 Voltage  : ");
    Serial.print(mq4Voltage, 3);
    Serial.println(" V");

    Serial.print("MQ-4 Change   : ");
    Serial.print(mq4Change, 1);
    Serial.println("%");

    Serial.print("MQ-7 RAW      : ");
    Serial.println(mq7Raw, 1);

    Serial.print("MQ-7 Voltage  : ");
    Serial.print(mq7Voltage, 3);
    Serial.println(" V");

    Serial.print("MQ-7 Change   : ");
    Serial.print(mq7Change, 1);
    Serial.println("%");

    Serial.println("MQ-7 Status   : MONITOR ONLY");

    Serial.print("Temperature   : ");
    Serial.print(helmetTemperature, 1);
    Serial.println(" C");

    Serial.print("Pressure      : ");
    if (bmp180Present)
    {
        Serial.print(helmetPressure, 1);
        Serial.println(" hPa");
    }
    else
    {
        Serial.println("--");
    }

    Serial.print("Humidity      : ");
    Serial.print(helmetHumidity, 1);
    Serial.println(" %");


    Serial.print("Accel X       : ");
    Serial.print(accelX, 2);
    Serial.println(" g");

    Serial.print("Accel Y       : ");
    Serial.print(accelY, 2);
    Serial.println(" g");

    Serial.print("Accel Z       : ");
    Serial.print(accelZ, 2);
    Serial.println(" g");

    Serial.print("Accel Magnitude: ");
    Serial.print(accelMagnitude, 2);
    Serial.println(" g");

    Serial.print("Gyro X        : ");
    Serial.print(gyroX, 1);
    Serial.println(" deg/s");

    Serial.print("Gyro Y        : ");
    Serial.print(gyroY, 1);
    Serial.println(" deg/s");

    Serial.print("Gyro Z        : ");
    Serial.print(gyroZ, 1);
    Serial.println(" deg/s");

    Serial.print("Motion Status : ");
    Serial.println(motionText());

    Serial.print("Heart Contact : ");
    if (contactPresent)
    {
        Serial.println("YES");
    }
    else
    {
        Serial.println("NO");
    }

    Serial.print("Heart Signal  : ");
    Serial.println(heartSignalText());

    Serial.print("Heart Rate    : ");
    if (heartBPMValid())
    {
        Serial.print(stableBPM, 1);
        Serial.println(" BPM");
    }
    else
    {
        Serial.println("--");
    }

    Serial.print("SOS           : ");
    if (isSOSActive())
    {
        Serial.println("ACTIVE");
    }
    else
    {
        Serial.println("INACTIVE");
    }

    Serial.print("Rover Hazard  : ");
    Serial.println(roverHazardState == 2 ? "EMERGENCY" : (roverHazardState == 1 ? "WARNING" : "NORMAL"));
    Serial.print("Rover Cause   : ");
    Serial.println(roverHazardReason);

    Serial.println("==============================================");
}

// ============================================================
// ROVER-11.2 - STRUCTURED HELMET SENSOR PACKET
// ============================================================
// Helmet -> Rover ESP-NOW communication
// Rover MAC: B0:3F:D3:6F:5F:48
// Helmet MAC: 20:50:0D:8B:E6:0C
// ============================================================

const uint8_t ROVER_MAC_ADDRESS[] = {
    0xB0, 0x3F, 0xD3, 0x6F, 0x5F, 0x48
};

const char HELMET_MINER_ID[] = "MINER-01";

const unsigned long HELMET_PACKET_INTERVAL = 1000;
unsigned long lastHelmetPacket = 0;
uint32_t helmetPacketSequence = 0;

// Rover-11.4.1 SOS event tracking.
// Every new physical SOS press gets a unique event number.
uint32_t helmetSOSEvent = 0;

bool sosLastRawState = HIGH;
bool sosStableState = HIGH;
unsigned long sosLastChangeTime = 0;
const unsigned long SOS_DEBOUNCE_TIME = 40;

struct HelmetPacket
{
    uint32_t packetVersion;
    uint32_t sequence;
    uint32_t sosEvent;

    char minerID[16];

    float mq4Change;
    float mq7Change;

    float temperature;
    float humidity;
    float pressure;

    float heartRate;
    float spo2;

    uint8_t heartRateValid;
    uint8_t contactPresent;
    uint8_t fallDetected;
    uint8_t sosActive;
    uint8_t signalQuality;
};

// ============================================================
// ROVER-11.5 - APPLICATION ACK PACKET
// ============================================================
// This ACK is generated by the Rover after a valid HelmetPacket
// is actually received by the Rover application.
struct HelmetAckPacket
{
    uint32_t ackVersion;
    uint32_t ackedSequence;
    uint32_t roverReceivedPacketCount;
    uint8_t roverStatus;
};

volatile bool helmetAckReceived = false;
volatile uint32_t lastAckedSequence = 0;
volatile uint32_t roverAckPacketCount = 0;
unsigned long lastAckReceiveMillis = 0;
unsigned long ackReceivedCount = 0;
unsigned long ackTimeoutCount = 0;
unsigned long ackOutOfOrderCount = 0;
unsigned long lastAckWaitStartMillis = 0;
unsigned long lastAckRTT = 0;
uint32_t lastPacketSendSequence = 0;
bool waitingForAck = false;
const unsigned long ACK_TIMEOUT = 1500;

void onHelmetDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
        Serial.println("ESP-NOW packet: MAC ACK SUCCESS");
    else
        Serial.println("ESP-NOW packet: MAC ACK FAILED");
}

struct RoverCommandPacket
{
    uint32_t commandVersion;
    uint32_t sequence;
    char command[20];
    uint8_t hazardState;
    uint8_t buzzer;
    uint8_t vibration;
    uint8_t redLED;
    uint8_t yellowLED;
    uint8_t greenLED;
    char reason[48];
};

struct RoverWarningAckPacket
{
    uint32_t ackVersion;
    uint32_t commandSequence;
    uint8_t receivedState;
    char command[20];
    char reason[48];
};

const uint32_t ROVER_COMMAND_VERSION = 2;
const uint32_t ROVER_WARNING_ACK_VERSION = 1;
volatile bool roverHazardCommandPending = false;
volatile uint32_t pendingRoverCommandSequence = 0;
volatile uint8_t pendingRoverHazardState = 0;
char pendingRoverCommand[20] = "NORMAL";
char pendingRoverReason[48] = "No rover hazard";
uint32_t lastRoverHazardCommandSequence = 0;

void onHelmetDataReceive(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    if (memcmp(mac_addr, ROVER_MAC_ADDRESS, 6) != 0)
        return;

    if (len == sizeof(HelmetAckPacket))
    {
        HelmetAckPacket ack = {};
        memcpy(&ack, data, sizeof(ack));
        if (ack.ackVersion != 1) return;
        lastAckedSequence = ack.ackedSequence;
        roverAckPacketCount = ack.roverReceivedPacketCount;
        helmetAckReceived = true;
        return;
    }

    if (len == sizeof(RoverCommandPacket))
    {
        RoverCommandPacket cmd = {};
        memcpy(&cmd, data, sizeof(cmd));
        if (cmd.commandVersion != ROVER_COMMAND_VERSION) return;

        // Validate hazard state before accepting any Rover command.
        // 0 = NORMAL, 1 = WARNING, 2 = EMERGENCY.
        if (cmd.hazardState > 2) return;

        // Only the expected command names are accepted for the
        // environmental hazard channel. Manual test commands are
        // still supported through their corresponding hazardState.
        if (strcmp(cmd.command, "NORMAL") != 0 &&
            strcmp(cmd.command, "WARNING") != 0 &&
            strcmp(cmd.command, "EMERGENCY") != 0 &&
            strcmp(cmd.command, "SOS_ACK") != 0 &&
            strcmp(cmd.command, "BUZZER_ON") != 0 &&
            strcmp(cmd.command, "BUZZER_OFF") != 0 &&
            strcmp(cmd.command, "VIBRATION_ON") != 0 &&
            strcmp(cmd.command, "VIBRATION_OFF") != 0)
        {
            return;
        }

        if (cmd.sequence == lastRoverHazardCommandSequence) return;

        lastRoverHazardCommandSequence = cmd.sequence;
        pendingRoverCommandSequence = cmd.sequence;
        pendingRoverHazardState = cmd.hazardState;
        strncpy(pendingRoverCommand, cmd.command, sizeof(pendingRoverCommand)-1);
        pendingRoverCommand[sizeof(pendingRoverCommand)-1] = '\0';
        strncpy(pendingRoverReason, cmd.reason, sizeof(pendingRoverReason)-1);
        pendingRoverReason[sizeof(pendingRoverReason)-1] = '\0';
        roverHazardCommandPending = true;
    }
}

bool setupHelmetESPNow()
{
    Serial.println();
    Serial.println("==============================================");
    Serial.println("ROVER-11.2 ESP-NOW INITIALIZATION");
    Serial.println("==============================================");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.print("Helmet MAC Address: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ERROR: ESP-NOW initialization failed.");
        return false;
    }

    esp_now_register_send_cb(onHelmetDataSent);
    esp_now_register_recv_cb(onHelmetDataReceive);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, ROVER_MAC_ADDRESS, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("ERROR: Could not add Rover peer.");
        return false;
    }

    Serial.println("Rover peer added.");
    Serial.println("Structured helmet packet ready.");
    Serial.println("Packet interval: 1000 ms");
    Serial.println("==============================================");
    Serial.println();

    return true;
}

void sendHelmetSensorPacket()
{
    HelmetPacket packet = {};

    packet.packetVersion = 2;
    packet.sequence = ++helmetPacketSequence;
    packet.sosEvent = helmetSOSEvent;

    strncpy(packet.minerID, HELMET_MINER_ID, sizeof(packet.minerID) - 1);
    packet.minerID[sizeof(packet.minerID) - 1] = '\0';

    // Real values from the existing helmet sensor-processing code.
    packet.mq4Change = mq4Change;
    packet.mq7Change = mq7Change;

    packet.temperature = helmetTemperature;
    packet.humidity = helmetHumidity;
    packet.pressure = helmetPressure;

    packet.heartRate = heartBPMValid() ? stableBPM : -1.0f;

    // The current helmet program does not calculate SpO2 yet.
    // -1 means unavailable; it is NOT a measured SpO2 value.
    packet.spo2 = -1.0f;

    packet.heartRateValid = heartBPMValid() ? 1 : 0;
    packet.contactPresent = contactPresent ? 1 : 0;
    packet.fallDetected = fallDetected ? 1 : 0;
    packet.sosActive = isSOSActive() ? 1 : 0;
    packet.signalQuality = (uint8_t)signalQuality;

    unsigned long sendStart = millis();
    esp_err_t result = esp_now_send(
        ROVER_MAC_ADDRESS,
        (uint8_t *)&packet,
        sizeof(packet));

    if (result == ESP_OK)
    {
        lastPacketSendSequence = packet.sequence;
        lastAckWaitStartMillis = sendStart;
        waitingForAck = true;
    }

    Serial.println();
    Serial.println("---------- HELMET PACKET ----------");
    Serial.print("Miner ID       : ");
    Serial.println(packet.minerID);
    Serial.print("Sequence       : ");
    Serial.println(packet.sequence);
    Serial.print("SOS Event ID   : ");
    Serial.println(packet.sosEvent);
    Serial.print("MQ-4 Change    : ");
    Serial.print(packet.mq4Change, 1);
    Serial.println(" %");
    Serial.print("MQ-7 Change    : ");
    Serial.print(packet.mq7Change, 1);
    Serial.println(" %");
    Serial.print("Temperature    : ");
    Serial.print(packet.temperature, 1);
    Serial.println(" C");
    Serial.print("Humidity       : ");
    Serial.print(packet.humidity, 1);
    Serial.println(" %");
    Serial.print("Pressure       : ");
    Serial.print(packet.pressure, 1);
    Serial.println(" hPa");
    Serial.print("Heart Rate     : ");
    if (packet.heartRateValid)
        Serial.print(packet.heartRate, 1);
    else
        Serial.print("--");
    Serial.println(" BPM");
    Serial.println("SpO2           : UNAVAILABLE");
    Serial.print("Contact        : ");
    Serial.println(packet.contactPresent ? "YES" : "NO");
    Serial.print("Fall           : ");
    Serial.println(packet.fallDetected ? "DETECTED" : "NO");
    Serial.print("SOS            : ");
    Serial.println(packet.sosActive ? "ACTIVE" : "INACTIVE");
    Serial.print("Send result    : ");
    Serial.println(result == ESP_OK ? "QUEUED" : "ERROR");
    Serial.println("Application ACK : WAITING");
    Serial.println("-----------------------------------");
}


// ============================================================
// ROVER-11.5 - ACK + COMMUNICATION MONITORING
// ============================================================
void updateHelmetACKStatus()
{
    unsigned long now = millis();

    if (helmetAckReceived)
    {
        noInterrupts();
        uint32_t ackSeq = lastAckedSequence;
        uint32_t roverCount = roverAckPacketCount;
        helmetAckReceived = false;
        interrupts();

        ackReceivedCount++;
        lastAckReceiveMillis = now;

        if (waitingForAck && ackSeq == lastPacketSendSequence)
        {
            lastAckRTT = now - lastAckWaitStartMillis;
            waitingForAck = false;
        }
        else if (ackSeq != lastPacketSendSequence)
        {
            ackOutOfOrderCount++;
        }

        Serial.println();
        Serial.println("==============================================");
        Serial.println("          ROVER-11.6 APPLICATION ACK");
        Serial.println("==============================================");
        Serial.print("ACK for Sequence : ");
        Serial.println(ackSeq);
        Serial.print("Rover RX Count   : ");
        Serial.println(roverCount);
        Serial.print("ACK RTT          : ");
        Serial.print(lastAckRTT);
        Serial.println(" ms");
        Serial.println("ACK Status       : RECEIVED");
        Serial.println("Proof            : ROVER APPLICATION RECEIVED PACKET");
        Serial.println("==============================================");
    }

    if (waitingForAck && (now - lastAckWaitStartMillis >= ACK_TIMEOUT))
    {
        ackTimeoutCount++;
        waitingForAck = false;

        Serial.println();
        Serial.println("==============================================");
        Serial.println("          ROVER-11.5 ACK TIMEOUT");
        Serial.println("==============================================");
        Serial.print("Sequence waiting : ");
        Serial.println(lastPacketSendSequence);
        Serial.println("ACK Status       : TIMEOUT");
        Serial.println("==============================================");
    }

    static unsigned long lastSummary = 0;
    if (now - lastSummary >= 5000)
    {
        lastSummary = now;
        unsigned long attempts = ackReceivedCount + ackTimeoutCount;
        float rate = attempts > 0 ? 100.0f * ackReceivedCount / attempts : 0.0f;

        Serial.println();
        Serial.println("==============================================");
        Serial.println("       ROVER-11.6 COMMUNICATION METRICS");
        Serial.println("==============================================");
        Serial.print("Packets Sent      : ");
        Serial.println(helmetPacketSequence);
        Serial.print("ACK Received      : ");
        Serial.println(ackReceivedCount);
        Serial.print("ACK Timeout       : ");
        Serial.println(ackTimeoutCount);
        Serial.print("ACK Success Rate  : ");
        Serial.print(rate, 1);
        Serial.println(" %");
        Serial.print("Last ACK Sequence : ");
        Serial.println(lastAckedSequence);
        Serial.print("Last ACK RTT      : ");
        Serial.print(lastAckRTT);
        Serial.println(" ms");
        Serial.println("==============================================");
    }
}

// ============================================================
// ROVER-11.6 - PROCESS ROVER HAZARD COMMAND
// ============================================================
void processRoverHazardCommand()
{
    if (!roverHazardCommandPending) return;

    noInterrupts();
    uint32_t seq = pendingRoverCommandSequence;
    uint8_t state = pendingRoverHazardState;
    char command[20];
    char reason[48];
    strncpy(command, pendingRoverCommand, sizeof(command));
    strncpy(reason, pendingRoverReason, sizeof(reason));
    roverHazardCommandPending = false;
    interrupts();
    command[sizeof(command)-1] = '\0';
    reason[sizeof(reason)-1] = '\0';

    roverHazardState = state;
    strncpy(roverHazardReason, reason, sizeof(roverHazardReason)-1);
    roverHazardReason[sizeof(roverHazardReason)-1] = '\0';
    roverHazardCommandCount++;

    Serial.println();
    Serial.println("==============================================");
    Serial.println("       ROVER-11.6 HAZARD COMMAND");
    Serial.println("==============================================");
    Serial.print("Command Seq.  : "); Serial.println(seq);
    Serial.print("Command       : "); Serial.println(command);
    Serial.print("Hazard State  : ");
    Serial.println(state == 2 ? "EMERGENCY" : (state == 1 ? "WARNING" : "NORMAL"));
    Serial.print("Cause         : "); Serial.println(reason);
    Serial.print("Helmet action : ");
    Serial.println(state == 2 ? "RED + BUZZER + VIBRATION" : (state == 1 ? "YELLOW + BUZZER + VIBRATION" : "GREEN / NORMAL"));
    Serial.println("==============================================");
}

// ============================================================
// ROVER-11.6 - SEND HAZARD COMMAND ACK
// ============================================================
void sendRoverWarningACK()
{
    static uint32_t lastAckedCommand = 0;
    if (lastRoverHazardCommandSequence == 0 || lastRoverHazardCommandSequence == lastAckedCommand) return;

    RoverWarningAckPacket ack = {};
    ack.ackVersion = ROVER_WARNING_ACK_VERSION;
    ack.commandSequence = lastRoverHazardCommandSequence;
    ack.receivedState = roverHazardState;
    strncpy(ack.command, roverHazardState == 2 ? "EMERGENCY" : (roverHazardState == 1 ? "WARNING" : "NORMAL"), sizeof(ack.command)-1);
    strncpy(ack.reason, roverHazardReason, sizeof(ack.reason)-1);

    esp_err_t result = esp_now_send(ROVER_MAC_ADDRESS, (uint8_t *)&ack, sizeof(ack));
    if (result == ESP_OK) { lastAckedCommand = lastRoverHazardCommandSequence; roverHazardAckCount++; }

    Serial.println("----------------------------------------------");
    Serial.println("      ROVER-11.6 HAZARD COMMAND ACK");
    Serial.println("----------------------------------------------");
    Serial.print("ACK Command Seq : "); Serial.println(ack.commandSequence);
    Serial.print("ACK State       : "); Serial.println(ack.command);
    Serial.print("ACK Cause       : "); Serial.println(ack.reason);
    Serial.print("ACK Queue       : "); Serial.println(result == ESP_OK ? "SUCCESS" : "FAILED");
    Serial.println("----------------------------------------------");
}

// ============================================================
// STARTUP HARDWARE INFORMATION
// ============================================================
void printPinConfiguration()
{
    Serial.println("==============================================");
    Serial.println("SMART MINER HELMET PIN CONFIGURATION");
    Serial.println("==============================================");
    Serial.println("MQ-4 AO       -> GPIO34");
    Serial.println("MQ-7 AO       -> GPIO35");
    Serial.println("I2C SDA       -> GPIO21");
    Serial.println("I2C SCL       -> GPIO22");
    Serial.println("MAX30100      -> 0x57");
    Serial.println("MPU6050       -> 0x68");
    Serial.println("SOS Button    -> GPIO27");
    Serial.println("Buzzer        -> GPIO25");
    Serial.println("Red LED       -> GPIO26");
    Serial.println("Motor Driver  -> GPIO33");
    Serial.println("Yellow LED    -> GPIO12");
    Serial.println("Green LED     -> GPIO14");
    Serial.println("DHT11 DATA    -> GPIO4");
    Serial.println("BMP180        -> I2C SDA21/SCL22");
    Serial.println("==============================================");
    Serial.println();
}

// ============================================================
// SETUP
// ============================================================
// ============================================================
// ROVER-11.5 - SOS EVENT DETECTION
// ============================================================
// The normal Helmet packet is still sent every 1 second.
// A NEW SOS press is additionally transmitted immediately.
// This prevents a short button press from being missed between
// two normal 1-second packets.
// ============================================================
void processSOSEvent()
{
    bool rawState = isSOSActive();
    unsigned long now = millis();

    // Detect any raw transition and start/restart debounce timer.
    if (rawState != sosLastRawState)
    {
        sosLastRawState = rawState;
        sosLastChangeTime = now;
    }

    // Accept the new stable state after debounce time.
    if ((now - sosLastChangeTime) >= SOS_DEBOUNCE_TIME &&
        rawState != sosStableState)
    {
        sosStableState = rawState;

        // LOW = button pressed.
        if (sosStableState == LOW)
        {
            helmetSOSEvent++;

            Serial.println();
            Serial.println("==============================================");
            Serial.println("          NEW HELMET SOS EVENT");
            Serial.println("==============================================");
            Serial.print("SOS EVENT ID  : ");
            Serial.println(helmetSOSEvent);
            Serial.println("Priority      : HIGH");
            Serial.println("Sending SOS packet immediately...");
            Serial.println("==============================================");

            // Send immediately instead of waiting for the next
            // 1-second periodic packet.
            sendHelmetSensorPacket();
            lastHelmetPacket = now;
        }
        else
        {
            Serial.println("Helmet SOS button released.");
            Serial.println("Sending SOS RELEASE packet immediately...");

            // Send immediately so the Rover can turn its RED LED and
            // BUZZER OFF without waiting for the next 1-second packet.
            sendHelmetSensorPacket();
            lastHelmetPacket = now;
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println();
    Serial.println("==============================================");
    Serial.println("        SMART MINER HELMET SYSTEM - ROVER-11.6");
    Serial.println("==============================================");
    Serial.println("AI-POWERED COOPERATIVE MINE SAFETY PROTOTYPE");
    Serial.println();

    printPinConfiguration();

    // --------------------------------------------------------
    // ROVER-11.2 ESP-NOW SETUP
    // --------------------------------------------------------
    if (!setupHelmetESPNow())
    {
        Serial.println("WARNING: ESP-NOW unavailable. Helmet will continue locally.");
    }

    // --------------------------------------------------------
    // GPIO SETUP
    // --------------------------------------------------------
    pinMode(SOS_PIN, INPUT_PULLUP);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(MOTOR_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);

    allOutputsOff();

    // --------------------------------------------------------
    // ADC SETUP
    // --------------------------------------------------------
    analogReadResolution(12);
    analogSetPinAttenuation(MQ4_PIN, ADC_11db);
    analogSetPinAttenuation(MQ7_PIN, ADC_11db);

    // --------------------------------------------------------
    // I2C SETUP
    // --------------------------------------------------------
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    delay(100);

    scanI2C();

    // --------------------------------------------------------
    // BMP180 + DHT11 SETUP
    // --------------------------------------------------------
    setupEnvironmentSensors();

    // --------------------------------------------------------
    // MAX30100 V3 SETUP
    // --------------------------------------------------------
    Serial.println("==============================================");
    Serial.println("MAX30100 INITIALIZATION");
    Serial.println("==============================================");

    if (!setupMAX30100())
    {
        Serial.println("MAX30100 ERROR - CHECK WIRING");
        Serial.println("System will continue without heart data.");
    }
    else
    {
        Serial.println("MAX30100: OK");
    }

    Serial.println();

    // --------------------------------------------------------
    // MPU6050 SETUP
    // --------------------------------------------------------
    Serial.println("==============================================");
    Serial.println("MPU6050 INITIALIZATION");
    Serial.println("==============================================");

    mpuPresent = setupMPU6050();

    if (mpuPresent)
    {
        Serial.println("MPU6050: OK");
    }
    else
    {
        Serial.println("MPU6050: CHECK WIRING");
    }

    Serial.println();

    // --------------------------------------------------------
    // MQ CALIBRATION
    // --------------------------------------------------------
    Serial.println("==============================================");
    Serial.println("GAS SENSOR STARTUP");
    Serial.println("==============================================");
    Serial.println("IMPORTANT: Keep both MQ sensors in clean air.");
    Serial.println();

    delay(1000);

    calibrateMQ4();
    calibrateMQ7();

    // --------------------------------------------------------
    // RESET STATES
    // --------------------------------------------------------
    resetFallState();
    safetyState = SAFE_STATE;
    updateOutputs();

    Serial.println("==============================================");
    Serial.println("SYSTEM READY");
    Serial.println("==============================================");
    Serial.println("GREEN LED = SAFE");
    Serial.println("YELLOW LED = MQ-4 WARNING (+15% delta, confirmed 3x)");
    Serial.println("RED LED + BUZZER + MOTOR = EMERGENCY");
    Serial.println("MQ-7 = MONITOR ONLY");
    Serial.println("Rover hazards -> Helmet WARNING/EMERGENCY");
    Serial.println("Local SOS/FALL = highest priority");
    Serial.println("==============================================");
    Serial.println();
}

// ============================================================
// FORWARD DECLARATION
// ============================================================
void processMAX30100();

// ============================================================
// LOOP
// ============================================================
void loop()
{
    // ========================================================
    // ROVER-11.6 HAZARD COMMANDS
    // ========================================================
    processRoverHazardCommand();
    sendRoverWarningACK();

    // ========================================================
    // ROVER-11.5 SOS PRIORITY
    // ========================================================
    // Check the SOS button before the slower sensor processing.
    // A new press is sent to the Rover immediately.
    // ========================================================
    processSOSEvent();

    updateHelmetACKStatus();

    unsigned long now = millis();

    // --------------------------------------------------------
    // TEMPERATURE / PRESSURE / HUMIDITY
    // --------------------------------------------------------
    if (now - lastEnvironmentRead >= ENVIRONMENT_INTERVAL)
    {
        lastEnvironmentRead = now;
        updateEnvironmentSensors();
    }

    // --------------------------------------------------------
    // GAS SENSORS
    // --------------------------------------------------------
    if (now - lastGasRead >= GAS_INTERVAL)
    {
        lastGasRead = now;
        updateGasSensors();
    }

    // --------------------------------------------------------
    // MPU6050 / FALL DETECTION
    // --------------------------------------------------------
    if (now - lastMPURead >= MPU_INTERVAL)
    {
        lastMPURead = now;
        processFallDetection();
    }

    // --------------------------------------------------------
    // MAX30100 V3 PROCESSING
    // --------------------------------------------------------
    processMAX30100();

    // --------------------------------------------------------
    // SAFETY DECISION
    // --------------------------------------------------------
    determineSafetyState();

    // --------------------------------------------------------
    // OUTPUTS
    // --------------------------------------------------------
    updateOutputs();

    // --------------------------------------------------------
    // ROVER-11.5 STRUCTURED ESP-NOW PACKET
    // Normal packets continue every 1 second. SOS events are
    // transmitted immediately by processSOSEvent().
    // --------------------------------------------------------
    if (now - lastHelmetPacket >= HELMET_PACKET_INTERVAL)
    {
        lastHelmetPacket = now;
        sendHelmetSensorPacket();
    }

    // --------------------------------------------------------
    // COMPLETE SERIAL STATUS
    // --------------------------------------------------------
    if (now - lastIntegratedPrint >= 500)
    {
        lastIntegratedPrint = now;
        printIntegratedStatus();
    }

    delay(5);
}
// ============================================================
// MAX30100 INTEGRATED PROCESSING
// This is the original V3 signal-processing loop moved into a
// function so it can run together with the other helmet sensors.
// ============================================================
void processMAX30100()
{

    uint16_t ir;
    uint16_t red;


    // --------------------------------------------------------
    // Read sensor
    // --------------------------------------------------------

    if (!readFIFO(ir, red))
    {
        Serial.println("FIFO READ ERROR");

        delay(20);

        return;
    }


    unsigned long now = millis();


    // ========================================================
    // CONTACT DETECTION
    // ========================================================

    if (ir < CONTACT_THRESHOLD)
    {
        if (contactPresent)
        {
            Serial.println();
            Serial.println("==============================================");
            Serial.println("NO CONTACT");
            Serial.println("==============================================");

            contactPresent = false;

            signalQuality = NO_CONTACT;

            resetSignalProcessing();
        }

        delay(10);

        return;
    }


    // ========================================================
    // NEW CONTACT
    // ========================================================

    if (!contactPresent)
    {
        contactPresent = true;

        contactStartTime = now;

        resetSignalProcessing();

        Serial.println();
        Serial.println("==============================================");
        Serial.println("CONTACT DETECTED");
        Serial.println("==============================================");

        Serial.println("Stabilizing sensor for 5 seconds...");

        Serial.println();
    }


    // ========================================================
    // DC TRACKING
    // ========================================================

    if (!filterInitialized)
    {
        dcValue = ir;

        filterInitialized = true;
    }
    else
    {
        dcValue =
            0.97 * dcValue +
            0.03 * ir;
    }


    // AC component

    float acSignal =
        (float)ir - dcValue;


    // ========================================================
    // LOW PASS FILTER
    // ========================================================

    filteredSignal =
        0.85 * filteredSignal +
        0.15 * acSignal;


    // ========================================================
    // STABILIZATION
    // ========================================================

    if (now - contactStartTime < STABILIZATION_TIME)
    {
        signalQuality = POOR;

        Serial.print("STABILIZING  IR=");

        Serial.print(ir);

        Serial.print("  Signal=");

        Serial.println(filteredSignal, 1);


        previousPreviousSignal =
            previousSignal;

        previousSignal =
            filteredSignal;


        delay(10);

        return;
    }


    if (!stabilized)
    {
        stabilized = true;

        Serial.println();
        Serial.println("==============================================");
        Serial.println("SENSOR STABILIZED");
        Serial.println("Starting BPM detection...");
        Serial.println("==============================================");
        Serial.println();
    }


    // ========================================================
    // ARTIFACT DETECTION
    // ========================================================

    float signalChange =
        fabs(filteredSignal - previousSignal);


    if (signalChange > ARTIFACT_LIMIT)
    {
        signalQuality = POOR;

        Serial.println();
        Serial.println("!!! MOTION / SIGNAL ARTIFACT !!!");

        Serial.println("BPM temporarily invalid.");

        clearBPM();

        previousPreviousSignal =
            previousSignal;

        previousSignal =
            filteredSignal;

        delay(20);

        return;
    }


    // ========================================================
    // SIGNAL QUALITY
    // ========================================================

    calculateSignalQuality(
        fabs(filteredSignal));


    // ========================================================
    // HEARTBEAT DETECTION
    // ========================================================

    processBeat(
        filteredSignal,
        now);


    // ========================================================
    // BPM TIMEOUT
    // ========================================================

    if (lastValidBeatTime > 0 &&
        now - lastValidBeatTime > BPM_TIMEOUT)
    {
        stableBPM = 0;

        bpmHistoryCount = 0;

        validBeatCount = 0;
    }


    // ========================================================
    // UPDATE SIGNAL HISTORY
    // ========================================================

    previousPreviousSignal =
        previousSignal;

    previousSignal =
        filteredSignal;


    delay(10);

}

