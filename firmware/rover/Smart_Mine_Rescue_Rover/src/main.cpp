#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>

// ============================================================
// SMART MINE RESCUE ROVER
// ROVER-01 TO ROVER-11.7 FINAL FIXED
//
// Rover-01 : ESP32 basic operation
// Rover-02 : LEDs + Buzzer
// Rover-03 : MQ-4 Methane
// Rover-04 : MQ-7 CO
// Rover-05 : Water Sensor
// Rover-06 : MQ-2 Smoke Sensor
// Rover-08 : HC-SR04 Ultrasonic Distance Sensor
//
// ============================================================
//
// IMPORTANT:
//
// MQ-7 is MONITOR ONLY.
// Proper 60s HIGH / 90s LOW heater cycle is NOT implemented.
//
// MQ-4 is used for gas warning.
//
// Water detection produces:
// YELLOW LED + BUZZER
//
// Smoke detection produces:
// YELLOW LED + BUZZER
//
// MQ-2 smoke threshold is based on your actual test readings.
//
// ============================================================


// ============================================================
// PIN DEFINITIONS
// ============================================================

// ---------------- Gas Sensors ----------------

#define MQ4_PIN 34
#define MQ7_PIN 35

// ---------------- Water Sensor ----------------

#define WATER_SENSOR_PIN 32

// ---------------- Smoke Sensor ----------------

#define SMOKE_SENSOR_PIN 33

// ---------------- HC-SR04 Ultrasonic Sensor ----------------

#define TRIG_PIN 27
#define ECHO_PIN 16

// ---------------- LEDs ----------------

#define GREEN_LED 14
#define YELLOW_LED 12
#define RED_LED 26

// ---------------- Buzzer ----------------

#define BUZZER_PIN 25


// ============================================================
// MQ-4 SETTINGS
// ============================================================

#define MQ4_SAMPLES 10
#define MQ4_BASELINE_SAMPLES 100
#define MQ4_WARNING_PERCENT 15.0
#define MQ4_CLEAR_PERCENT 10.0
#define MQ4_CONFIRM_COUNT 3
#define MQ4_WARMUP_TIME_MS 60000UL
#define MQ4_ADC_MAX 4095
#define MQ4_SATURATION_LIMIT 4090


// ============================================================
// MQ-7 SETTINGS
// ============================================================

#define MQ7_SAMPLES 10
#define MQ7_BASELINE_SAMPLES 100

// MQ-7 monitor-only threshold
#define MQ7_WARNING_PERCENT 15.0


// ============================================================
// WATER SENSOR SETTINGS
// ============================================================

#define WATER_SAMPLES 10

// Based on your actual water sensor test:
//
// Dry:
// 0
//
// Wet:
// 1000 - 2200+
//
// Drying:
// 1479
// 1263
// 992
// 379
// 0
//
// Starting threshold:
#define WATER_THRESHOLD 500


// ============================================================
// MQ-2 SMOKE SENSOR SETTINGS
// ============================================================

#define SMOKE_SAMPLES 10

// Based on your actual MQ-2 test:
//
// Clean air:
// approximately 85 - 145
//
// Smoke:
// 500+
// 771
// 1071
// 1430
// etc.
//
// Therefore:
#define SMOKE_THRESHOLD 500

// Number of consecutive readings required
// before smoke is confirmed.
#define SMOKE_CONFIRM_COUNT 3


// ============================================================
// HC-SR04 SETTINGS
// ============================================================

#define ULTRASONIC_TIMEOUT 30000UL
#define ULTRASONIC_MIN_DISTANCE 2.0
#define ULTRASONIC_MAX_DISTANCE 400.0

// HC-SR04 distance status thresholds
// > 100 cm  = CLEAR
// 30 - 100 cm = OBJECT NEAR
// < 30 cm   = OBSTACLE / CLOSE
// < 10 cm   = VERY CLOSE

#define DISTANCE_CLEAR_THRESHOLD 100.0
#define DISTANCE_NEAR_THRESHOLD 30.0
#define DISTANCE_VERY_CLOSE_THRESHOLD 10.0



// ============================================================
// ROVER-11.4 : ROVER -> HELMET ESP-NOW COMMANDS
// ============================================================

// Helmet MAC from Rover-11.1 / Rover-11.2
uint8_t helmetMAC[] = {0x20, 0x50, 0x0D, 0x8B, 0xE6, 0x0C};

#define HELMET_PACKET_VERSION 2
#define ROVER_COMMAND_VERSION 2

// ------------------------------------------------------------
// HELMET SENSOR PACKET
// Must exactly match the Helmet-11.2 structure.
// ------------------------------------------------------------

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
struct HelmetAckPacket
{
    uint32_t ackVersion;
    uint32_t ackedSequence;
    uint32_t roverReceivedPacketCount;
    uint8_t roverStatus;
};

HelmetPacket helmetPacket;

volatile bool newHelmetPacket = false;
volatile bool helmetPacketValid = false;
volatile unsigned long lastHelmetPacketMillis = 0;

unsigned long receivedPacketCount = 0;

// ============================================================
// ROVER-11.5 - COMMUNICATION METRICS
// ============================================================
uint32_t lastHelmetSequence = 0;
unsigned long missedHelmetPackets = 0;
unsigned long duplicateHelmetPackets = 0;
unsigned long outOfOrderHelmetPackets = 0;
unsigned long ackPacketsQueued = 0;
unsigned long ackMacSuccess = 0;
unsigned long ackMacFailed = 0;
volatile bool helmetAckPending = false;
volatile uint32_t pendingAckSequence = 0;
volatile uint32_t pendingAckRxCount = 0;
const unsigned long COMMUNICATION_LOSS_TIMEOUT = 5000;

// Helmet emergency/SOS state.
// This is updated when a valid Helmet packet is received.
volatile bool helmetSOSActive = false;

// Rover-11.4.1 event-based SOS handling.
// A new SOS event is latched until the Rover receives an explicit
// local CLEAR_SOS command. This guarantees that even a short SOS
// press remains visible to the Rover.
volatile bool helmetSOSAlarmLatched = false;
volatile bool helmetSOSEventPending = false;
volatile uint32_t pendingHelmetSOSEvent = 0;

// Release handling: the Helmet sends an immediate packet when the
// SOS button is released. The Rover clears its SOS alarm from that
// release packet, but never clears a brand-new SOS event in the same
// packet.
volatile bool helmetSOSReleasePending = false;
volatile uint32_t pendingHelmetSOSReleaseEvent = 0;

uint32_t lastProcessedHelmetSOSEvent = 0;

// ------------------------------------------------------------
// ROVER -> HELMET COMMAND PACKET
// ------------------------------------------------------------

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

RoverCommandPacket roverCommand;

struct RoverWarningAckPacket
{
    uint32_t ackVersion;
    uint32_t commandSequence;
    uint8_t receivedState;
    char command[20];
    char reason[48];
};

const uint32_t ROVER_WARNING_ACK_VERSION = 1;
volatile bool roverWarningAckReceived = false;
volatile uint32_t roverWarningAckSequence = 0;
volatile uint8_t roverWarningAckState = 0;
char roverWarningAckCommand[20] = "";
char roverWarningAckReason[48] = "";
unsigned long roverWarningAckCount = 0;

uint8_t lastSentHazardState = 255;
char lastSentHazardReason[48] = "";

// ============================================================
// ROVER-11.7 - INTEGRATED RISK ENGINE
// ============================================================
// Prototype deterministic risk engine. It combines Helmet + Rover
// data into a single 0-100 score. It is designed so a future ML
// model can replace the scoring rules without changing the sensors.
// ============================================================
int overallRiskScore = 0;
uint8_t overallRiskLevel = 0; // 0 SAFE, 1 WARNING, 2 HIGH, 3 CRITICAL
uint8_t sensorConfidence = 0;
char overallRiskReason[96] = "System starting";
char sensorConfidenceReason[96] = "Waiting for Helmet data";
unsigned long lastRiskPrintMillis = 0;

// ============================================================
// ROVER-12.1 - WI-FI LAPTOP CONNECTION
// ============================================================
// The Rover creates its own Wi-Fi network. The laptop connects
// directly to this network; no router or Internet is required.
// ESP-NOW continues to use the Rover's STA interface for Helmet
// communication.
#define ROVER_WIFI_SSID     "MINE-ROVER"
#define ROVER_WIFI_PASSWORD "mine12345"
#define ROVER_WIFI_IP       "192.168.4.1"

WebServer roverWebServer(80);
bool laptopWebClientSeen = false;
unsigned long lastLaptopRequestMillis = 0;


#define RISK_SAFE_MAX       30
#define RISK_WARNING_MAX    60
#define RISK_HIGH_MAX       80
#define RISK_CRITICAL_MAX  100


uint32_t roverCommandSequence = 0;
volatile bool lastRoverSendWasApplicationACK = false;

// ------------------------------------------------------------
// ESP-NOW RECEIVE CALLBACK
// ------------------------------------------------------------
// Receives structured sensor packets from the Helmet.
// Uses the older callback format required by the current
// ESP32 Arduino framework.
// ------------------------------------------------------------

void onHelmetDataReceive(
    const uint8_t *mac_addr,
    const uint8_t *data,
    int len
)
{
    if (memcmp(mac_addr, helmetMAC, 6) != 0)
    {
        return;
    }

    if (len == sizeof(RoverWarningAckPacket))
    {
        RoverWarningAckPacket ack = {};
        memcpy(&ack, data, sizeof(ack));
        if (ack.ackVersion != ROVER_WARNING_ACK_VERSION) return;
        roverWarningAckSequence = ack.commandSequence;
        roverWarningAckState = ack.receivedState;
        strncpy(roverWarningAckCommand, ack.command, sizeof(roverWarningAckCommand)-1);
        roverWarningAckCommand[sizeof(roverWarningAckCommand)-1] = '\0';
        strncpy(roverWarningAckReason, ack.reason, sizeof(roverWarningAckReason)-1);
        roverWarningAckReason[sizeof(roverWarningAckReason)-1] = '\0';
        roverWarningAckReceived = true;
        return;
    }

    if (len != sizeof(HelmetPacket))
    {
        return;
    }

    memcpy(&helmetPacket, data, sizeof(HelmetPacket));

    if (helmetPacket.packetVersion != HELMET_PACKET_VERSION)
    {
        return;
    }

    helmetPacketValid = true;

    // Keep the current physical SOS state for display.
    helmetSOSActive = (helmetPacket.sosActive != 0);

    // --------------------------------------------------------
    // ROVER-11.5 EVENT + RELEASE HANDLING
    // --------------------------------------------------------
    // Every physical SOS press increments sosEvent on the Helmet.
    // A NEW event always turns the Rover alarm ON.
    // A later packet with the same event ID and sosActive=0 means
    // the Helmet button has been released, so the Rover alarm turns OFF.
    //
    // IMPORTANT: if a very fast press/release produces a packet where
    // the NEW event and sosActive=0 arrive together, we DO NOT clear
    // the newly triggered alarm in that same packet. A later periodic
    // packet will clear it. This prevents a short SOS event from being
    // accidentally cancelled.
    // --------------------------------------------------------
    bool newEventDetected = false;

    if (helmetPacket.sosEvent != lastProcessedHelmetSOSEvent)
    {
        lastProcessedHelmetSOSEvent = helmetPacket.sosEvent;
        pendingHelmetSOSEvent = helmetPacket.sosEvent;
        helmetSOSEventPending = true;
        helmetSOSAlarmLatched = true;
        newEventDetected = true;
    }

    // The Helmet sends an immediate packet when SOS is released.
    // Do not clear the alarm if this packet also introduced a brand-new
    // event; let the next packet handle the release in that edge case.
    if (!newEventDetected &&
        helmetPacket.sosActive == 0 &&
        helmetPacket.sosEvent == lastProcessedHelmetSOSEvent &&
        helmetSOSAlarmLatched)
    {
        pendingHelmetSOSReleaseEvent = helmetPacket.sosEvent;
        helmetSOSReleasePending = true;
    }

    // --------------------------------------------------------
    // ROVER-11.5 SEQUENCE / PACKET-LOSS MEASUREMENT
    // --------------------------------------------------------
    uint32_t seq = helmetPacket.sequence;

    if (lastHelmetSequence == 0)
        lastHelmetSequence = seq;
    else if (seq == lastHelmetSequence + 1)
        lastHelmetSequence = seq;
    else if (seq > lastHelmetSequence + 1)
    {
        missedHelmetPackets += (seq - lastHelmetSequence - 1);
        lastHelmetSequence = seq;
    }
    else if (seq == lastHelmetSequence)
        duplicateHelmetPackets++;
    else
        outOfOrderHelmetPackets++;

    // Queue the ACK; actual transmission occurs in loop().
    pendingAckSequence = seq;
    pendingAckRxCount = receivedPacketCount + 1;
    helmetAckPending = true;

    newHelmetPacket = true;
    lastHelmetPacketMillis = millis();
    receivedPacketCount++;
}

// ------------------------------------------------------------
// ROVER-11.5 - SEND APPLICATION ACK
// ------------------------------------------------------------
void sendHelmetApplicationACK()
{
    if (!helmetAckPending)
        return;

    noInterrupts();
    uint32_t seq = pendingAckSequence;
    uint32_t rxCount = pendingAckRxCount;
    helmetAckPending = false;
    interrupts();

    HelmetAckPacket ack = {};
    ack.ackVersion = 1;
    ack.ackedSequence = seq;
    ack.roverReceivedPacketCount = rxCount;
    ack.roverStatus = 1;

    lastRoverSendWasApplicationACK = true;

    esp_err_t result = esp_now_send(
        helmetMAC,
        (uint8_t *)&ack,
        sizeof(ack)
    );

    ackPacketsQueued++;

    Serial.println();
    Serial.println("----------------------------------------------");
    Serial.println("          ROVER-11.5 APPLICATION ACK");
    Serial.println("----------------------------------------------");
    Serial.print("ACK Sequence : ");
    Serial.println(seq);
    Serial.print("ACK Queue    : ");
    Serial.println(result == ESP_OK ? "SUCCESS" : "FAILED");
    Serial.println("----------------------------------------------");
}

// ------------------------------------------------------------
// PRINT HELMET MAC
// ------------------------------------------------------------

void printHelmetMAC()
{
    for (int i = 0; i < 6; i++)
    {
        if (helmetMAC[i] < 16)
        {
            Serial.print("0");
        }

        Serial.print(helmetMAC[i], HEX);

        if (i < 5)
        {
            Serial.print(":");
        }
    }
}

// ------------------------------------------------------------
// SEND COMMAND TO HELMET
// ------------------------------------------------------------

void sendRoverCommand(
    const char *command,
    bool buzzerState,
    bool vibrationState,
    bool redState,
    bool yellowState,
    bool greenState
)
{
    memset(&roverCommand, 0, sizeof(roverCommand));

    roverCommand.commandVersion = ROVER_COMMAND_VERSION;
    roverCommandSequence++;

    roverCommand.sequence = roverCommandSequence;

    strncpy(
        roverCommand.command,
        command,
        sizeof(roverCommand.command) - 1
    );

    // Preserve the Rover-11.4 manual command tests while using the
    // new structured hazard state.
    if (strcmp(command, "EMERGENCY") == 0 || strcmp(command, "SOS_ACK") == 0)
        roverCommand.hazardState = 2;
    else if (strcmp(command, "WARNING") == 0)
        roverCommand.hazardState = 1;
    else
        roverCommand.hazardState = 0;

    roverCommand.buzzer = buzzerState ? 1 : 0;
    roverCommand.vibration = vibrationState ? 1 : 0;
    roverCommand.redLED = redState ? 1 : 0;
    roverCommand.yellowLED = yellowState ? 1 : 0;
    roverCommand.greenLED = greenState ? 1 : 0;
    strncpy(roverCommand.reason, "Manual command", sizeof(roverCommand.reason)-1);

    lastRoverSendWasApplicationACK = false;

    esp_err_t result = esp_now_send(
        helmetMAC,
        (uint8_t *)&roverCommand,
        sizeof(roverCommand)
    );

    Serial.println();
    Serial.println("================================");
    Serial.println("       ROVER -> HELMET");
    Serial.println("       COMMAND SENT");
    Serial.println("================================");

    Serial.print("Command       : ");
    Serial.println(roverCommand.command);

    Serial.print("Command Seq.  : ");
    Serial.println(roverCommand.sequence);

    Serial.print("Buzzer        : ");
    Serial.println(
        roverCommand.buzzer ? "ON" : "OFF"
    );

    Serial.print("Vibration     : ");
    Serial.println(
        roverCommand.vibration ? "ON" : "OFF"
    );

    Serial.print("Red LED       : ");
    Serial.println(
        roverCommand.redLED ? "ON" : "OFF"
    );

    Serial.print("Yellow LED    : ");
    Serial.println(
        roverCommand.yellowLED ? "ON" : "OFF"
    );

    Serial.print("Green LED     : ");
    Serial.println(
        roverCommand.greenLED ? "ON" : "OFF"
    );

    Serial.print("Send result   : ");

    if (result == ESP_OK)
    {
        Serial.println("QUEUED");
    }
    else
    {
        Serial.print("FAILED, error = ");
        Serial.println(result);
    }

    Serial.println("================================");
}

// ============================================================
// ROVER-11.6 - ENVIRONMENTAL HAZARD STATE
// ============================================================
// ROVER-11.7 FIX: forward declarations for Rover sensor globals
// These variables are defined later in the file, but the hazard
// function is compiled before those definitions.
extern float mq4Change;
extern bool mq4Warning;
extern bool waterDetected;
extern bool smokeDetected;

uint8_t getEnvironmentalHazardState(char *reason, size_t reasonSize)
{
    bool gas = mq4Warning;
    bool water = waterDetected;
    bool smoke = smokeDetected;
    uint8_t count = (gas ? 1 : 0) + (water ? 1 : 0) + (smoke ? 1 : 0);
    reason[0] = '\0';
    if (count == 0)
    {
        strncpy(reason, "No rover hazard", reasonSize-1);
        reason[reasonSize-1] = '\0';
        return 0;
    }
    uint8_t state = (count >= 2) ? 2 : 1;
    bool first = true;
    if (gas) { strncat(reason, "MQ-4 gas increase", reasonSize-strlen(reason)-1); first=false; }
    if (water) { if(!first) strncat(reason, "; ", reasonSize-strlen(reason)-1); strncat(reason, "Water detected", reasonSize-strlen(reason)-1); first=false; }
    if (smoke) { if(!first) strncat(reason, "; ", reasonSize-strlen(reason)-1); strncat(reason, "Smoke detected", reasonSize-strlen(reason)-1); }
    return state;
}

// ============================================================
// ROVER-11.6 - SEND ENVIRONMENTAL HAZARD TO HELMET
// ============================================================
// ROVER-11.7 FIX: forward declaration for risk-level text
const char* getOverallRiskLevelText();

void sendEnvironmentalHazardIfChanged()
{
    // ROVER-11.7 FINAL FIXED:
    // Send the SAME overall safety decision to the Helmet.
    // This removes the old dual-controller behavior where Rover LEDs
    // followed the Risk Engine but Helmet commands followed only
    // environmental sensors.
    uint8_t state = 0;

    if (overallRiskLevel == 3)
        state = 2;
    else if (overallRiskLevel == 1 || overallRiskLevel == 2)
        state = 1;

    char reason[48];
    strncpy(reason, overallRiskReason, sizeof(reason) - 1);
    reason[sizeof(reason) - 1] = '\0';

    // Always prioritize explicit Helmet SOS/fall in the command sent back.
    if (helmetSOSAlarmLatched || helmetSOSActive)
    {
        state = 2;
        strncpy(reason, "Helmet SOS active", sizeof(reason) - 1);
        reason[sizeof(reason) - 1] = '\0';
    }
    else if (helmetPacketValid && helmetPacket.fallDetected)
    {
        state = 2;
        strncpy(reason, "Helmet fall detected", sizeof(reason) - 1);
        reason[sizeof(reason) - 1] = '\0';
    }

    if (state == lastSentHazardState && strcmp(reason, lastSentHazardReason) == 0)
        return;

    memset(&roverCommand, 0, sizeof(roverCommand));
    roverCommand.commandVersion = ROVER_COMMAND_VERSION;
    roverCommand.sequence = ++roverCommandSequence;

    const char *command =
        state == 2 ? "EMERGENCY" :
        (state == 1 ? "WARNING" : "NORMAL");

    strncpy(roverCommand.command, command, sizeof(roverCommand.command) - 1);
    roverCommand.hazardState = state;
    roverCommand.buzzer = state ? 1 : 0;
    roverCommand.vibration = state ? 1 : 0;
    roverCommand.redLED = state == 2 ? 1 : 0;
    roverCommand.yellowLED = state == 1 ? 1 : 0;
    roverCommand.greenLED = state == 0 ? 1 : 0;
    strncpy(roverCommand.reason, reason, sizeof(roverCommand.reason) - 1);

    esp_err_t result = esp_now_send(helmetMAC, (uint8_t *)&roverCommand, sizeof(roverCommand));

    if (result == ESP_OK)
    {
        lastSentHazardState = state;
        strncpy(lastSentHazardReason, reason, sizeof(lastSentHazardReason) - 1);
        lastSentHazardReason[sizeof(lastSentHazardReason) - 1] = '\0';
    }

    Serial.println();
    Serial.println("==============================================");
    Serial.println("      ROVER-11.7 RISK -> HELMET");
    Serial.println("==============================================");
    Serial.print("Command Seq.  : "); Serial.println(roverCommand.sequence);
    Serial.print("Command       : "); Serial.println(command);
    Serial.print("Risk Score    : "); Serial.println(overallRiskScore);
    Serial.print("Risk Level    : "); Serial.println(getOverallRiskLevelText());
    Serial.print("Cause         : "); Serial.println(reason);
    Serial.print("Helmet action : ");
    Serial.println(state == 2 ? "RED + BUZZER + VIBRATION" :
                   (state == 1 ? "YELLOW + BUZZER + VIBRATION" :
                                  "GREEN / NORMAL"));
    Serial.print("Send result   : "); Serial.println(result == ESP_OK ? "QUEUED" : "FAILED");
    Serial.println("==============================================");
}

void processRoverWarningACK()
{
    if (!roverWarningAckReceived) return;
    noInterrupts();
    uint32_t seq = roverWarningAckSequence;
    uint8_t state = roverWarningAckState;
    char command[20]; char reason[48];
    strncpy(command, roverWarningAckCommand, sizeof(command));
    strncpy(reason, roverWarningAckReason, sizeof(reason));
    roverWarningAckReceived = false;
    interrupts();
    command[sizeof(command)-1] = '\0'; reason[sizeof(reason)-1] = '\0';
    roverWarningAckCount++;
    Serial.println("----------------------------------------------");
    Serial.println("      HELMET HAZARD COMMAND ACK");
    Serial.println("----------------------------------------------");
    Serial.print("Command Seq.  : "); Serial.println(seq);
    Serial.print("Helmet state  : "); Serial.println(command);
    Serial.print("Helmet cause  : "); Serial.println(reason);
    Serial.println("Application ACK: RECEIVED");
    Serial.println("----------------------------------------------");
}

// ------------------------------------------------------------
// SERIAL COMMAND HANDLER
// ------------------------------------------------------------
// This gives us a simple way to test Rover -> Helmet.
// Open Rover Serial Monitor and type one of:
//
// NORMAL
// WARNING
// EMERGENCY
// SOS_ACK
// BUZZER_ON
// BUZZER_OFF
// VIBRATION_ON
// VIBRATION_OFF
//
// Press Enter after the command.
// ------------------------------------------------------------

void processRoverCommand()
{
    if (!Serial.available())
    {
        return;
    }

    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();

    if (input.length() == 0)
    {
        return;
    }

    Serial.print("Rover command received: ");
    Serial.println(input);

    if (input == "NORMAL")
    {
        // NORMAL is also a local Rover SOS clear command.
        helmetSOSAlarmLatched = false;
        helmetSOSEventPending = false;

        Serial.println("Local Rover SOS alarm cleared.");

        sendRoverCommand(
            "NORMAL",
            false,
            false,
            false,
            false,
            true
        );
    }
    else if (input == "CLEAR_SOS")
    {
        helmetSOSAlarmLatched = false;
        helmetSOSEventPending = false;

        Serial.println();
        Serial.println("================================");
        Serial.println("       ROVER SOS CLEARED");
        Serial.println("================================");
        Serial.println("Helmet SOS event history is kept.");
        Serial.println("A new SOS event ID will trigger again.");
        Serial.println("================================");
    }
    else if (input == "WARNING")
    {
        sendRoverCommand(
            "WARNING",
            true,
            true,
            false,
            true,
            false
        );
    }
    else if (input == "EMERGENCY")
    {
        sendRoverCommand(
            "EMERGENCY",
            true,
            true,
            true,
            false,
            false
        );
    }
    else if (input == "SOS_ACK")
    {
        sendRoverCommand(
            "SOS_ACK",
            true,
            true,
            true,
            false,
            false
        );
    }
    else if (input == "BUZZER_ON")
    {
        sendRoverCommand(
            "BUZZER_ON",
            true,
            false,
            false,
            false,
            false
        );
    }
    else if (input == "BUZZER_OFF")
    {
        sendRoverCommand(
            "BUZZER_OFF",
            false,
            false,
            false,
            false,
            false
        );
    }
    else if (input == "VIBRATION_ON")
    {
        sendRoverCommand(
            "VIBRATION_ON",
            false,
            true,
            false,
            false,
            false
        );
    }
    else if (input == "VIBRATION_OFF")
    {
        sendRoverCommand(
            "VIBRATION_OFF",
            false,
            false,
            false,
            false,
            false
        );
    }
    else
    {
        Serial.println();
        Serial.println("Unknown command.");
        Serial.println("Available commands:");
        Serial.println("  NORMAL");
        Serial.println("  WARNING");
        Serial.println("  EMERGENCY");
        Serial.println("  SOS_ACK");
        Serial.println("  BUZZER_ON");
        Serial.println("  BUZZER_OFF");
        Serial.println("  VIBRATION_ON");
        Serial.println("  VIBRATION_OFF");
        Serial.println("  CLEAR_SOS");
    }
}

// ------------------------------------------------------------
// ESP-NOW SEND CALLBACK
// ------------------------------------------------------------
// This confirms whether the command was acknowledged at the
// ESP-NOW MAC layer by the Helmet.
// ------------------------------------------------------------

void onRoverCommandSent(
    const uint8_t *mac_addr,
    esp_now_send_status_t status
)
{
    if (lastRoverSendWasApplicationACK)
    {
        if (status == ESP_NOW_SEND_SUCCESS)
            ackMacSuccess++;
        else
            ackMacFailed++;

        Serial.print("Rover -> Helmet APPLICATION ACK MAC: ");
        Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED");
        return;
    }

    Serial.print("Rover -> Helmet COMMAND MAC ACK: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED");
}

// ------------------------------------------------------------
// ESP-NOW SETUP
// ------------------------------------------------------------

void setupHelmetESPNow()
{
    Serial.println();
    Serial.println("================================");
    Serial.println("     ROVER-11.4 ESP-NOW");
    Serial.println(" HELMET DATA + COMMAND CHANNEL");
    Serial.println("================================");

    WiFi.mode(WIFI_AP_STA);

    Serial.print("Rover MAC Address: ");
    Serial.println(WiFi.macAddress());

    Serial.print("Helmet MAC Address: ");
    printHelmetMAC();
    Serial.println();

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ERROR: ESP-NOW initialization failed!");
        return;
    }

    esp_now_register_recv_cb(onHelmetDataReceive);
    esp_now_register_send_cb(onRoverCommandSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(
        peerInfo.peer_addr,
        helmetMAC,
        6
    );

    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_is_peer_exist(helmetMAC))
    {
        Serial.println("Helmet peer already exists.");
    }
    else
    {
        esp_err_t peerResult =
            esp_now_add_peer(&peerInfo);

        if (peerResult == ESP_OK)
        {
            Serial.println("Helmet peer added.");
        }
        else
        {
            Serial.print(
                "ERROR: Failed to add Helmet peer. Error = "
            );
            Serial.println(peerResult);
        }
    }

    Serial.println("ESP-NOW initialized.");
    Serial.println("Helmet receiver ready.");
    Serial.println("Rover command channel ready.");
    Serial.println("Application ACK channel ready.");
    Serial.println("Communication metrics ready.");

    Serial.println();
    Serial.println("Serial command test:");
    Serial.println("  NORMAL");
    Serial.println("  WARNING");
    Serial.println("  EMERGENCY");
    Serial.println("  SOS_ACK");
    Serial.println("  BUZZER_ON");
    Serial.println("  BUZZER_OFF");
    Serial.println("  VIBRATION_ON");
    Serial.println("  VIBRATION_OFF");
    Serial.println("  CLEAR_SOS");

    Serial.println("================================");
    Serial.println();
}

// ------------------------------------------------------------
// PRINT RECEIVED HELMET PACKET
// ------------------------------------------------------------

void printHelmetPacket()
{
    if (!newHelmetPacket)
    {
        return;
    }

    noInterrupts();
    HelmetPacket packetCopy = helmetPacket;
    newHelmetPacket = false;
    interrupts();

    Serial.println();
    Serial.println("================================");
    Serial.println("       HELMET DATA RECEIVED");
    Serial.println("================================");

    Serial.print("Miner ID       : ");
    Serial.println(packetCopy.minerID);

    Serial.print("Packet Version : ");
    Serial.println(packetCopy.packetVersion);

    Serial.print("Sequence       : ");
    Serial.println(packetCopy.sequence);

    Serial.print("SOS Event ID   : ");
    Serial.println(packetCopy.sosEvent);

    Serial.println("--------------------------------");

    Serial.print("Helmet MQ-4    : ");
    Serial.print(packetCopy.mq4Change, 1);
    Serial.println(" % change");

    Serial.print("Helmet MQ-7    : ");
    Serial.print(packetCopy.mq7Change, 1);
    Serial.println(" % change");

    Serial.print("Temperature    : ");
    Serial.print(packetCopy.temperature, 1);
    Serial.println(" C");

    Serial.print("Humidity       : ");
    Serial.print(packetCopy.humidity, 1);
    Serial.println(" %");

    Serial.print("Pressure       : ");
    Serial.print(packetCopy.pressure, 1);
    Serial.println(" hPa");

    Serial.print("Heart Rate     : ");
    if (packetCopy.heartRateValid &&
        packetCopy.heartRate >= 0.0f)
    {
        Serial.print(packetCopy.heartRate, 1);
        Serial.println(" BPM");
    }
    else
    {
        Serial.println("-- BPM");
    }

    Serial.print("SpO2           : ");
    if (packetCopy.spo2 >= 0.0f)
    {
        Serial.print(packetCopy.spo2, 1);
        Serial.println(" %");
    }
    else
    {
        Serial.println("UNAVAILABLE");
    }

    Serial.print("Contact        : ");
    Serial.println(
        packetCopy.contactPresent ? "YES" : "NO"
    );

    Serial.print("Fall           : ");
    Serial.println(
        packetCopy.fallDetected ? "YES" : "NO"
    );

    Serial.print("SOS            : ");
    Serial.println(
        packetCopy.sosActive ? "ACTIVE" : "INACTIVE"
    );

    Serial.print("SOS Event ID   : ");
    Serial.println(packetCopy.sosEvent);

    Serial.print("Signal Quality : ");
    Serial.println(packetCopy.signalQuality);

    Serial.println("--------------------------------");
    Serial.print("Packets Received: ");
    Serial.println(receivedPacketCount);

    Serial.println("Communication  : WORKING");
    Serial.println("================================");
}

// ------------------------------------------------------------
// ROVER-11.5 - COMMUNICATION METRICS
// ------------------------------------------------------------
void printCommunicationMetrics()
{
    static unsigned long lastPrint = 0;
    unsigned long now = millis();

    if (now - lastPrint < 5000)
        return;

    lastPrint = now;
    unsigned long totalObserved = receivedPacketCount + missedHelmetPackets;
    float delivery = totalObserved > 0
        ? 100.0f * receivedPacketCount / totalObserved
        : 0.0f;

    Serial.println();
    Serial.println("==============================================");
    Serial.println("       ROVER-11.5 COMMUNICATION METRICS");
    Serial.println("==============================================");
    Serial.print("Packets Received   : "); Serial.println(receivedPacketCount);
    Serial.print("Last Sequence      : "); Serial.println(lastHelmetSequence);
    Serial.print("Missed Packets     : "); Serial.println(missedHelmetPackets);
    Serial.print("Duplicate Packets  : "); Serial.println(duplicateHelmetPackets);
    Serial.print("Out-of-order       : "); Serial.println(outOfOrderHelmetPackets);
    Serial.print("ACKs Queued        : "); Serial.println(ackPacketsQueued);
    Serial.print("ACK MAC Success    : "); Serial.println(ackMacSuccess);
    Serial.print("ACK MAC Failed     : "); Serial.println(ackMacFailed);
    Serial.print("Packet Delivery    : "); Serial.print(delivery, 1); Serial.println(" %");

    unsigned long age = now - lastHelmetPacketMillis;
    Serial.print("Last Packet Age    : "); Serial.print(age); Serial.println(" ms");
    Serial.println(age <= COMMUNICATION_LOSS_TIMEOUT
        ? "Link Measurement   : GOOD / ACTIVE"
        : "Link Measurement   : TIMEOUT");
    Serial.println("==============================================");
}

// ------------------------------------------------------------
// COMMUNICATION STATUS
// ------------------------------------------------------------

void updateHelmetCommunicationStatus()
{
    // Rover-11.5 measures packet reception and application ACKs.
    // Automatic safety action will be added in Rover-11.6.

    unsigned long age =
        millis() - lastHelmetPacketMillis;

    if (!helmetPacketValid)
    {
        return;
    }

    if (age > 5000)
    {
        static unsigned long lastWarningPrint = 0;

        if (millis() - lastWarningPrint >= 2000)
        {
            Serial.println();
            Serial.println("================================");
            Serial.println("HELMET COMMUNICATION STATUS");
            Serial.println("Communication  : LOST");

            Serial.print("Last packet age: ");
            Serial.print(age);
            Serial.println(" ms");

            Serial.println(
                "Rover-11.6 will add safety action."
            );

            Serial.println("================================");

            lastWarningPrint = millis();
        }
    }
}


// ============================================================
// GLOBAL VARIABLES
// ============================================================

// ---------------- MQ-4 ----------------

float mq4Baseline = 0.0;
float mq4Raw = 0.0;
float mq4Change = 0.0;
bool mq4Ready = false;
bool mq4Warning = false;
uint8_t mq4ConfirmCount = 0;
bool mq4Saturated = false;


// ---------------- MQ-7 ----------------

float mq7Baseline = 0.0;
float mq7Raw = 0.0;
float mq7Change = 0.0;


// ---------------- Water ----------------

float waterRaw = 0.0;
bool waterDetected = false;


// ---------------- Smoke ----------------

float smokeRaw = 0.0;
bool smokeDetected = false;

int smokeConfirmCounter = 0;


// ---------------- HC-SR04 ----------------

float distanceCm = 0.0;
bool ultrasonicValid = false;


// ============================================================
// READ MQ-4
// ============================================================

float readMQ4()
{
    long total = 0;

    for (int i = 0; i < MQ4_SAMPLES; i++)
    {
        int value = analogRead(MQ4_PIN);
        value = constrain(value, 0, MQ4_ADC_MAX);
        total += value;
        delay(10);
    }

    float average = (float)total / MQ4_SAMPLES;
    mq4Saturated = (average >= MQ4_SATURATION_LIMIT);
    return average;
}


// ============================================================
// READ MQ-7
// ============================================================

float readMQ7()
{
    long total = 0;

    for (int i = 0; i < MQ7_SAMPLES; i++)
    {
        total += analogRead(MQ7_PIN);

        delay(10);
    }

    return (float)total / MQ7_SAMPLES;
}


// ============================================================
// READ WATER SENSOR
// ============================================================

float readWaterSensor()
{
    long total = 0;

    for (int i = 0; i < WATER_SAMPLES; i++)
    {
        total += analogRead(WATER_SENSOR_PIN);

        delay(5);
    }

    return (float)total / WATER_SAMPLES;
}


// ============================================================
// READ MQ-2 SMOKE SENSOR
// ============================================================

float readSmokeSensor()
{
    long total = 0;

    for (int i = 0; i < SMOKE_SAMPLES; i++)
    {
        total += analogRead(SMOKE_SENSOR_PIN);

        delay(5);
    }

    return (float)total / SMOKE_SAMPLES;
}


// ============================================================
// READ HC-SR04 ULTRASONIC SENSOR
// ============================================================

float readUltrasonicDistance()
{
    // Make sure trigger starts LOW
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    // Send 10 microsecond trigger pulse
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Measure ECHO HIGH time
    unsigned long duration =
        pulseIn(ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT);

    // No echo received
    if (duration == 0)
    {
        ultrasonicValid = false;
        return 0.0;
    }

    // Convert microseconds to centimeters
    float distance =
        (duration * 0.0343) / 2.0;

    // Reject physically invalid readings
    if (distance < ULTRASONIC_MIN_DISTANCE ||
        distance > ULTRASONIC_MAX_DISTANCE)
    {
        ultrasonicValid = false;
        return 0.0;
    }

    ultrasonicValid = true;
    return distance;
}


// ============================================================
// CALCULATE GAS CHANGE
// ============================================================

float calculateChange(
    float currentValue,
    float baselineValue
)
{
    if (baselineValue <= 0.1)
    {
        return 0.0;
    }

    return (
        (currentValue - baselineValue)
        / baselineValue
    ) * 100.0;
}


// ============================================================
// MQ-4 WARNING PROCESSOR
// ============================================================

void updateMQ4Warning()
{
    if (!mq4Ready)
    {
        mq4Warning = false;
        mq4ConfirmCount = 0;
        return;
    }

    if (mq4Saturated)
    {
        mq4Warning = false;
        mq4ConfirmCount = 0;
        return;
    }

    if (mq4Change >= MQ4_WARNING_PERCENT)
    {
        if (mq4ConfirmCount < MQ4_CONFIRM_COUNT)
            mq4ConfirmCount++;

        if (mq4ConfirmCount >= MQ4_CONFIRM_COUNT)
            mq4Warning = true;
    }
    else
    {
        mq4ConfirmCount = 0;
        if (mq4Change <= MQ4_CLEAR_PERCENT)
            mq4Warning = false;
    }
}


// ============================================================
// TURN ALL LEDs OFF
// ============================================================

void allLEDsOff()
{
    digitalWrite(GREEN_LED, LOW);

    digitalWrite(YELLOW_LED, LOW);

    digitalWrite(RED_LED, LOW);
}


// ============================================================
// SAFE STATE
// ============================================================

void showSafe()
{
    allLEDsOff();

    // Green LED ON
    digitalWrite(GREEN_LED, HIGH);

    // Buzzer OFF
    digitalWrite(BUZZER_PIN, LOW);
}


// ============================================================
// WARNING STATE
// ============================================================

void showWarning()
{
    allLEDsOff();

    // Yellow LED ON
    digitalWrite(YELLOW_LED, HIGH);

    // Buzzer ON
    digitalWrite(BUZZER_PIN, HIGH);
}


// ============================================================
// EMERGENCY STATE
// ============================================================

void showEmergency()
{
    allLEDsOff();

    // Red LED ON
    digitalWrite(RED_LED, HIGH);

    // Buzzer ON
    digitalWrite(BUZZER_PIN, HIGH);
}


// ============================================================
// MQ-4 CALIBRATION
// ============================================================

void calibrateMQ4()
{
    Serial.println();
    Serial.println("================================");
    Serial.println("       MQ-4 STARTUP");
    Serial.println("================================");
    Serial.println("Keep MQ-4 in clean air.");
    Serial.println("60-second warm-up before baseline.");

    mq4Ready = false;
    mq4Warning = false;
    mq4ConfirmCount = 0;
    mq4Saturated = false;

    unsigned long startTime = millis();
    unsigned long lastPrint = 0;

    while (millis() - startTime < MQ4_WARMUP_TIME_MS)
    {
        if (millis() - lastPrint >= 1000)
        {
            lastPrint = millis();
            unsigned long elapsed = millis() - startTime;
            unsigned long remaining = (MQ4_WARMUP_TIME_MS - elapsed) / 1000;
            int raw = analogRead(MQ4_PIN);
            Serial.print("MQ-4 warm-up: ");
            Serial.print(remaining);
            Serial.print(" sec remaining | RAW = ");
            Serial.println(raw);
        }
        delay(50);
    }

    Serial.println();
    Serial.println("MQ-4 warm-up complete.");
    Serial.println("Collecting clean-air baseline...");

    long total = 0;
    int minimumValue = MQ4_ADC_MAX;
    int maximumValue = 0;

    for (int i = 0; i < MQ4_BASELINE_SAMPLES; i++)
    {
        int value = analogRead(MQ4_PIN);
        value = constrain(value, 0, MQ4_ADC_MAX);
        total += value;
        if (value < minimumValue) minimumValue = value;
        if (value > maximumValue) maximumValue = value;

        if ((i + 1) % 10 == 0) Serial.print(".");
        delay(100);
    }

    mq4Baseline = (float)total / MQ4_BASELINE_SAMPLES;

    Serial.println();
    Serial.print("MQ-4 BASELINE = "); Serial.println(mq4Baseline, 1);
    Serial.print("MQ-4 MINIMUM  = "); Serial.println(minimumValue);
    Serial.print("MQ-4 MAXIMUM  = "); Serial.println(maximumValue);

    if (mq4Baseline >= MQ4_SATURATION_LIMIT)
    {
        mq4Ready = false;
        Serial.println("MQ-4 STATUS   = SENSOR SATURATED / NOT READY");
        Serial.println("Check MQ-4 AO voltage and 10k/10k divider.");
    }
    else
    {
        mq4Ready = true;
        Serial.println("MQ-4 STATUS   = ACTIVE");
        Serial.println("MQ-4 calibration complete.");
    }
    Serial.println();
}

// ============================================================
// MQ-7 BASELINE
// ============================================================

void calibrateMQ7()
{
    Serial.println();

    Serial.println("================================");
    Serial.println("      MQ-7 BASELINE");
    Serial.println("================================");

    Serial.println(
        "Reading initial MQ-7 value..."
    );

    Serial.println(
        "MQ-7 is monitor-only at this stage."
    );

    delay(3000);

    long total = 0;

    for (int i = 0;
         i < MQ7_BASELINE_SAMPLES;
         i++)
    {
        int value = analogRead(MQ7_PIN);

        total += value;

        if ((i + 1) % 10 == 0)
        {
            Serial.print(".");
        }

        delay(50);
    }

    mq7Baseline =
        (float)total / MQ7_BASELINE_SAMPLES;

    Serial.println();

    Serial.print("MQ-7 BASELINE = ");

    Serial.println(mq7Baseline);

    Serial.println(
        "MQ-7 baseline complete."
    );

    Serial.println();
}


// ============================================================
// SMOKE DETECTION
// ============================================================

void updateSmokeDetection()
{
    // --------------------------------------------------------
    // If smoke reading is above threshold
    // --------------------------------------------------------

    if (smokeRaw > SMOKE_THRESHOLD)
    {
        smokeConfirmCounter++;

        // Limit counter
        if (smokeConfirmCounter >
            SMOKE_CONFIRM_COUNT)
        {
            smokeConfirmCounter =
                SMOKE_CONFIRM_COUNT;
        }
    }

    // --------------------------------------------------------
    // If reading goes below threshold
    // --------------------------------------------------------

    else
    {
        smokeConfirmCounter = 0;

        smokeDetected = false;
    }


    // --------------------------------------------------------
    // Confirm smoke after consecutive readings
    // --------------------------------------------------------

    if (smokeConfirmCounter >=
        SMOKE_CONFIRM_COUNT)
    {
        smokeDetected = true;
    }
}


// ============================================================
// HC-SR04 DISTANCE STATUS
// ============================================================

const char* getDistanceStatus()
{
    if (!ultrasonicValid)
    {
        return "NO RESPONSE";
    }

    if (distanceCm < DISTANCE_VERY_CLOSE_THRESHOLD)
    {
        return "VERY CLOSE";
    }

    if (distanceCm < DISTANCE_NEAR_THRESHOLD)
    {
        return "OBSTACLE / CLOSE";
    }

    if (distanceCm <= DISTANCE_CLEAR_THRESHOLD)
    {
        return "OBJECT NEAR";
    }

    return "CLEAR";
}


// ============================================================
// PRINT SENSOR DATA
// ============================================================

void printSensorData()
{
    Serial.println();

    Serial.println("--------------------------------");

    Serial.println(
        "        ROVER SENSOR DATA"
    );

    Serial.println("--------------------------------");


    // ========================================================
    // MQ-4
    // ========================================================

    Serial.print("MQ-4 RAW       = ");

    Serial.println(mq4Raw, 1);

    Serial.print("MQ-4 CHANGE    = ");

    Serial.print(mq4Change, 1);

    Serial.println(" %");


    Serial.print("MQ-4 STATUS    = ");
    if (!mq4Ready) Serial.println("NOT READY");
    else if (mq4Saturated) Serial.println("ADC SATURATED");
    else if (mq4Warning) Serial.println("WARNING");
    else Serial.println("SAFE");

    Serial.print("MQ-4 CONFIRM   = ");
    Serial.print(mq4ConfirmCount);
    Serial.print("/");
    Serial.println(MQ4_CONFIRM_COUNT);


    // ========================================================
    // MQ-7
    // ========================================================

    Serial.print("MQ-7 RAW       = ");

    Serial.println(mq7Raw, 1);

    Serial.print("MQ-7 CHANGE    = ");

    Serial.print(mq7Change, 1);

    Serial.println(" %");

    Serial.println(
        "MQ-7 STATUS    = MONITOR ONLY"
    );


    // ========================================================
    // WATER
    // ========================================================

    Serial.print("WATER RAW      = ");

    Serial.println(waterRaw, 1);


    if (waterDetected)
    {
        Serial.println(
            "WATER STATUS   = DETECTED"
        );
    }
    else
    {
        Serial.println(
            "WATER STATUS   = SAFE / DRY"
        );
    }


    // ========================================================
    // SMOKE
    // ========================================================

    Serial.print("SMOKE RAW      = ");

    Serial.println(smokeRaw, 1);


    if (smokeDetected)
    {
        Serial.println(
            "SMOKE STATUS   = DETECTED"
        );
    }
    else
    {
        Serial.println(
            "SMOKE STATUS   = SAFE"
        );
    }


    // ========================================================
    // SMOKE CONFIRMATION
    // ========================================================

    Serial.print(
        "SMOKE CONFIRM  = "
    );

    Serial.print(smokeConfirmCounter);

    Serial.print("/");

    Serial.println(SMOKE_CONFIRM_COUNT);


    // ========================================================
    // HC-SR04 DISTANCE
    // ========================================================

    if (ultrasonicValid)
    {
        Serial.print("DISTANCE       = ");
        Serial.print(distanceCm, 2);
        Serial.println(" cm");

        Serial.print("DISTANCE STATUS= ");
        Serial.println(getDistanceStatus());
    }
    else
    {
        Serial.println("DISTANCE       = NO RESPONSE");
        Serial.println("DISTANCE STATUS= NO RESPONSE");
    }


    Serial.println("--------------------------------");
}


// ============================================================
// ROVER-11.5 - PROCESS SOS EVENT + RELEASE
// ============================================================
// ESP-NOW callback only records state/event information. GPIO and
// Serial work are performed here in the normal loop context.
// This function runs BEFORE the slower sensor reads.
// ============================================================
void processHelmetSOSEvent()
{
    // --------------------------------------------------------
    // NEW SOS EVENT
    // --------------------------------------------------------
    if (helmetSOSEventPending)
    {
        noInterrupts();
        uint32_t eventID = pendingHelmetSOSEvent;
        helmetSOSEventPending = false;
        interrupts();

        Serial.println();
        Serial.println("==============================================");
        Serial.println("        !!! HELMET SOS EVENT !!!");
        Serial.println("==============================================");
        Serial.print("SOS EVENT ID : ");
        Serial.println(eventID);
        Serial.println("Rover action : IMMEDIATE EMERGENCY");
        Serial.println("RED LED      : ON");
        Serial.println("BUZZER       : ON");
        Serial.println("Rover vibration: NOT USED");
        Serial.println("Alarm mode   : ACTIVE WHILE HELMET SOS ACTIVE");
        Serial.println("==============================================");

        // Immediate output action. It is NOT permanently latched.
        showEmergency();
    }

    // --------------------------------------------------------
    // SOS RELEASE
    // --------------------------------------------------------
    if (helmetSOSReleasePending)
    {
        noInterrupts();
        uint32_t releaseEventID = pendingHelmetSOSReleaseEvent;
        helmetSOSReleasePending = false;
        uint32_t currentEventID = lastProcessedHelmetSOSEvent;
        interrupts();

        // Only clear if the release belongs to the latest known event.
        if (releaseEventID == currentEventID && !helmetSOSActive)
        {
            helmetSOSAlarmLatched = false;

            Serial.println();
            Serial.println("==============================================");
            Serial.println("        HELMET SOS RELEASED");
            Serial.println("==============================================");
            Serial.print("SOS EVENT ID : ");
            Serial.println(releaseEventID);
            Serial.println("Rover action : CLEAR HELMET SOS ALARM");
            Serial.println("RED LED      : OFF");
            Serial.println("BUZZER       : OFF");
            Serial.println("==============================================");
        }
    }
}

// ============================================================
// DETERMINE SAFETY STATE
// ============================================================

// ============================================================
// ROVER-11.7 - RISK ENGINE FUNCTIONS
// ============================================================

const char* getOverallRiskLevelText()
{
    switch (overallRiskLevel)
    {
        case 0: return "SAFE";
        case 1: return "WARNING";
        case 2: return "HIGH RISK";
        default: return "CRITICAL";
    }
}

void addRiskReason(char *reason, size_t reasonSize, const char *text)
{
    if (reason[0] != '\0')
        strncat(reason, "; ", reasonSize - strlen(reason) - 1);
    strncat(reason, text, reasonSize - strlen(reason) - 1);
}

uint8_t calculateSensorConfidence()
{
    if (!helmetPacketValid)
    {
        strncpy(sensorConfidenceReason, "Helmet data unavailable", sizeof(sensorConfidenceReason) - 1);
        sensorConfidenceReason[sizeof(sensorConfidenceReason) - 1] = '\0';
        return 0;
    }

    unsigned long age = millis() - lastHelmetPacketMillis;
    if (age > COMMUNICATION_LOSS_TIMEOUT)
    {
        strncpy(sensorConfidenceReason, "Helmet link lost / last known data", sizeof(sensorConfidenceReason) - 1);
        sensorConfidenceReason[sizeof(sensorConfidenceReason) - 1] = '\0';
        return 50;
    }

    bool roverGas = mq4Warning;
    bool helmetGas = helmetPacket.mq4Change >= MQ4_WARNING_PERCENT;

    if (roverGas && helmetGas)
    {
        strncpy(sensorConfidenceReason, "Gas sensors agree - high confidence", sizeof(sensorConfidenceReason) - 1);
        sensorConfidenceReason[sizeof(sensorConfidenceReason) - 1] = '\0';
        return 100;
    }

    if (roverGas != helmetGas)
    {
        strncpy(sensorConfidenceReason, "Helmet/Rover gas disagreement", sizeof(sensorConfidenceReason) - 1);
        sensorConfidenceReason[sizeof(sensorConfidenceReason) - 1] = '\0';
        return 70;
    }

    strncpy(sensorConfidenceReason, "Helmet + Rover data consistent", sizeof(sensorConfidenceReason) - 1);
    sensorConfidenceReason[sizeof(sensorConfidenceReason) - 1] = '\0';
    return 95;
}

int calculateOverallRiskScore()
{
    int score = 0;
    char reason[96] = "";

    // Miner SOS always overrides every other condition.
    if (helmetSOSAlarmLatched || helmetSOSActive)
    {
        strncpy(overallRiskReason, "Helmet SOS active", sizeof(overallRiskReason) - 1);
        overallRiskReason[sizeof(overallRiskReason) - 1] = '\0';
        return 100;
    }

    // Helmet fall is a direct personal-safety emergency.
    if (helmetPacketValid && helmetPacket.fallDetected)
    {
        strncpy(overallRiskReason, "Helmet fall detected", sizeof(overallRiskReason) - 1);
        overallRiskReason[sizeof(overallRiskReason) - 1] = '\0';
        return 100;
    }

    // Rover environmental hazards.
    bool roverGas = mq4Warning;
    bool water = waterDetected;
    bool smoke = smokeDetected;

    if (roverGas)
    {
        score += 35;
        addRiskReason(reason, sizeof(reason), "Rover MQ-4 warning");
    }

    if (water)
    {
        score += 35;
        addRiskReason(reason, sizeof(reason), "Water detected");
    }

    if (smoke)
    {
        score += 40;
        addRiskReason(reason, sizeof(reason), "Smoke detected");
    }

    // Helmet gas provides redundant evidence.
    bool helmetGas = helmetPacketValid &&
                     (helmetPacket.mq4Change >= MQ4_WARNING_PERCENT);

    if (helmetGas)
    {
        score += 25;
        addRiskReason(reason, sizeof(reason), "Helmet MQ-4 warning");
    }

    if (roverGas && helmetGas)
    {
        score += 10;
        addRiskReason(reason, sizeof(reason), "Helmet/Rover gas agreement");
    }

    // Heart-rate contribution only when the current prototype says it is valid.
    if (helmetPacketValid && helmetPacket.heartRateValid &&
        helmetPacket.heartRate >= 0.0f)
    {
        if (helmetPacket.heartRate < 50.0f || helmetPacket.heartRate > 120.0f)
        {
            score += 10;
            addRiskReason(reason, sizeof(reason), "Abnormal heart rate");
        }
    }

    // SpO2 is currently unavailable in the Helmet firmware, so do not invent a value.

    // Communication health.
    if (!helmetPacketValid)
    {
        score += 20;
        addRiskReason(reason, sizeof(reason), "Helmet data unavailable");
    }
    else if ((millis() - lastHelmetPacketMillis) > COMMUNICATION_LOSS_TIMEOUT)
    {
        score += 25;
        addRiskReason(reason, sizeof(reason), "Helmet communication lost");
    }

    // Prototype-only temperature contribution; not a certified mine criterion.
    if (helmetPacketValid && helmetPacket.temperature > 45.0f)
    {
        score += 15;
        addRiskReason(reason, sizeof(reason), "High helmet temperature");
    }

    if (reason[0] == '\0')
        strncpy(reason, "No active risk condition", sizeof(reason) - 1);

    reason[sizeof(reason) - 1] = '\0';
    strncpy(overallRiskReason, reason, sizeof(overallRiskReason) - 1);
    overallRiskReason[sizeof(overallRiskReason) - 1] = '\0';

    if (score > RISK_CRITICAL_MAX) score = RISK_CRITICAL_MAX;
    return score;
}

void updateRiskEngine()
{
    overallRiskScore = calculateOverallRiskScore();
    sensorConfidence = calculateSensorConfidence();

    if (overallRiskScore <= RISK_SAFE_MAX)
        overallRiskLevel = 0;
    else if (overallRiskScore <= RISK_WARNING_MAX)
        overallRiskLevel = 1;
    else if (overallRiskScore <= RISK_HIGH_MAX)
        overallRiskLevel = 2;
    else
        overallRiskLevel = 3;

    if (helmetSOSAlarmLatched || helmetSOSActive)
    {
        overallRiskScore = 100;
        overallRiskLevel = 3;
    }

    unsigned long now = millis();
    if (now - lastRiskPrintMillis < 3000)
        return;

    lastRiskPrintMillis = now;

    bool roverGas = mq4Warning;
    bool water = waterDetected;
    bool smoke = smokeDetected;

    Serial.println();
    Serial.println("==============================================");
    Serial.println("          ROVER-11.7 RISK ENGINE");
    Serial.println("==============================================");
    Serial.print("Miner ID          : ");
    if (helmetPacketValid) Serial.println(helmetPacket.minerID);
    else Serial.println("UNKNOWN");

    Serial.println("HELMET STATUS");
    if (helmetPacketValid)
    {
        Serial.print("  MQ-4            : "); Serial.print(helmetPacket.mq4Change, 1); Serial.println(" % change");
        Serial.print("  Fall            : "); Serial.println(helmetPacket.fallDetected ? "DETECTED" : "NO");
        Serial.print("  SOS             : "); Serial.println(helmetPacket.sosActive ? "ACTIVE" : "INACTIVE");
        Serial.print("  Heart Rate      : ");
        if (helmetPacket.heartRateValid) { Serial.print(helmetPacket.heartRate, 1); Serial.println(" BPM"); }
        else Serial.println("--");
    }
    else
    {
        Serial.println("  Data            : UNAVAILABLE");
    }

    Serial.println("ROVER ENVIRONMENT");
    Serial.print("  MQ-4            : "); Serial.println(roverGas ? "WARNING" : "SAFE");
    Serial.print("  MQ-4 Change     : "); Serial.print(mq4Change, 1); Serial.println(" %");
    Serial.print("  MQ-4 Confirm    : "); Serial.print(mq4ConfirmCount); Serial.print("/"); Serial.println(MQ4_CONFIRM_COUNT);
    Serial.print("  Water           : "); Serial.println(water ? "DETECTED" : "CLEAR");
    Serial.print("  Smoke           : "); Serial.println(smoke ? "DETECTED" : "CLEAR");
    Serial.println("  MQ-7 CO         : MONITOR ONLY");

    Serial.println("COMMUNICATION");
    if (helmetPacketValid)
    {
        unsigned long age = millis() - lastHelmetPacketMillis;
        Serial.print("  Helmet Link     : "); Serial.println(age <= COMMUNICATION_LOSS_TIMEOUT ? "CONNECTED" : "LOST");
        Serial.print("  Last Packet Age : "); Serial.print(age); Serial.println(" ms");
    }
    else
    {
        Serial.println("  Helmet Link     : NOT ESTABLISHED");
    }

    Serial.println("SENSOR CONFIDENCE");
    Serial.print("  Confidence      : "); Serial.print(sensorConfidence); Serial.println(" %");
    Serial.print("  Assessment      : "); Serial.println(sensorConfidenceReason);

    Serial.println("----------------------------------------------");
    Serial.print("OVERALL RISK SCORE: "); Serial.print(overallRiskScore); Serial.println(" / 100");
    Serial.print("RISK LEVEL        : "); Serial.println(getOverallRiskLevelText());
    Serial.print("RISK REASON       : "); Serial.println(overallRiskReason);
    Serial.println("----------------------------------------------");
    Serial.println("Score bands: 0-30 SAFE | 31-60 WARNING | 61-80 HIGH | 81-100 CRITICAL");
    Serial.println("==============================================");
}

// ============================================================
// DETERMINE SAFETY STATE
// ============================================================

void updateSafetyState()
{
    // ROVER-11.7 FINAL FIXED:
    // The Risk Engine is the SINGLE authority for Rover outputs.
    // SOS and fall always have emergency priority.
    updateRiskEngine();

    if (helmetSOSAlarmLatched || helmetSOSActive ||
        (helmetPacketValid && helmetPacket.fallDetected))
    {
        overallRiskScore = 100;
        overallRiskLevel = 3;

        if (helmetSOSAlarmLatched || helmetSOSActive)
        {
            strncpy(overallRiskReason, "Helmet SOS active", sizeof(overallRiskReason) - 1);
        }
        else
        {
            strncpy(overallRiskReason, "Helmet fall detected", sizeof(overallRiskReason) - 1);
        }
        overallRiskReason[sizeof(overallRiskReason) - 1] = '\0';

        showEmergency();
        return;
    }

    if (overallRiskLevel == 3)
    {
        showEmergency();
        return;
    }

    if (overallRiskLevel == 1 || overallRiskLevel == 2)
    {
        showWarning();
        return;
    }

    showSafe();
}

// ============================================================
// ROVER-12.1 - LAPTOP WEB DASHBOARD TEST SERVER
// ============================================================
String jsonEscape(const char *text)
{
    String out = "";
    if (!text) return out;

    for (size_t i = 0; text[i] != '\0'; i++)
    {
        char c = text[i];
        if (c == '"' || c == '\\')
        {
            out += '\\';
        }
        out += c;
    }
    return out;
}

void handleLaptopHome()
{
    laptopWebClientSeen = true;
    lastLaptopRequestMillis = millis();

    String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Mine Rover 12.1</title>
<style>
body{font-family:Arial,sans-serif;background:#f4f6f8;margin:0;padding:24px;color:#222}
.card{max-width:760px;margin:auto;background:white;padding:24px;border-radius:16px;box-shadow:0 4px 18px #0002}
h1{margin-top:0}.ok{font-size:28px;font-weight:bold}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.item{padding:14px;background:#f0f2f4;border-radius:10px}.label{font-size:13px;color:#666}.value{font-size:21px;font-weight:bold;margin-top:4px}
@media(max-width:600px){.grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="card">
<h1>SMART MINE RESCUE ROVER</h1>
<div class="ok">ROVER-12.1 — LAPTOP CONNECTED</div>
<p>This is the Rover Wi-Fi connection test.</p>
<div class="grid">
<div class="item"><div class="label">Rover IP</div><div class="value">192.168.4.1</div></div>
<div class="item"><div class="label">Wi-Fi Network</div><div class="value">MINE-ROVER</div></div>
<div class="item"><div class="label">Risk Score</div><div class="value" id="risk">Loading...</div></div>
<div class="item"><div class="label">Risk Level</div><div class="value" id="level">Loading...</div></div>
<div class="item"><div class="label">Helmet Link</div><div class="value" id="link">Loading...</div></div>
<div class="item"><div class="label">Miner ID</div><div class="value" id="miner">Loading...</div></div>
</div>
<p id="reason">Waiting for Rover data...</p>
</div>
<script>
async function update(){
 try{
  const r=await fetch('/data'); const d=await r.json();
  document.getElementById('risk').textContent=d.riskScore+' / 100';
  document.getElementById('level').textContent=d.riskLevel;
  document.getElementById('link').textContent=d.helmetLink;
  document.getElementById('miner').textContent=d.minerID;
  document.getElementById('reason').textContent='Reason: '+d.riskReason;
 }catch(e){document.getElementById('reason').textContent='Rover data connection error';}
}
update(); setInterval(update,1000);
</script>
</body>
</html>
)HTML";

    roverWebServer.sendHeader("Access-Control-Allow-Origin", "*");
    roverWebServer.send(200, "text/html", html);
}

void handleLaptopData()
{
    laptopWebClientSeen = true;
    lastLaptopRequestMillis = millis();

    HelmetPacket packetCopy = {};
    bool valid;
    bool sosLatched;
    bool sosActive;
    unsigned long packetMillis;

    noInterrupts();
    packetCopy = helmetPacket;
    valid = helmetPacketValid;
    sosLatched = helmetSOSAlarmLatched;
    sosActive = helmetSOSActive;
    packetMillis = lastHelmetPacketMillis;
    interrupts();

    unsigned long packetAge = valid ? millis() - packetMillis : 0;
    bool linkConnected = valid && packetAge <= COMMUNICATION_LOSS_TIMEOUT;

    String json = "{";
    json += "\"rover\":\"ROVER-12.1\",";
    json += "\"riskScore\":" + String(overallRiskScore) + ",";
    json += "\"riskLevel\":\"" + String(getOverallRiskLevelText()) + "\",";
    json += "\"riskReason\":\"" + jsonEscape(overallRiskReason) + "\",";
    json += "\"sensorConfidence\":" + String(sensorConfidence) + ",";
    json += "\"minerID\":\"" + jsonEscape(valid ? packetCopy.minerID : "UNKNOWN") + "\",";
    json += "\"helmetLink\":\"" + String(linkConnected ? "CONNECTED" : "LOST") + "\",";
    json += "\"lastPacketAgeMs\":" + String(packetAge) + ",";
    json += "\"helmetMQ4Change\":" + String(valid ? packetCopy.mq4Change : 0.0f, 1) + ",";
    json += "\"roverMQ4Change\":" + String(mq4Change, 1) + ",";
    json += "\"water\":" + String(waterDetected ? "true" : "false") + ",";
    json += "\"smoke\":" + String(smokeDetected ? "true" : "false") + ",";
    json += "\"distanceCm\":" + String(distanceCm, 1) + ",";
    json += "\"distanceValid\":" + String(ultrasonicValid ? "true" : "false") + ",";
    json += "\"helmetSOS\":" + String((sosLatched || sosActive) ? "true" : "false") + ",";
    json += "\"helmetFall\":" + String((valid && packetCopy.fallDetected) ? "true" : "false") + ",";
    json += "\"packetsReceived\":" + String(receivedPacketCount) + ",";
    json += "\"missedPackets\":" + String(missedHelmetPackets) + ",";
    json += "\"packetDeliveryPercent\":";
    unsigned long totalObserved = receivedPacketCount + missedHelmetPackets;
    float delivery = totalObserved > 0 ? 100.0f * receivedPacketCount / totalObserved : 0.0f;
    json += String(delivery, 1);
    json += "}";

    roverWebServer.sendHeader("Access-Control-Allow-Origin", "*");
    roverWebServer.send(200, "application/json", json);
}

void setupRoverWiFi()
{
    Serial.println();
    Serial.println("==============================================");
    Serial.println("       ROVER-12.1 WI-FI LAPTOP LINK");
    Serial.println("==============================================");
    Serial.println("Starting Rover Wi-Fi Access Point...");

    // AP+STA keeps the existing ESP-NOW station interface available.
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);

    bool started = WiFi.softAP(ROVER_WIFI_SSID, ROVER_WIFI_PASSWORD);

    if (!started)
    {
        Serial.println("ERROR: Rover Wi-Fi AP failed to start!");
        return;
    }

    Serial.print("Wi-Fi SSID     : ");
    Serial.println(ROVER_WIFI_SSID);
    Serial.print("Wi-Fi Password : ");
    Serial.println(ROVER_WIFI_PASSWORD);
    Serial.print("Rover IP       : ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Laptop action  : Connect to MINE-ROVER");
    Serial.println("Browser        : http://192.168.4.1");

    roverWebServer.on("/", HTTP_GET, handleLaptopHome);
    roverWebServer.on("/data", HTTP_GET, handleLaptopData);
    roverWebServer.begin();

    Serial.println("HTTP server   : STARTED");
    Serial.println("API endpoint  : /data");
    Serial.println("==============================================");
    Serial.println();
}

void processRoverWiFi()
{
    roverWebServer.handleClient();

    static unsigned long lastWiFiStatusPrint = 0;
    unsigned long now = millis();

    if (now - lastWiFiStatusPrint >= 5000)
    {
        lastWiFiStatusPrint = now;
        Serial.println();
        Serial.println("----------------------------------------------");
        Serial.println("ROVER-12.1 WI-FI STATUS");
        Serial.print("AP SSID       : "); Serial.println(ROVER_WIFI_SSID);
        Serial.print("AP IP         : "); Serial.println(WiFi.softAPIP());
        Serial.print("Laptop clients: "); Serial.println(WiFi.softAPgetStationNum());
        Serial.print("HTTP client   : ");
        Serial.println(laptopWebClientSeen ? "SEEN" : "WAITING");
        if (laptopWebClientSeen)
        {
            Serial.print("Last request  : ");
            Serial.print(now - lastLaptopRequestMillis);
            Serial.println(" ms ago");
        }
        Serial.println("----------------------------------------------");
    }
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    // ========================================================
    // SERIAL
    // ========================================================

    Serial.begin(115200);

    delay(1000);


    // ========================================================
    // START MESSAGE
    // ========================================================

    Serial.println();

    Serial.println("================================");

    Serial.println(
        "   SMART MINE RESCUE ROVER"
    );

    Serial.println(
        "       ROVER-01 TO ROVER-11.7"
    );

    Serial.println("================================");

    Serial.println();

    Serial.println(
        "ESP32 Rover is starting..."
    );

    Serial.println();


    // ========================================================
    // ROVER-12.1 LAPTOP WI-FI
    // ========================================================

    setupRoverWiFi();

    // ========================================================
    // ROVER-11.3 ESP-NOW RECEIVER
    // ========================================================

    setupHelmetESPNow();

    // ========================================================
    // PIN CONFIGURATION
    // ========================================================

    pinMode(MQ4_PIN, INPUT);

    pinMode(MQ7_PIN, INPUT);

    pinMode(WATER_SENSOR_PIN, INPUT);

    pinMode(SMOKE_SENSOR_PIN, INPUT);

    pinMode(TRIG_PIN, OUTPUT);

    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);


    pinMode(GREEN_LED, OUTPUT);

    pinMode(YELLOW_LED, OUTPUT);

    pinMode(RED_LED, OUTPUT);


    pinMode(BUZZER_PIN, OUTPUT);


    // ========================================================
    // ADC CONFIGURATION
    // ========================================================

    analogReadResolution(12);


    analogSetPinAttenuation(
        MQ4_PIN,
        ADC_11db
    );

    analogSetPinAttenuation(
        MQ7_PIN,
        ADC_11db
    );

    analogSetPinAttenuation(
        WATER_SENSOR_PIN,
        ADC_11db
    );

    analogSetPinAttenuation(
        SMOKE_SENSOR_PIN,
        ADC_11db
    );


    // ========================================================
    // INITIAL OUTPUT
    // ========================================================

    allLEDsOff();

    digitalWrite(
        BUZZER_PIN,
        LOW
    );

    // Start SAFE
    digitalWrite(
        GREEN_LED,
        HIGH
    );


    // ========================================================
    // PIN CONFIGURATION DISPLAY
    // ========================================================

    Serial.println("--------------------------------");

    Serial.println(
        "PIN CONFIGURATION"
    );

    Serial.println("--------------------------------");

    Serial.println(
        "MQ-4 AO       -> GPIO34"
    );

    Serial.println(
        "MQ-7 AO       -> GPIO35"
    );

    Serial.println(
        "Water S       -> GPIO32"
    );

    Serial.println(
        "Smoke AO      -> GPIO33"
    );

    Serial.println(
        "HC-SR04 TRIG  -> GPIO27"
    );

    Serial.println(
        "HC-SR04 ECHO  -> GPIO16"
    );

    Serial.println(
        "GREEN LED     -> GPIO14"
    );

    Serial.println(
        "YELLOW LED    -> GPIO12"
    );

    Serial.println(
        "RED LED       -> GPIO26"
    );

    Serial.println(
        "BUZZER        -> GPIO25"
    );

    Serial.println("--------------------------------");


    // ========================================================
    // MQ-4 CALIBRATION
    // ========================================================

    calibrateMQ4();


    // ========================================================
    // MQ-7 BASELINE
    // ========================================================

    calibrateMQ7();


    // ========================================================
    // READY
    // ========================================================

    showSafe();


    Serial.println();

    Serial.println("================================");

    Serial.println(
        "       ROVER-11.7 READY"
    );

    Serial.println("================================");

    Serial.println();

    Serial.println(
        "MQ-4       : ACTIVE"
    );

    Serial.println(
        "MQ-7       : MONITOR ONLY"
    );

    Serial.println(
        "Water      : ACTIVE"
    );

    Serial.println(
        "Smoke      : ACTIVE"
    );

    Serial.println(
        "HC-SR04    : ACTIVE"
    );

    Serial.println();

    Serial.println(
        "Distance status:"
    );

    Serial.println(
        ">100 cm  -> CLEAR"
    );

    Serial.println(
        "30-100 cm -> OBJECT NEAR"
    );

    Serial.println(
        "<30 cm   -> OBSTACLE / CLOSE"
    );

    Serial.println(
        "<10 cm   -> VERY CLOSE"
    );

    Serial.println();

    Serial.println(
        "Smoke threshold = 500"
    );

    Serial.println(
        "Smoke confirmation = 3 readings"
    );

    Serial.println();

    Serial.println(
        "Safety logic:"
    );

    Serial.println(
        "SOS / FALL -> CRITICAL -> RED + BUZZER"
    );

    Serial.println(
        "Risk 31-80 -> WARNING/HIGH -> YELLOW + BUZZER"
    );

    Serial.println(
        "Risk 81-100 -> CRITICAL -> RED + BUZZER"
    );

    Serial.println();
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
    // ========================================================
    // ROVER-12.1 LAPTOP WI-FI
    // ========================================================
    processRoverWiFi();

    // ========================================================
    // ROVER-11.5 SOS PRIORITY
    // ========================================================
    // Process a newly received SOS before any slower sensor reads.
    // This removes the previous ~1-second loop delay from the alarm.
    // ========================================================
    processHelmetSOSEvent();

    // ROVER-11.5 application ACK and communication metrics.
    sendHelmetApplicationACK();
    printCommunicationMetrics();

    // ========================================================
    // ROVER-11.4 COMMAND INPUT
    // ========================================================

    processRoverCommand();

    // ========================================================
    // ROVER-11.4 HELMET PACKET
    // ========================================================

    printHelmetPacket();
    updateHelmetCommunicationStatus();

    // ========================================================
    // READ MQ-4
    // ========================================================

    mq4Raw = readMQ4();


    // ========================================================
    // READ MQ-7
    // ========================================================

    mq7Raw = readMQ7();


    // ========================================================
    // READ WATER
    // ========================================================

    waterRaw =
        readWaterSensor();


    // ========================================================
    // READ SMOKE
    // ========================================================

    smokeRaw =
        readSmokeSensor();


    // ========================================================
    // READ HC-SR04 DISTANCE
    // ========================================================

    distanceCm =
        readUltrasonicDistance();


    // ========================================================
    // CALCULATE MQ-4 CHANGE
    // ========================================================

    mq4Change =
        calculateChange(
            mq4Raw,
            mq4Baseline
        );

    updateMQ4Warning();


    // ========================================================
    // CALCULATE MQ-7 CHANGE
    // ========================================================

    mq7Change =
        calculateChange(
            mq7Raw,
            mq7Baseline
        );


    // ========================================================
    // WATER DETECTION
    // ========================================================

    if (
        waterRaw >
        WATER_THRESHOLD
    )
    {
        waterDetected = true;
    }
    else
    {
        waterDetected = false;
    }


    // ========================================================
    // SMOKE DETECTION
    // ========================================================

    updateSmokeDetection();


    // ========================================================
    // PRINT DATA
    // ========================================================

    printSensorData();


    // ========================================================
    // UPDATE SAFETY
    // ========================================================

    updateSafetyState();

    // ROVER-11.6: propagate environmental state/cause to Helmet.
    // ROVER-11.7: overall risk is calculated by updateSafetyState().
    sendEnvironmentalHazardIfChanged();
    processRoverWarningACK();


    // ========================================================
    // LOOP DELAY
    // ========================================================

    delay(1000);
}