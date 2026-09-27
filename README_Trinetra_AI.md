# Trinetra AI
### AI-Powered Underground Mine Safety, Monitoring & Rescue System

> **THREE EYES. ONE MISSION. EVERY MINER SAFE.**

Trinetra AI is a prototype underground mine-safety platform connecting a **Smart Miner Helmet**, **Rescue Rover**, **Risk Intelligence Engine**, and **Control Dashboard** into one operational safety system.

**Sense the miner and environment → combine the evidence → identify risk → communicate the decision → support emergency response.**

---

## 🚨 Why Trinetra AI?

Underground incidents can involve multiple signals at the same time: gas changes, smoke, water ingress, a miner fall, an SOS event, communication loss, or abnormal physiological data.

A single sensor does not provide the complete picture.

**Trinetra AI combines miner-side and rover-side evidence into one real-time safety decision.**

```text
SMART HELMET                RESCUE ROVER
     │                           │
     │ miner + health data       │ environmental + visual data
     └──────────────┬────────────┘
                    ↓
          SENSOR FUSION / RISK ENGINE
                    ↓
             RISK SCORE 0–100
                    ↓
       SAFE / WARNING / HIGH / CRITICAL
                    ↓
          DASHBOARD + ALERT + SOS
                    ↓
             RESCUE DECISION
```

---

# 👷 01. Smart Miner Helmet

The helmet is the **miner-side safety node**.

### Current firmware monitors

- **MQ-4** — methane-related gas change detection
- **MQ-7** — carbon-monoxide sensor input
- **MPU6050** — motion/fall detection
- **MAX30100** — heart-rate sensing in the current firmware
- **DHT11** — temperature/humidity in the current firmware
- **SOS button** — manual emergency trigger
- **Buzzer + LEDs** — local warning and status feedback
- **ESP-NOW** — telemetry and emergency communication with the rover

### Safety logic

The helmet performs sensor sampling, baseline calibration and warning filtering. MQ-4 warnings use a baseline/change approach with repeated confirmation before activation.

A detected **SOS or fall condition is treated as an immediate personal-safety emergency** by the rover risk engine.

---

# 🤖 02. Smart Rescue Rover

The rover acts as a **remote sensing and emergency-support platform**.

### Current rover sensing

- **MQ-4** — methane-related gas change
- **MQ-7** — carbon-monoxide sensor input
- **MQ-2** — smoke detection
- **Water sensor** — water presence detection
- **Ultrasonic sensor** — obstacle distance
- **Helmet telemetry** received over ESP-NOW
- **Status LEDs + buzzer** — local hazard indication

The rover communicates detected hazard states back to the helmet and exposes telemetry to the control dashboard.

**Helmet + Rover = cross-location safety evidence.**

---

# 🧠 03. Risk Intelligence Engine

The current prototype implements a **real-time sensor-fusion/rule-based risk engine** on the rover.

## `RISK SCORE: 0–100`

| Score | Level |
|---:|---|
| **0–30** | SAFE |
| **31–60** | WARNING |
| **61–80** | HIGH |
| **81–100** | CRITICAL |

### Emergency overrides

- **Helmet SOS → 100 / CRITICAL**
- **Helmet fall → 100 / CRITICAL**

### Evidence used by the engine

- Rover MQ-4 warning
- Rover smoke detection
- Rover water detection
- Helmet MQ-4 warning
- Helmet/rover gas agreement
- Valid abnormal heart-rate condition
- Helmet communication availability/timeout
- Prototype temperature condition

The architecture is modular: the current deterministic engine provides explainable decisions and a foundation for future ML-based anomaly detection and prediction.

> **Engineering honesty:** this repository currently implements sensor-fusion/rule-based scoring; it does not claim a trained ML model where none is implemented.

---

# 📡 04. Communication Architecture

### Helmet ↔ Rover

**ESP-NOW** is used for local telemetry, hazard commands, acknowledgements and emergency-state propagation.

### Rover ↔ Control Dashboard

**Wi-Fi Access Point + HTTP API**

The rover creates the local network:

```text
MINE-ROVER
```

The dashboard reads rover telemetry from:

```text
http://192.168.4.1/data
```

### Camera

A separate **ESP32-CAM** firmware provides a low-latency camera feed using **GC2145, RGB565, 320 × 240** frames over HTTP.

---

# 🖥️ 05. Control Dashboard

Built with **React + Vite** as the control-room interface.

### Dashboard modules

- System Risk Panel
- Miner Status
- Helmet RF Transport
- Environmental Sensors
- Rover Camera
- AI Risk Assessment Engine
- Rover Hardware Core Telemetry
- Emergency Rescue Alert

### Live operational data

- Risk score, level and reason
- Sensor confidence
- Miner ID and helmet link
- SOS and fall state
- Helmet/rover MQ-4 change
- Smoke and water status
- Obstacle distance
- Packet delivery rate and missed packets
- Last packet age
- Rover status

The frontend refreshes rover telemetry every **1 second**.

---

# 🏗️ System Architecture

```text
                  ┌─────────────────────┐
                  │   SMART MINER       │
                  │      HELMET         │
                  │ MQ-4 / MQ-7         │
                  │ MAX30100 / MPU6050  │
                  │ DHT11 / SOS         │
                  └──────────┬──────────┘
                             │
                         ESP-NOW
                             │
                             ↓
                  ┌─────────────────────┐
                  │   RESCUE ROVER      │
                  │ MQ-4 / MQ-7 / MQ-2 │
                  │ Water / Ultrasonic  │
                  │ Risk Engine         │
                  └──────────┬──────────┘
                             │
                    Wi-Fi + HTTP API
                             │
                             ↓
                  ┌─────────────────────┐
                  │  CONTROL DASHBOARD  │
                  │ Risk • Miner • RF   │
                  │ Sensors • Camera    │
                  │ Alerts • Telemetry  │
                  └─────────────────────┘

                  ┌─────────────────────┐
                  │     ESP32-CAM       │
                  │  Rover visual feed  │
                  └─────────────────────┘
```

---

# 📁 Repository Structure

```text
Smart-Mine-Rescue/
│
├── firmware/
│   ├── helmet/Smart_Miner_Helmet/
│   │   ├── platformio.ini
│   │   └── src/main.cpp
│   ├── rover/Smart_Mine_Rescue_Rover/
│   │   ├── platformio.ini
│   │   └── src/main.cpp
│   └── esp32-cam/Rover_Camera/
│       ├── platformio.ini
│       └── src/main.cpp
│
├── frontend/rover-dashboard/
│   ├── src/
│   │   ├── components/
│   │   ├── pages/
│   │   └── services/
│   ├── package.json
│   └── vite.config.js
│
├── package.json
└── README.md
```

---

# 🛠️ Technology Stack

### Embedded

ESP32 • ESP32-CAM • Arduino Framework • PlatformIO • ESP-NOW • Wi-Fi • HTTP

### Sensors

MQ-4 • MQ-7 • MQ-2 • MPU6050 • MAX30100 • DHT11 • Water Sensor • Ultrasonic Sensor

### Software

C++ • React • Vite • JavaScript • HTML/CSS

---

# 🚀 Getting Started

## 1. Clone

```bash
git clone https://github.com/sandesh-cyber/sih-project-Al-Powered-Underground-Mine-Safety-Monitoring-and-Rescue-System.git
cd sih-project-Al-Powered-Underground-Mine-Safety-Monitoring-and-Rescue-System
```

## 2. Install dashboard dependencies

```bash
npm install --prefix frontend/rover-dashboard
```

## 3. Run the dashboard

```bash
npm run dev
```

Available root scripts:

```bash
npm run build
npm run lint
npm run preview
```

## 4. Flash the helmet

Open `firmware/helmet/Smart_Miner_Helmet` in PlatformIO, build/upload, then open the serial monitor at **115200 baud**.

## 5. Flash the rover

Open `firmware/rover/Smart_Mine_Rescue_Rover` in PlatformIO, build/upload, then open the serial monitor at **115200 baud**.

## 6. Flash the ESP32-CAM

Open `firmware/esp32-cam/Rover_Camera` in PlatformIO and set the upload/monitor port in `platformio.ini` for your machine before uploading.

---

# 🔬 Prototype Validation Flow

### Normal

```text
Helmet connected → Rover connected → No active hazard → SAFE
```

### Gas warning

```text
MQ-4 change → confirmation → risk increases → dashboard warning
```

### Multiple hazards

```text
Gas + smoke/water → sensor fusion → higher risk state
```

### Miner emergency

```text
SOS / fall → emergency override → RISK 100 → CRITICAL RESCUE ALERT
```

---

# 🎯 What Makes Trinetra AI Different?

### 01 — THREE EYES → ONE DECISION

**Helmet + Rover + Risk Intelligence** combine multiple sources instead of treating every sensor as an isolated system.

### 02 — EXPLAINABLE RISK

The system exposes both the **risk score and the reason behind the risk state**.

### 03 — PERSONAL + ENVIRONMENTAL SAFETY

Miner-side conditions and rover-side environmental hazards contribute to the safety picture.

### 04 — EMERGENCY-FIRST DESIGN

SOS and fall detection can immediately override normal risk accumulation.

### 05 — LOCAL-FIRST OPERATION

Core prototype operation uses local ESP-NOW and rover-hosted Wi-Fi/HTTP communication without requiring cloud connectivity for the demonstration.

---

# 📊 Current Prototype Status

| Capability | Status |
|---|---|
| Smart helmet firmware | ✅ Implemented |
| Rover firmware | ✅ Implemented |
| ESP-NOW helmet ↔ rover | ✅ Implemented |
| MQ-4 / MQ-7 monitoring | ✅ Implemented |
| Smoke detection | ✅ Implemented |
| Water detection | ✅ Implemented |
| Fall detection | ✅ Implemented |
| SOS event handling | ✅ Implemented |
| Risk score 0–100 | ✅ Implemented |
| Risk reason | ✅ Implemented |
| Sensor confidence | ✅ Implemented |
| Rover HTTP API | ✅ Implemented |
| React control dashboard | ✅ Implemented |
| ESP32-CAM firmware | ✅ Implemented |
| ML-trained prediction model | 🔄 Future integration |
| Certified mine-safety hardware | 🔄 Engineering/validation required |

---

# 🔐 Safety & Engineering Note

Trinetra AI is a **research/prototype system**, not a certified mine-safety product.

MQ-series sensors, hobby-grade controllers, prototype communication links and other components require calibration, validation, ruggedization, redundancy and certification before safety-critical deployment.

This repository demonstrates the **system architecture, embedded implementation, sensor fusion, communication flow, risk logic and control interface**.

---

# 🌱 Roadmap

### Phase 1 — Current Prototype
Helmet sensing • Rover sensing • ESP-NOW • Risk engine • Dashboard • Camera • Emergency handling

### Phase 2 — Field Engineering
Rugged enclosure • Power optimization • Sensor calibration/fault detection • Communication testing • Mine-map integration • Local positioning • Improved camera/thermal integration

### Phase 3 — Intelligent Safety
Historical telemetry • Anomaly detection • ML-based prediction • Sensor-health diagnostics • Predictive warnings

### Phase 4 — Deployment Validation
Mine-environment trials • Reliability testing • Environmental qualification • Safety validation • Compliance/certification assessment

---

# 🏆 Smart India Hackathon 2026

**Problem Statement:** 26039  
**Team:** Trinetra AI  
**Theme:** Smart Automation  
**Category:** Hardware – Smart Automation  

### Project

**AI-Powered Underground Mine Safety, Monitoring and Rescue System**

> **Three Eyes. One Mission. Every Miner Safe.**

---

# 📌 Project Links

- **Repository:** https://github.com/sandesh-cyber/sih-project-Al-Powered-Underground-Mine-Safety-Monitoring-and-Rescue-System
- **Prototype:** _Add your deployed prototype link_
- **Demo Video:** _Add your demonstration video link_
- **Presentation:** _Add your SIH presentation link_

---

## ⭐ Final Takeaway

> **Trinetra AI does not simply detect a hazard. It connects the miner, the environment and the rescue system into one real-time safety decision loop.**

**SENSE → UNDERSTAND → LOCATE → ALERT → SUPPORT RESCUE**

---

### License

This project is developed as a Smart India Hackathon prototype and is intended for educational, research and demonstration purposes.
