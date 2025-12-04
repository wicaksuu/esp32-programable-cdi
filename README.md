# Smart CDI Programmable v2.2

> **Programmable Capacitive Discharge Ignition System for 2-Stroke Motorcycles**
>
> Advanced ignition management with Quick Shifter, Launch Control, Anti-Wheelie, Traction Control, and real-time tuning via Web Dashboard.

[![Version](https://img.shields.io/badge/version-2.2-blue.svg)](https://github.com/yourusername/smart-cdi)
[![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![License](https://img.shields.io/badge/license-MIT-orange.svg)](LICENSE)

---

## 📋 Table of Contents

- [Features](#-features)
- [Hardware Requirements](#-hardware-requirements)
- [Pin Mapping](#-pin-mapping)
- [Quick Start](#-quick-start)
- [Feature Logic](#-feature-logic)
- [Web Dashboard](#-web-dashboard)
- [API Reference](#-api-reference)
- [Configuration](#-configuration)
- [Safety & Edge Protection](#-safety--edge-protection)
- [Troubleshooting](#-troubleshooting)

---

## 🚀 Features

### Core Ignition
- ✅ **Programmable Ignition Curves** - Up to 41 RPM/advance points per map
- ✅ **Multi-Map Storage** - 5 maps on SD card, switchable on-the-fly
- ✅ **AC/DC Pickup Support** - Works with magneto, hall sensors, or points
- ✅ **Adjustable Dwell Time** - 500-10,000 μs for optimal coil saturation
- ✅ **Rev Limiter** - 4 cut patterns (hard cut, 50%, 66%, 33%)

### Advanced Features
- ✅ **Quick Shifter (QS)** - Clutchless upshifts with RPM-dependent kill time
- ✅ **Launch Control (LC)** - RPM hold with configurable retard/cut patterns
- ✅ **Anti-Wheelie (AW)** - Pitch-based intervention via IMU
- ✅ **Traction Control (TC)** - Dual wheel speed sensor slip detection

### Safety & Monitoring
- ✅ **6-Layer Edge Protection**
  - Sensor health monitoring (stuck/erratic detection)
  - RPM sanity checks (glitch >5000 RPM/sec)
  - Coil overheat protection (>350 sparks/sec)
  - Consecutive misfire detection (>100 = safe mode, >200 = shutdown)
  - Battery voltage monitoring (<10.5V = shutdown)
  - Emergency shutdown system
- ✅ **High-Performance Logging** - Ring buffer + SD card (100KB auto-rotation)
- ✅ **Real-Time Diagnostics** - Web dashboard with live metrics

### User Interface
- ✅ **WiFi Web Dashboard** - Full tuning interface via smartphone/laptop
- ✅ **Physical Buttons** - Map switching without tools
- ✅ **OTA Firmware Updates** - No USB cable needed for updates

---

## 🔧 Hardware Requirements

### Minimum (Basic Ignition)
- ESP32 Development Board (ESP32-DevKitC or compatible)
- Logic-level MOSFET (IRFZ44N, IRLZ44N)
- Flyback diode (1N4007 or better)
- AC/DC pickup sensor (magneto, hall, or points)
- microSD card (FAT32, 1-32GB)
- 12V motorcycle battery
- Buck converter (12V → 5V, 3A)

### Full Features
All of the above, plus:
- MPU9250 IMU (for anti-wheelie)
- 2x Hall effect sensors + magnets (for traction control)
- Pressure/strain sensor (for quick shifter)
- Microswitch (for clutch detection)
- Pushbuttons (3x for map selection)

---

## 📍 Pin Mapping (ESP32 Board)

### Critical Pins

| GPIO | Function | Type | Notes |
|------|----------|------|-------|
| **4** | Ignition Output | Output | To MOSFET gate (1kΩ resistor) |
| **34** | AC Sensor | Analog In | Pickup coil signal (biased 1.65V) |
| **35** | DC Sensor | Digital In | Hall/points (pullup enabled) |
| **36** | QS Pressure | Analog In | 0-3.3V strain/pressure sensor |
| **27** | QS Digital | Digital In | Backup microswitch |
| **39** | Clutch Sensor | Digital In | LOW = pulled, HIGH = released |
| **22** | Front Wheel | Digital In | Hall sensor (TC) |
| **14** | Rear Wheel | Digital In | Hall sensor (TC) |
| **21** | I2C SDA | Bidir | MPU9250 data (AW) |
| **22** | I2C SCL | Output | MPU9250 clock |

### SD Card (SPI)
| GPIO | Function |
|------|----------|
| **5** | CS (Chip Select) |
| **18** | SCK (Clock) |
| **19** | MISO (Data In) |
| **23** | MOSI (Data Out) |

### User Interface
| GPIO | Function |
|------|----------|
| **25** | Map Select Button |
| **32** | Map Down Button |
| **33** | Map Up Button |
| **26** | AC/DC Mode Switch |

**🔗 For detailed wiring diagrams, see [PINOUT.md](PINOUT.md)**

---

## 🏁 Quick Start

### 1. Hardware Setup

**Wire ignition circuit:**
```
ESP32 GPIO4 → 1kΩ → MOSFET Gate
MOSFET Drain → Ignition Coil (-)
Coil (+) → +12V Battery
Flyback Diode across Coil
```

**Connect power:**
```
Battery +12V → Buck Converter → 5V → ESP32 VIN
Battery GND → ESP32 GND
```

**Connect pickup sensor:**
- **AC Mode:** Pickup coil → voltage divider → GPIO34
- **DC Mode:** Hall sensor OUT → GPIO35

### 2. Firmware Upload

```bash
# Install Arduino IDE & ESP32 board support
# Install libraries: ArduinoJson, SPI, SD, WiFi, MPU9250
# Open smart-cdi-progamable.ino
# Select Board: "ESP32 Dev Module"
# Upload
```

### 3. Initial Configuration

1. Connect to WiFi AP: `SmartCDI-Config` (password: `12345678`)
2. Navigate to: `http://192.168.4.1`
3. Load default map & adjust rev limiter
4. Calibrate sensors (AC threshold, QS, IMU)

---

## 🧠 Feature Logic

### 1. Ignition Timing

**Algorithm:**
1. Read pickup sensor (AC or DC)
2. Calculate RPM: `RPM = 60,000,000 / intervalMicros`
3. Interpolate advance from curve
4. Apply retards (LC/AW/TC if active)
5. Clamp to 0-60° BTDC
6. Schedule ignition interrupt

**Safety:** Invalid RPM → default 5° BTDC

---

### 2. Rev Limiter Cut Patterns

| Pattern | Fire Rate | Logic |
|---------|-----------|-------|
| 0 | 0% (Hard) | `shouldFire = false` |
| 1 | 50% | `shouldFire = (counter % 2) != 0` |
| 2 | 66% | `shouldFire = (counter % 3) != 0` |
| 3 | 33% | `shouldFire = (counter % 3) == 0` |

**Priority:** Rev Limiter > LC > AW > TC

---

### 3. Quick Shifter

**Activation:**
```cpp
qsActive = (
  qsEnabled &&
  currentRPM >= qsMinRPM &&
  currentRPM <= qsMaxRPM &&
  (qsPressure > threshold || qsSwitch == LOW)
);
```

**Kill Time:** Interpolated from RPM curve
- High RPM → Shorter kill time (55-70ms)
- Low RPM → Longer kill time (120-150ms)

---

### 4. Launch Control

**Activation:**
```cpp
lcActive = (
  lcEnabled &&
  frontSpeed < 5 km/h &&
  currentRPM >= lcTargetRPM &&
  clutchPulled == true
);
```

**Control:** Retard timing + optional cut pattern

---

### 5. Anti-Wheelie

**Detection:**
```cpp
pitchDeviation = currentPitch - baselinePitch;
awActive = (
  awEnabled &&
  pitchDeviation > awThreshold &&  // e.g., >18°
  frontSpeed > 10 km/h
);
```

**Intervention:** Timing retard 3-10°, optional cut

---

### 6. Traction Control

**Slip Calculation:**
```cpp
slipRatio = (rearSpeed - frontSpeed) / frontSpeed;
tcActive = (slipRatio > threshold);  // e.g., >15%
```

**Intervention:** Timing retard 4-8°, optional cut

---

### 7. Emergency Shutdown

**6 Protection Layers:**
1. Sensor stuck detection
2. RPM glitch detection (>5000 RPM jump)
3. Coil overheat (>350 sparks/sec)
4. Consecutive misfires (>200 = shutdown)
5. Low voltage (<10.5V for >5 sec)
6. User emergency button

**Recovery:** Fix issue → Reset via dashboard

---

## 🌐 Web Dashboard

**Access:** http://192.168.4.1 (AP mode) or http://[ESP32_IP]

**Tabs:**
1. **Dashboard** - Real-time RPM, advance, features status
2. **Maps** - Visual curve editor, load/save
3. **Quick Shifter** - Kill time tuning
4. **Launch Control** - Target RPM, retard settings
5. **Anti-Wheelie** - Pitch threshold, IMU calibration
6. **Traction Control** - Slip threshold, tire calculator
7. **Calibration** - Sensor setup wizards
8. **Logs** - Real-time log viewer, CSV download
9. **Settings** - WiFi, OTA updates

---

## 📡 API Reference

### Key Endpoints

```http
GET  /api/status              # Current state
GET  /api/maps                # List all maps
GET  /api/maps/{id}           # Get map details
PUT  /api/maps/{id}           # Update map
POST /api/maps/create         # Create new map
POST /api/emergency           # Emergency control
GET  /api/logs                # Retrieve logs
```

**Full API documentation in code comments**

---

## ⚙️ Configuration

### Default Maps
- **Default Optimal** (Map 1): Balanced, 11,500 RPM
- **Street** (Map 2): Safe, 10,500 RPM
- **Race** (Map 3): Aggressive, 12,100 RPM
- **Drag** (Map 4): Launch-focused, 11,800 RPM

See [MAPS_README.md](MAPS_README.md) for tuning guide.

---

## 🛡️ Safety & Edge Protection

### Pre-Flight Checklist
- [ ] Disconnect coil during initial testing
- [ ] Verify timing with oscilloscope + timing light
- [ ] Test on dyno before track/street
- [ ] Set conservative limits (rev limiter -500 RPM)
- [ ] Enable all edge protections

### Failsafes
- Watchdog timer (2 sec)
- Default safe map if SD fails
- Voltage lockout (<10.5V)
- Invalid RPM handling

---

## 🔍 Troubleshooting

### No Spark
1. Check LED on GPIO4 (should blink when cranking)
2. Verify pickup sensor (AC: 0-3.3V swing, DC: HIGH/LOW toggle)
3. Check MOSFET wiring (gate resistor, flyback diode)

### Erratic RPM
1. AC: Add 100pF cap GPIO34→GND, use shielded cable
2. DC: Check hall sensor gap (1-3mm), verify pullup

### QS Not Working
1. Check sensor value in dashboard (should change)
2. Verify RPM in range (qsMinRPM to qsMaxRPM)
3. Adjust threshold above resting value

### LC Won't Activate
1. Verify clutch sensor (LOW when pulled)
2. Check speed sensor (<5 km/h required)
3. Ensure RPM ≥ target RPM

### Full troubleshooting guide in [PINOUT.md](PINOUT.md)

---

## 📚 Resources

- **Detailed Pinout:** [PINOUT.md](PINOUT.md)
- **Map Tuning:** [MAPS_README.md](MAPS_README.md)
- **Libraries:** ArduinoJson, MPU9250-DMP

---

## ⚠️ Disclaimer

**Experimental racing electronics. Use at own risk.**
- Not street legal
- No warranty
- Test on dyno first
- Engine damage possible if misconfigured

**You accept full responsibility for damage/injury.**

---

## 🙏 Credits

- ESP32: Espressif Systems
- ArduinoJSON: Benoit Blanchon
- MPU9250: Kris Winer
- Community contributors

---

**Version:** 2.2 | **Updated:** 2025-12-04
**Platform:** ESP32 Board (Not CYD)

**⭐ Star us on GitHub if this helped you!**
