# Smart CDI v2.2 - Production Schematic

> **Complete Production-Ready Schematics for PCB Manufacturing**

---

## 📋 Table of Contents

- [Complete System Diagram](#complete-system-diagram)
- [Power Supply Circuit](#power-supply-circuit)
- [Ignition Output Circuit](#ignition-output-circuit)
- [Sensor Input Circuits](#sensor-input-circuits)
- [PCB Layout Guide](#pcb-layout-guide)
- [Bill of Materials (BOM)](#bill-of-materials-bom)
- [Assembly Instructions](#assembly-instructions)
- [Testing & Validation](#testing--validation)

---

## 🔌 Complete System Diagram

### Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        SMART CDI SYSTEM v2.2                            │
└─────────────────────────────────────────────────────────────────────────┘

                    BATTERY +12V (10-14.5V DC)
                         │
        ┌────────────────┼────────────────┐
        │                │                │
    [FUSE 10A]      [FUSE 5A]      [TVS Diode]
        │                │            P6KE15A
        │                │                │
        ▼                ▼               GND
   ┌─────────┐     ┌──────────┐
   │ LM2596  │     │ MOSFET   │
   │ 12V→5V  │     │ Driver   │
   │  3A     │     └────┬─────┘
   └────┬────┘          │
        │               │
        ▼               ▼
   ┌─────────────┐  ┌────────────────┐
   │   ESP32     │  │ Ignition Coil  │
   │  Dev Board  │  │   Primary      │
   └──────┬──────┘  └────────────────┘
          │
   ┌──────┴───────────────────────────────────┐
   │                                          │
   ▼                                          ▼
┌──────────────┐                      ┌──────────────┐
│   SENSORS    │                      │  PERIPHERALS │
│              │                      │              │
│ • AC Pickup  │                      │ • SD Card    │
│ • DC Hall    │                      │ • WiFi       │
│ • QS Sensor  │                      │ • Buttons    │
│ • Clutch SW  │                      │ • MPU9250    │
│ • Wheel Hall │                      │ • Status LED │
└──────────────┘                      └──────────────┘
```

---

## ⚡ Power Supply Circuit

### Main Power Rail (12V → 5V)

```
12V Battery ──┬── [F1: 10A Fuse] ──┬── [D1: 1N5822] ──┬── [C1: 1000µF/16V] ──┐
              │                     │                  │                      │
              │                     │                  │                      │
             ┌┴┐                   ┌┴┐                ┌┴┐                    ┌┴┐
             │ │ TVS               │ │ 10kΩ          │ │ 100nF              │ │
             │ │ P6KE15A           │ │ Bleeder       │ │ MLCC               │ │
             └┬┘                   └┬┘                └┬┘                    └┬┘
              │                     │                  │                      │
             GND                   GND                GND                    ┌┴──────────┐
                                                                             │  LM2596   │
                  ┌──────────────────────────────────────────────────────────┤  Buck     │
                  │                                                          │  Module   │
                  │  Output 5V @ 3A                                          │           │
                  │                                                          └───────────┘
                  │
                  ├── [C2: 470µF/6.3V] ──┬── [C3: 100µF/6.3V] ──┬── 5V Rail
                  │                       │                      │
                  │                      ┌┴┐                    ┌┴┐
                  │                      │ │ 10nF               │ │ 100nF
                  │                      │ │ MLCC               │ │ X7R
                  │                      └┬┘                    └┬┘
                  │                       │                      │
                 GND                     GND                    GND

5V Rail ──┬── ESP32 VIN (5V input)
          ├── Hall Sensors VCC (5V)
          ├── SD Card VCC (via LDO 3.3V)
          └── Optional TFT Display (check voltage!)
```

### 3.3V Rail (From ESP32 Internal LDO)

```
ESP32 3.3V ──┬── MPU9250 VCC (3.3V ONLY!)
             ├── I2C Pullups (2x 4.7kΩ)
             ├── Analog Sensor Bias
             └── [C4: 10µF/6.3V] ── GND (local decoupling)
```

---

## 🔥 Ignition Output Circuit (CRITICAL)

### High-Side Driver with Protection

```
ESP32 GPIO4 ──┬── [R1: 1kΩ] ──────┬─── MOSFET Gate (IRFZ44N)
              │                    │
             ┌┴┐                  ┌┴┐
             │ │ 10kΩ             │ │ 100Ω (Gate stopper)
             │ │ Pulldown         │ │ Optional
             └┬┘                  └┬┘
              │                    │
             GND                  ┌┴─────────────┐
                                  │   IRFZ44N    │
                                  │   N-MOSFET   │
                                  │              │
         Ignition Coil (-) ───────┤ Drain        │
                                  │              │
         GND ─────────────────────┤ Source       │
                                  └──────────────┘

Ignition Coil Wiring:

                 +12V Battery
                      │
                      ├─────────────┐
                      │             │
                  [F2: 5A]      ┌───┴────┐
                      │         │ Diode  │ D2: 1N4007 (Flyback Protection)
                      │         │  │▌─   │ Cathode to +12V
                      │         └───┬────┘
                      │             │
                 ┌────┴─────────────┴────┐
                 │  IGNITION COIL        │
                 │  Primary Winding      │
                 │  (+)              (-) │
                 └───────────────────┬───┘
                                     │
                              To MOSFET Drain

CRITICAL COMPONENTS:
- R1 (1kΩ): Gate current limiter (prevents inrush)
- 10kΩ Pulldown: Keeps MOSFET OFF during ESP32 boot
- D2 (1N4007): Flyback protection (MUST have, or ESP32 will reset!)
- F2 (5A): Coil circuit protection
- MOSFET: IRFZ44N (Vgs(th) < 3V, Rds(on) < 0.03Ω @ 5V)

ALTERNATIVE MOSFETS:
- IRLZ44N (better for 3.3V logic)
- IRF3708 (lower Rds(on))
- AOD4184 (SMD equivalent)
```

### Optional: Dual Ignition Output (for dual-plug engines)

```
GPIO4 ────► MOSFET 1 ────► Coil 1 (Front Cylinder)
GPIO16 ───► MOSFET 2 ────► Coil 2 (Rear Cylinder)

(Same circuit as above, replicated)
```

---

## 📡 Sensor Input Circuits

### 1. AC Pickup Sensor (Alternator/Magneto)

```
AC Pickup Coil (from engine)
    ~  ~
    │  │
    ├──┴── [C5: 10nF] ──┬── [R2: 1MΩ] ── GND
    │                   │
    │                   │                +3.3V
    │                   │                  │
    │                   │               [R3: 10kΩ]
    │                   │                  │
    │                   └──────────────────┼────────────┐
    │                                      │            │
    │                                   [R4: 10kΩ]      │
    │                                      │            │
    └──────────────────────────────────────┼──── GPIO34 (ADC1_CH6)
                                           │            │
                                        [R5: 10kΩ]   [C6: 100pF]
                                           │            │
                                          GND          GND

COMPONENT VALUES:
- C5 (10nF): AC coupling, blocks DC offset
- R2 (1MΩ): DC path to ground, prevents charge buildup
- R3/R4 (10kΩ each): Voltage divider, biases signal to 1.65V (mid-rail)
- R5 (10kΩ): Input impedance, noise reduction
- C6 (100pF): Optional noise filter (add if needed)

ADC READINGS:
- Idle: ~2048 (1.65V, mid-point)
- Trigger pulse: Swings 0-3.3V (0-4095 ADC)
- Threshold: 2048 (crossing detector in code)

TUNING:
- Weak signal: Replace R5 with 4.7kΩ (more gain)
- Too much noise: Increase C6 to 220pF
- Invert signal: Set acInvertSignal = true in map
```

### 2. DC Pickup Sensor (Hall Effect)

```
Hall Sensor (A3144 / SS441A)
┌──────────────┐
│  1   VCC     │──── +5V (or 3.3V, check datasheet)
│              │
│  2   GND     │──── GND
│              │
│  3   OUT     │──┬─── [R6: 4.7kΩ] ──── +3.3V (Pullup)
└──────────────┘  │
                  ├─── [C7: 100nF] ──── GND (Debounce)
                  │
                  └─── GPIO35 (Input)

OPERATION:
- Magnet present: OUT = LOW (0V)
- No magnet: OUT = HIGH (3.3V via pullup)

ALTERNATIVE: Use internal ESP32 pullup (35kΩ)
pinMode(DC_SENSOR_PIN, INPUT_PULLUP);  // Enable in code
```

### 3. Quick Shifter - Analog Pressure Sensor

```
Pressure/Strain Sensor (0-5V Output)
┌──────────────┐
│  VCC         │──── +5V
│              │
│  GND         │──── GND
│              │
│  OUT (0-5V)  │──┬─── [R7: 4.7kΩ] ───┬─── GPIO36 (ADC1_CH0)
└──────────────┘  │                   │
                  │                [R8: 10kΩ]
                  │                   │
                 GND                 GND

VOLTAGE DIVIDER (if sensor outputs 5V):
- Ratio: R8/(R7+R8) = 10k/(4.7k+10k) = 0.68
- 5V input → 3.4V output (safe for ESP32)

BETTER: Use 0-3.3V sensor directly (no divider needed)

CALIBRATION:
- Resting: ~1.65V (ADC ~2048)
- Pressed: >2.5V (ADC >3000)
- Threshold: Set in dashboard (e.g., 2600)
```

### 4. Quick Shifter - Digital Microswitch (Backup)

```
Microswitch (NO - Normally Open)
    │
    ├───┬─── GPIO27 (Input_Pullup enabled in code)
    │   │
   GND [C8: 100nF] ── GND (Debounce)

OPERATION:
- Switch open (no shift): GPIO27 = HIGH (pulled up)
- Switch closed (shifting): GPIO27 = LOW (grounded)
```

### 5. Clutch Position Sensor

```
Lever-Mount Microswitch (NO)
    │
    ├───┬─── GPIO39 (Input_Pullup enabled)
    │   │
   GND [C9: 100nF] ── GND (Debounce)

OPERATION:
- Clutch released: Switch OPEN → GPIO39 = HIGH
- Clutch pulled: Switch CLOSED → GPIO39 = LOW

MOUNTING:
- Mount on clutch lever perch
- Adjust so switch closes at 90% clutch pull
```

### 6. Wheel Speed Sensors (Traction Control)

```
Front Wheel Hall Sensor
┌──────────────┐
│  VCC         │──── +5V
│  GND         │──── GND
│  OUT         │──┬─── [R9: 4.7kΩ] ──── +3.3V
└──────────────┘  │
                  ├─── [C10: 100nF] ──── GND
                  │
                  └─── GPIO22 (Front Wheel)

Rear Wheel Hall Sensor
(Same circuit)
└──────────────────── GPIO14 (Rear Wheel)

MAGNET PLACEMENT:
- Use 4-6 neodymium magnets (5mm x 2mm)
- Mount on brake disc or sprocket carrier
- Equal spacing around circumference
- Gap: 1-3mm from sensor face

SIGNAL:
- Each magnet pass: Pulse LOW (magnet detected)
- Frequency increases with wheel speed
- Code measures pulse frequency → speed in km/h
```

### 7. MPU9250 IMU (Anti-Wheelie)

```
MPU9250 Module
┌──────────────┐
│  VCC         │──── +3.3V (NOT 5V!)
│              │
│  GND         │──── GND
│              │
│  SDA         │──┬─── [R10: 4.7kΩ] ──┬─── +3.3V
│              │  │                    │
│  SCL         │──┼─── [R11: 4.7kΩ] ──┘
│              │  │
│  AD0 (Addr)  │──┤─── GND (I2C addr = 0x68)
└──────────────┘  │
                  │
            ┌─────┴──────┐
            │   ESP32    │
            │  GPIO21 ───┤ SDA
            │  GPIO22 ───┤ SCL
            └────────────┘

I2C PULLUPS:
- R10, R11 (4.7kΩ): Required if not present on MPU9250 module
- Most modules have onboard pullups, check first!

CRITICAL:
- **ONLY 3.3V!** 5V will damage the sensor permanently!
- Rigid mounting (no vibration isolation needed)
- Mount with Y-axis pointing forward (bike direction)
- Calibrate with bike level (sidestand, flat surface)
```

### 8. User Interface Buttons

```
MAP_UP Button (GPIO33)
    ┌───┐
    │ ● │ 6x6mm Tactile Switch
    └─┬─┘
      ├───┬─── GPIO33 (Input_Pullup)
      │   │
     GND [C11: 100nF] ── GND

MAP_DOWN Button (GPIO32)
(Same circuit)
      └─── GPIO32

MAP_SELECT Button (GPIO25)
(Same circuit)
      └─── GPIO25

PCB LAYOUT TIP:
- Add 100nF cap close to ESP32 pin (debounce)
- Use panel-mount buttons with wire leads
- Or SMD tactile switches directly on PCB
```

---

## 🖨️ PCB Layout Guide

### Layer Stack (2-Layer PCB Recommended)

```
TOP LAYER (Component Side):
┌─────────────────────────────────────────────┐
│  ┌─────────┐        ┌──────────┐           │
│  │  ESP32  │        │ LM2596   │  [F1][F2] │
│  │  Module │        │  Buck    │           │
│  └─────────┘        └──────────┘           │
│                                             │
│  [R1-R11]  [C1-C11]  [D1-D2]               │
│                                             │
│  ┌─────────┐        ┌──────────┐           │
│  │ MOSFET  │        │ SD Card  │           │
│  │ Driver  │        │  Slot    │           │
│  └─────────┘        └──────────┘           │
│                                             │
│  [Buttons]  [Connectors]  [Status LEDs]    │
└─────────────────────────────────────────────┘

BOTTOM LAYER (Ground Plane):
┌─────────────────────────────────────────────┐
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│  ▓▓▓▓▓▓▓▓▓ GROUND PLANE ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│  ▓▓▓▓ (Pour copper, connect to GND) ▓▓▓▓▓▓ │
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
└─────────────────────────────────────────────┘
```

### Critical Design Rules

**Trace Widths:**
```
- 12V Power (pre-buck): 1.5mm (50 mils) minimum
- 5V Rail: 1.0mm (40 mils)
- 3.3V Rail: 0.8mm (30 mils)
- Ignition coil (-): 2.0mm (80 mils) - HIGH CURRENT!
- Signal traces: 0.3mm (12 mils)
- Ground plane: Maximum copper pour
```

**Clearances:**
```
- Trace to trace: 0.3mm (12 mils) minimum
- High voltage (coil): 1.0mm (40 mils) from other traces
- Power to signal: 0.5mm (20 mils)
```

**Component Placement:**

1. **Power Section (Top-Left):**
   - Buck converter (LM2596)
   - Input caps (C1: 1000µF, C2: 470µF)
   - Fuses (F1, F2) near input
   - TVS diode (D1) close to input connector

2. **ESP32 Section (Center):**
   - ESP32 module with 0.1" headers
   - Decoupling caps (C3, C4) VERY close to VCC pins
   - Keep analog traces (GPIO34, 36, 39) away from digital

3. **Ignition Driver (Top-Right):**
   - MOSFET with heatsink pad
   - Gate resistor (R1) close to MOSFET gate
   - Pulldown (10kΩ) close to gate
   - Flyback diode (D2) close to coil connector
   - **IMPORTANT:** Keep away from sensor section!

4. **Sensor Section (Bottom):**
   - Screw terminals or JST-XH connectors
   - AC sensor circuit (bias resistors R3-R5)
   - Filter caps (C6-C11) close to input pins
   - I2C pullups (R10, R11) near ESP32

5. **Peripherals (Right):**
   - SD card slot
   - User buttons (with debounce caps)
   - Status LEDs with resistors

**Grounding Strategy:**

```
STAR GROUND TOPOLOGY:

Power GND ──┬── Buck Converter GND
            │
            ├── ESP32 GND (multiple points)
            │
            ├── MOSFET Source (HEAVY trace!)
            │
            ├── Sensor GND (isolated from power GND)
            │
            └── Battery Negative (main ground point)

- Avoid ground loops: Connect grounds at ONE central point
- Use thick traces for power grounds (2mm+)
- Sensor grounds separate from power grounds (minimize noise)
```

### EMI/Noise Reduction

**Shielding:**
```
- Ignition coil trace: Route on BOTTOM layer, away from sensors
- Analog inputs: Guard rings (GND trace surrounding signal)
- High-speed signals: Differential pairs if possible
```

**Filtering:**
```
- Place bypass caps (0.1µF) on EVERY IC power pin
- Bulk caps (10-100µF) on power rails
- Ferrite beads on power supply lines (optional, for severe noise)
```

**PCB Stack:**
```
Layer 1 (Top):    Components + Signal traces
Layer 2 (Bottom): Ground plane (solid copper pour)

For 4-layer PCB (better noise immunity):
Layer 1: Components + Signals
Layer 2: Ground plane
Layer 3: Power plane (5V, 3.3V zones)
Layer 4: Signals + Shield traces
```

---

## 📦 Bill of Materials (BOM)

### Core Components

| Ref | Description | Value | Package | Qty | Notes |
|-----|-------------|-------|---------|-----|-------|
| U1 | ESP32 Dev Module | 30-pin | DIP | 1 | ESP32-DevKitC or compatible |
| U2 | Buck Converter | LM2596 | Module | 1 | 12V→5V, 3A, adjustable |
| Q1 | N-MOSFET | IRFZ44N | TO-220 | 1 | Vgs(th)<3V, Rds(on)<0.03Ω |
| D1 | TVS Diode | P6KE15A | DO-201 | 1 | 15V bidirectional |
| D2 | Flyback Diode | 1N4007 | DO-41 | 1 | 1A, 1000V |

### Passive Components

| Ref | Description | Value | Package | Qty | Notes |
|-----|-------------|-------|---------|-----|-------|
| R1 | Resistor | 1kΩ | 0805/TH | 1 | MOSFET gate limiter |
| R2 | Resistor | 1MΩ | 0805/TH | 1 | AC sensor DC block |
| R3-R5 | Resistor | 10kΩ | 0805/TH | 3 | AC sensor bias |
| R6,R9 | Resistor | 4.7kΩ | 0805/TH | 2 | Hall sensor pullups |
| R7 | Resistor | 4.7kΩ | 0805/TH | 1 | QS divider (if 5V sensor) |
| R8 | Resistor | 10kΩ | 0805/TH | 1 | QS divider (if 5V sensor) |
| R10-R11 | Resistor | 4.7kΩ | 0805/TH | 2 | I2C pullups |
| R12 | Resistor | 10kΩ | 0805/TH | 1 | MOSFET pulldown |
| C1 | Electrolytic Cap | 1000µF/16V | Radial | 1 | Input filter |
| C2 | Electrolytic Cap | 470µF/6.3V | Radial | 1 | 5V rail bulk |
| C3-C4 | Ceramic Cap | 100µF/6.3V | 0805 | 2 | ESP32 decoupling |
| C5 | Ceramic Cap | 10nF | 0805 | 1 | AC coupling |
| C6 | Ceramic Cap | 100pF | 0805 | 1 | AC noise filter (optional) |
| C7-C11 | Ceramic Cap | 100nF | 0805 | 5 | Debounce/bypass |

### Connectors

| Ref | Description | Type | Qty | Notes |
|-----|-------------|------|-----|-------|
| J1 | Power Input | 2-pin screw terminal | 1 | 5mm pitch, 15A rated |
| J2 | Ignition Coil | 2-pin screw terminal | 1 | 5mm pitch, 10A rated |
| J3 | AC Pickup | JST-XH 2-pin | 1 | Or screw terminal |
| J4 | DC Hall | JST-XH 3-pin | 1 | VCC, GND, OUT |
| J5 | QS Sensor | JST-XH 3-pin | 1 | VCC, GND, OUT |
| J6 | Clutch Switch | JST-XH 2-pin | 1 | Or screw terminal |
| J7-J8 | Wheel Sensors | JST-XH 3-pin | 2 | Front + Rear |
| J9 | MPU9250 | JST-XH 4-pin | 1 | VCC, GND, SDA, SCL |

### Protection & Misc

| Ref | Description | Value | Qty | Notes |
|-----|-------------|-------|-----|-------|
| F1 | Blade Fuse | 10A | 1 | 5V rail protection |
| F2 | Blade Fuse | 5A | 1 | Coil circuit protection |
| HS1 | Heatsink | TO-220 | 1 | For MOSFET (if needed) |
| SD1 | microSD Slot | Push-push | 1 | Standard footprint |
| SW1-SW3 | Tactile Switch | 6x6mm | 3 | Map buttons |
| LED1-LED3 | Status LED | 3mm/5mm | 3 | Red, Yellow, Green |

### Sensors (External, not on PCB)

| Item | Description | Qty | Notes |
|------|-------------|-----|-------|
| Hall Sensor | A3144 or SS441A | 3 | DC pickup + wheels |
| IMU Module | MPU9250 breakout | 1 | GY-9250 or compatible |
| QS Sensor | Pressure/strain 0-3.3V | 1 | HT-SCS-01 or load cell |
| Microswitches | SPST NO | 2 | Clutch + optional QS backup |

### Estimated Cost

| Category | Cost (USD) |
|----------|------------|
| ESP32 Module | $5-8 |
| Buck Converter | $2-3 |
| MOSFET & Diodes | $2-3 |
| Passives (R, C) | $3-5 |
| Connectors | $5-8 |
| PCB (5pcs from JLCPCB) | $5-10 |
| **Subtotal (PCB only)** | **$22-37** |
| | |
| Sensors (Hall x3) | $3-6 |
| MPU9250 Module | $5-8 |
| QS Pressure Sensor | $15-30 |
| Enclosure (IP65) | $10-15 |
| **Total (Complete)** | **$55-96** |

---

## 🔧 Assembly Instructions

### Step 1: PCB Preparation
1. Inspect PCB for defects (shorts, broken traces)
2. Clean with isopropyl alcohol
3. Mark component reference designators if needed

### Step 2: Solder SMD Components (if using)
**Order: Smallest to Largest**
1. Resistors (0805)
2. Ceramic caps (0805)
3. Diodes (SOD-123)
4. Larger components

**Technique:**
- Use 0.5mm solder wire
- 300-350°C soldering iron
- Flux pen recommended

### Step 3: Through-Hole Components
**Order:**
1. Low-profile first:
   - Resistors (if TH)
   - Diodes (D1, D2)
   - Ceramic caps (if TH)
2. Medium height:
   - Screw terminals
   - JST-XH connectors
   - Tactile switches
3. Tall components:
   - Electrolytic caps (C1, C2)
   - MOSFET Q1 (leave heatsink off for now)
4. Last:
   - ESP32 module (use socket headers)
   - SD card slot

### Step 4: ESP32 Module Preparation
```
DO NOT solder ESP32 directly!
Use 2x15 female pin headers (sockets)

Why:
- Easy replacement if damaged
- Can program off-board
- Reusable for testing
```

### Step 5: MOSFET Heatsink
```
If coil current >3A or prolonged high RPM:
1. Apply thermal paste to MOSFET back
2. Mount TO-220 heatsink (use screw + insulating washer if grounding matters)
3. Check continuity: Heatsink should NOT connect to MOSFET tab (isolated)
```

### Step 6: Inspection
- [ ] Visual check for solder bridges
- [ ] Continuity test: VCC to GND (should be >10kΩ, not short!)
- [ ] Check all polarized components (caps, diodes, MOSFETs)

### Step 7: Initial Power Test (NO ESP32 YET!)
1. Set buck converter to 5.0V (adjust potentiometer, use multimeter)
2. Connect 12V bench supply (current limit 1A)
3. Measure 5V rail → Should be 4.9-5.1V
4. Check for hot components (if any, STOP and debug)
5. Power off

### Step 8: ESP32 Installation
1. Insert ESP32 into socket headers
2. Power on
3. Measure 3.3V pin → Should be 3.2-3.4V (from onboard LDO)
4. Check current draw → Should be <200mA idle

### Step 9: Firmware Upload
1. Connect USB to ESP32 module
2. Upload Smart CDI firmware
3. Check serial monitor for boot messages
4. Verify SD card detection
5. Test WiFi AP mode (connect to `SmartCDI-Config`)

### Step 10: Sensor Connections (Bench Test)
**Do NOT connect ignition coil yet!**

Connect sensors one by one:
1. AC pickup → GPIO34 (monitor ADC value in dashboard)
2. Hall sensor → GPIO35 (check toggle HIGH/LOW)
3. QS sensor → GPIO36 (verify pressure readings)
4. Wheel sensors → GPIO14, 22 (spin by hand, check pulses)
5. MPU9250 → I2C (verify I2C communication, check pitch values)

### Step 11: Ignition Output Test (LED Test)
```
DO NOT connect coil for first test!

Instead:
GPIO4 → 330Ω resistor → LED → GND

Crank engine (or simulate with trigger):
- LED should blink at firing rate
- Use oscilloscope to verify timing (compare with advance setting)
```

### Step 12: Coil Connection (Final Step)
1. Connect ignition coil (-)  to MOSFET drain
2. Connect coil (+) to +12V battery
3. Verify flyback diode polarity (cathode to +12V)
4. Ground spark plug to engine
5. Crank → Should see spark
6. Use timing light to verify advance angle

---

## ✅ Testing & Validation

### Pre-Flight Checklist

**Power Supply:**
- [ ] 12V input: 10-14.5V range tested
- [ ] 5V rail: 4.9-5.1V under load
- [ ] 3.3V rail: 3.2-3.4V
- [ ] Current draw: <1A idle, <3A max
- [ ] No overheating (buck converter, MOSFET)

**Ignition Output:**
- [ ] GPIO4 output: 0V (idle), 3.3V (firing)
- [ ] MOSFET switching: Clean edges on oscilloscope
- [ ] Flyback diode: No voltage spikes >20V on coil (-)
- [ ] Coil current: Measure with clamp meter (should be 3-8A depending on coil)
- [ ] Spark intensity: Strong blue spark at plug gap

**Sensors:**
- [ ] AC pickup: 0-3.3V swing at GPIO34
- [ ] DC hall: HIGH/LOW toggle at GPIO35
- [ ] QS sensor: Values change when pressed (ADC >threshold)
- [ ] Clutch switch: LOW when pulled, HIGH when released
- [ ] Front wheel: Pulses when spun, frequency matches speed
- [ ] Rear wheel: Same as front
- [ ] MPU9250: I2C responds (addr 0x68), pitch values update

**Communication:**
- [ ] SD card: Detected on boot
- [ ] Maps: Load from SD correctly
- [ ] WiFi AP: SSID visible, can connect
- [ ] Web dashboard: Loads, shows live RPM
- [ ] OTA update: Test firmware upload via WiFi

**Safety Features:**
- [ ] Rev limiter: Triggers at correct RPM
- [ ] Emergency shutdown: Can trigger via dashboard
- [ ] Low voltage: Cuts spark at <10.5V
- [ ] Watchdog: ESP32 resets if hung (test by infinite loop)

### Functional Tests

**Test 1: Static Timing Check**
```
Tools: Timing light, timing marks on flywheel

1. Set map to known advance (e.g., 20° BTDC at 3000 RPM)
2. Run engine at 3000 RPM (verify with tachometer)
3. Point timing light at flywheel
4. Timing mark should align with 20° BTDC mark
5. Repeat at different RPMs (check entire curve)
```

**Test 2: Rev Limiter**
```
1. Set rev limiter to 8000 RPM
2. Run engine and accelerate
3. At 8000 RPM, ignition should cut (listen for misfire)
4. Test all 4 cut patterns (0, 1, 2, 3)
5. Verify smooth limiter action (no harsh bouncing)
```

**Test 3: Quick Shifter**
```
1. Set kill time to 80ms
2. Run engine at 5000 RPM (stationary, wheels off ground)
3. Press QS sensor
4. Ignition should cut for exactly 80ms (verify with scope)
5. Test at different RPMs (verify kill time interpolation)
```

**Test 4: Launch Control**
```
1. Set LC target to 6000 RPM
2. Bike stationary, clutch pulled
3. Throttle WOT
4. RPM should hold at ~6000 (±200 RPM)
5. Release clutch → LC should deactivate as speed increases
```

**Test 5: Anti-Wheelie**
```
1. Calibrate IMU with bike level
2. Lift front wheel (on stand or jack)
3. Monitor pitch angle in dashboard
4. Pitch should read ~15-25° (depending on lift height)
5. If AW enabled, timing should retard when threshold exceeded
```

**Test 6: Traction Control**
```
1. Mount bike on rear stand
2. Spin rear wheel by hand (faster than front)
3. Monitor slip ratio in dashboard
4. If slip >threshold, TC should activate (timing retards)
5. Verify calculation accuracy with GPS speed (road test)
```

**Test 7: Data Logging**
```
1. Enable logging in dashboard
2. Run engine through full RPM range
3. Perform quick shift, launch, wheelie
4. Download log from dashboard
5. Verify CSV contains: timestamp, RPM, advance, features active
```

### Oscilloscope Waveforms (Expected)

**GPIO4 (Ignition Output):**
```
Advance = 20° BTDC, RPM = 6000

     3.3V ┤     ┌─────┐     ┌─────┐     ┌─────┐
          │     │     │     │     │     │     │
          │     │     │     │     │     │     │
     0V   └─────┘     └─────┘     └─────┘     └───
          <─────── 10ms ──────> (60,000 / 6000 RPM)

          ↑
      Fire point (20° before TDC)

Pulse width = dwellTime (e.g., 3ms)
```

**Coil (-) Voltage (with flyback diode):**
```
    +12V ┤
         │
         │
     0V  ├─────┐              ┌─────
         │     │              │
   -40V  │     └──────────────┘  ← Inductive kickback (clamped by diode)

         └─────┘
         Coil ON (energizing)
```

**AC Pickup Sensor (GPIO34):**
```
ADC  4095 ┤        ╱╲
          │       ╱  ╲
     2048 ├──────┼────┼────── Threshold (mid-rail bias)
          │     ╱      ╲
        0 ┤    ╱        ╲

          ↑
    Trigger point (threshold crossing)
```

---

## 🛡️ Production Quality Checks

### Visual Inspection (100% of boards)
- [ ] No solder bridges
- [ ] All components present and correct orientation
- [ ] No damaged traces
- [ ] Clean flux residue (if using rosin flux)

### Electrical Test (100% of boards)
- [ ] Continuity: VCC to GND >10kΩ (no shorts)
- [ ] 5V rail voltage: 4.9-5.1V
- [ ] 3.3V rail voltage: 3.2-3.4V
- [ ] Ignition output: Toggles correctly

### Functional Test (Sampling)
- [ ] WiFi AP starts successfully
- [ ] SD card detected
- [ ] All sensors read correctly
- [ ] Spark output verified with LED

### Burn-In Test (Optional, for critical applications)
```
Run assembled unit for 24 hours:
- Continuous WiFi connection
- Simulate RPM input (signal generator)
- Log all data
- Monitor temperature (should be <60°C ambient)
- Check for intermittent failures
```

### Environmental Test (Optional)
```
- Vibration: Shake test (simulate bike vibration)
- Temperature: -10°C to +80°C operating range
- Humidity: 90% RH (sealed enclosure required)
- Water: IP65 splash test (conformal coating recommended)
```

---

## 📏 PCB Dimensions & Mounting

### Recommended PCB Size
```
100mm x 80mm (compact)
120mm x 100mm (with extra connectors/features)

Mounting Holes:
- 4x M3 holes at corners
- 5mm from edge
- Grounded via stitching if possible
```

### Enclosure Requirements
```
Material: Aluminum or ABS plastic
Rating: IP65 (dust-tight, water-resistant)
Ventilation: Mesh-covered holes (prevent moisture)
Cable glands: PG7 size for all wire entry points

Dimensions (internal):
- Minimum: 110mm x 90mm x 30mm (height)
- Recommended: 130mm x 110mm x 40mm (allows airflow)
```

---

## 🔒 Conformal Coating (Recommended)

**For harsh environments (racing, off-road):**
```
Coating Type: Acrylic or silicone conformal coating
Application: Spray or brush-on
Coverage: All PCB except:
  - Connectors
  - SD card slot
  - Programming headers

Process:
1. Clean PCB with isopropyl alcohol
2. Mask connectors with Kapton tape
3. Apply 2-3 thin coats (allow drying between coats)
4. Cure per datasheet (24-48 hours room temp)
5. Remove masking tape

Benefits:
- Moisture protection
- Prevents corrosion
- Vibration damping (slightly)
```

---

## 📖 References & Standards

**PCB Design:**
- IPC-2221: Generic PCB Design Standards
- IPC-6012: Qualification and Performance Specification

**Automotive Electronics:**
- ISO 7637: Electrical transient immunity
- ISO 16750: Environmental conditions

**Safety:**
- CE marking requirements (if selling in EU)
- FCC Part 15 (EMI/RFI compliance)

---

**End of Schematic Document**

For assembly questions, see Assembly Instructions section.
For troubleshooting, see main README.md.

**Version:** 2.2
**Last Updated:** 2025-12-04
**Platform:** ESP32 Development Board
