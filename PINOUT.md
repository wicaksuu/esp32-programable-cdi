# Smart CDI Programmable - Detailed Pinout Guide

## 📌 Pin Assignment Table

### Complete Pin Mapping

| ESP32 GPIO | Function | Direction | Pull | Voltage | Description |
|------------|----------|-----------|------|---------|-------------|
| **GPIO 4** | IGNITION_OUTPUT | Output | - | 3.3V | Ignition coil trigger |
| **GPIO 5** | SD_CS | Output | - | 3.3V | SD Card chip select |
| **GPIO 14** | REAR_WHEEL_SENSOR | Input | UP | 3.3V | Rear wheel hall sensor |
| **GPIO 18** | SD_SCK | Output | - | 3.3V | SPI clock (SD + TFT) |
| **GPIO 19** | SD_MISO | Input | - | 3.3V | SPI data in (SD + TFT) |
| **GPIO 21** | TFT_BL / I2C_SDA | Output / Bidir | - | 3.3V | TFT backlight OR I2C data |
| **GPIO 22** | FRONT_WHEEL_SENSOR / I2C_SCL | Input / Output | UP | 3.3V | Front wheel sensor OR I2C clock |
| **GPIO 23** | SD_MOSI | Output | - | 3.3V | SPI data out (SD + TFT) |
| **GPIO 25** | MAP_SELECT_BTN | Input | UP | 3.3V | Map selection button |
| **GPIO 26** | MODE_SELECT_PIN | Input | UP | 3.3V | AC/DC mode select |
| **GPIO 27** | QS_INPUT_PIN | Input | UP | 3.3V | Quick shifter digital input |
| **GPIO 32** | MAP_DOWN_BTN | Input | UP | 3.3V | Map down button |
| **GPIO 33** | MAP_UP_BTN | Input | UP | 3.3V | Map up button |
| **GPIO 34** | AC_SENSOR_PIN | Input (ADC) | - | 0-3.3V | AC pickup sensor (analog) |
| **GPIO 35** | DC_SENSOR_PIN | Input | - | 3.3V | DC pickup sensor (digital) |
| **GPIO 36** | QS_PRESSURE_PIN | Input (ADC) | - | 0-3.3V | QS pressure sensor (analog) |
| **GPIO 39** | CLUTCH_SENSOR_PIN | Input | UP | 3.3V | Clutch position sensor |

**Legend:**
- **UP** = Internal pullup enabled
- **ADC** = Analog input (0-3.3V, 12-bit resolution)
- **Bidir** = Bidirectional (I2C)

---

## 🔌 Detailed Wiring Schematics

### 1. Ignition Output (CRITICAL)

```
┌─────────────┐
│   ESP32     │
│   GPIO 4    ├──┐
└─────────────┘  │
                 │ 1kΩ
                 ├─────┬──────┐
                 │     │      │
              ┌──┴──┐  │    ┌─┴─┐ 10kΩ
              │Gate │  │    │   │
              │     │  │    └─┬─┘
              │MOSFET │      │
              │IRFZ44N│      GND
              │     │  │
         Drain├──┬──┘  │
              │  │     │
         Source│  │     │
              └──┴─────┴────── GND
                 │
                 │
            ┌────┴────┐
            │ Ignition│
            │  Coil   │
            │ Primary │
            │   (-)   │
            └────┬────┘
                 │
              ┌──┴──┐
              │Diode│ 1N4007 (flyback protection)
              │ │▌─ │
              └──┬──┘
                 │
                 └─────────── +12V Battery

Ignition Coil Primary (+) ────── +12V Battery
```

**CRITICAL Notes:**
- Use logic-level MOSFET (Vgs(th) < 3V)
- IRFZ44N, IRLZ44N, or similar
- **MUST** add flyback diode (1N4007 or better)
- Gate resistor limits inrush current
- 10kΩ pulldown keeps MOSFET off when ESP32 boots

---

### 2. AC Sensor (Pickup Coil) Circuit

```
AC Pickup Coil        Voltage Divider Bias
(from alternator)
                           +3.3V
    ~                        │
    │                       10kΩ
    ├─────[10nF]────┬────────┤
    │               │        │
    ~              1MΩ      10kΩ
                    │        │
                    GND   ───┴─── GPIO34 (ADC)
                             │
                            10kΩ
                             │
                            GND

Components:
- 10nF ceramic cap: AC coupling
- 1MΩ: DC blocking, prevents offset drift
- 10kΩ + 10kΩ: Bias to 1.65V (mid-rail)
- 10kΩ to GND: Input impedance, noise reduction

ADC reads:
- Idle: ~1.65V (midpoint, ADC ~2048)
- Pulse: swings 0-3.3V
- Threshold: 2048 (trigger on crossing)
```

**Tuning:**
- Weak signal? Reduce bottom 10kΩ to 4.7kΩ (more gain)
- Too much noise? Add 100pF cap across GPIO34-GND
- Invert signal: Set `acInvertSignal: true` in config

---

### 3. DC Sensor (Hall Effect / Points)

```
Hall Effect Sensor (A3144, SS441A)

       Hall Sensor
     ┌─────────────┐
VCC  │ 1    VCC    │──── +5V (or 3.3V, check datasheet)
     │             │
GND  │ 2    GND    │──── GND
     │             │
OUT  │ 3    OUT    │──┬─ GPIO35
     └─────────────┘  │
                     100nF
                      │
                     GND

For Points/Magneto (mechanical):

   Points
     /│
    / │
   ─  ├─────┬─── GPIO35
      │    100nF
      │     │
     GND   GND

- Enable internal pullup in code
- 100nF cap debounces noise
- Points close → GPIO35 = LOW
- Points open → GPIO35 = HIGH (pulled up)
```

**Hall Sensor Selection:**
- **Digital**: A3144 (latching), SS441A (switching)
- **Supply**: 3.3V or 5V (check spec)
- **Output**: Open-collector (needs pullup)
- **Gap**: 1-3mm from magnet

---

### 4. Quick Shifter Sensors

#### A. Pressure/Strain Sensor (Analog)

```
┌──────────────────┐
│ QS Pressure      │
│  Sensor          │
│  (0-5V output)   │
└────┬────┬────┬───┘
     │    │    │
    VCC  GND  OUT
     │    │    │
    +5V  GND   ├────[Divider]──── GPIO36 (ADC)
               │
              4.7kΩ (if 5V sensor)
               │
              10kΩ
               │
              GND

If sensor outputs 0-5V, use divider to scale to 0-3.3V:
- Divider ratio: 10k/(4.7k+10k) = 0.68
- 5V → 3.4V (safe, slightly over but ESP32 tolerant)

Better: Use 0-3.3V sensor directly
```

#### B. Digital Switch (Backup/Redundant)

```
   Microswitch (NO)
      │
      ├───────── GPIO27
      │
     GND

- Pullup enabled in code
- Switch closes on shifter movement
- GPIO27 = LOW when shifting
```

**Sensor Recommendations:**
- **Best**: HT-SCS-01 (strain gauge, 0-3.3V)
- **Budget**: Load cell + HX711 (I2C/SPI, slower)
- **DIY**: Microswitch (binary, less precise)

---

### 5. Launch Control Sensors

#### Clutch Sensor

```
   Lever Mount          Clutch Cable Housing
      │                        │
   ┌──┴───┐                 ┌──┴───┐
   │ ┌──┐ │                 │ ┌──┐ │
   │ │  │ │                 │ │  │ │
   │ └──┘ │                 │ └──┘ │
   │Switch│◄────────────────┤Actuator
   └──┬───┘                 └──────┘
      │
   Wiring:
    Common ──── GPIO39 (pullup enabled)
    NO ──────── GND

Operation:
- Clutch released: Switch OPEN → GPIO39 = HIGH
- Clutch pulled: Switch CLOSED → GPIO39 = LOW

Mounting:
- Mount on clutch lever or at clutch actuator
- Adjust so switch closes when clutch 90% pulled
- Use normally-open (NO) contacts
```

**Alternative**: Hall sensor + magnet on lever

---

### 6. Wheel Speed Sensors (Traction Control)

```
Front Wheel:
                        Brake Disc with 4 magnets
                             ┌─────┐
    Hall Sensor              │  ●  │
    ┌─────────┐             ● ─── ●
VCC │1   VCC  │── +5V        │  ●  │
    │         │              └─────┘
GND │2   GND  │── GND          ▲
    │         │                │
OUT │3   OUT  │── GPIO22      2mm gap
    └─────────┘

Rear Wheel:
   (Same circuit, use GPIO14)

Magnet Placement:
- Mount 4-6 small neodymium magnets
- Equal spacing around disc/sprocket
- Use double-sided tape or epoxy
- Distance: 1-3mm from sensor face

Sensor Mounting:
- Rigid mount (no vibration)
- Waterproof connector recommended
- Route cable away from ignition wires (EMI)
```

**Magnet Count Configuration:**
```json
"tractionControl": {
  "frontWheelHoles": 4,  // Number of magnets
  "rearWheelHoles": 4
}
```

**Speed Calculation:**
- Pulses per revolution = magnet count
- Circumference = π × (wheel diameter + tire height)
- Speed = (pulses/sec ÷ magnets) × circumference × 3.6 km/h

---

### 7. IMU (MPU9250) - Anti-Wheelie

```
┌─────────────┐        ┌────────────┐
│   ESP32     │        │  MPU9250   │
│             │        │            │
│   GPIO21 ───┼────────┤ SDA        │
│        (SDA)│   ┌───►│            │
│             │   │    │ SCL        │
│   GPIO22 ───┼───┘    │            │
│        (SCL)│        │ VCC        │──── +3.3V
│             │        │            │
│        GND ─┼────────┤ GND        │──── GND
└─────────────┘        │            │
                       │ AD0        │──── GND (address 0x68)
                       └────────────┘

I2C Pullup Resistors (if not on module):
         +3.3V
           │
         4.7kΩ
           ├──── SDA
         4.7kΩ
           ├──── SCL
           │
          GND

Mounting Orientation:
         ┌─────────────┐
         │   MPU9250   │
         │   ●   ●     │
         │   Front     │  ← Forward (bike direction)
         │             │
         │      ▲      │
         │      │Z     │  ← Z-axis UP (perpendicular to ground)
         │      │      │
         └─────────────┘
         Y-axis: Forward
         X-axis: Left-Right
         Z-axis: Up-Down

CRITICAL:
- Use 3.3V ONLY (5V will damage sensor!)
- Rigid mounting (no vibration isolation)
- Calibrate with bike level (sidestand down, on flat surface)
```

---

### 8. User Interface Buttons

```
MAP_UP Button:
   ┌───┐
   │ ● │ Tactile Switch (6x6mm)
   └─┬─┘
     ├────── GPIO33 (pullup enabled)
     │
    GND

MAP_DOWN Button:
   (Same circuit, GPIO32)

MAP_SELECT Button:
   (Same circuit, GPIO25)

Mounting:
- Panel-mount pushbuttons
- Or tactile switches on PCB
- Add 100nF cap across each button (debounce)
- Use momentary (NO) switches

Optional: Rotary encoder instead of UP/DOWN
```

---

### 9. TFT Display (Example: ILI9341)

```
TFT Pin    ESP32 Pin    Notes
─────────  ──────────   ──────────────────────
VCC   ───  3.3V/5V      Check display voltage!
GND   ───  GND
CS    ───  GPIO15       (configurable in TFT_eSPI)
RESET ───  GPIO2        (configurable)
DC    ───  GPIO4        (configurable, conflicts with IGNITION!)
SCK   ───  GPIO18       (shared with SD card)
MOSI  ───  GPIO23       (shared with SD card)
MISO  ───  GPIO19       (shared with SD card, optional)
LED   ───  GPIO21       Backlight (PWM capable)

⚠️ IMPORTANT: Resolve GPIO4 conflict!
- Either use different pin for TFT_DC
- Or use different pin for IGNITION_OUTPUT
- Recommended: Move TFT_DC to GPIO16
```

**TFT_eSPI Configuration** (`User_Setup.h`):
```cpp
#define ILI9341_DRIVER
#define TFT_CS   15
#define TFT_DC   16  // Changed from 4 to avoid conflict!
#define TFT_RST  2
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
```

---

### 10. SD Card Module

```
SD Module    ESP32
──────────   ──────────
VCC    ────  3.3V (NOT 5V!)
GND    ────  GND
MISO   ────  GPIO19 (shared SPI)
MOSI   ────  GPIO23 (shared SPI)
SCK    ────  GPIO18 (shared SPI)
CS     ────  GPIO5

Optional: Add 10kΩ pullup on MISO if card detection flaky

SPI Bus Sharing:
- SD Card + TFT share same SPI bus
- Each has separate CS (chip select)
- SD_CS = GPIO5
- TFT_CS = GPIO15 (or per your TFT_eSPI config)
```

---

## 🔋 Power Supply Design

### Recommended Circuit

```
Motorcycle Battery (12V, can fluctuate 10-14.5V)
   │
   ├── Fuse 10A ──┬── LM2596 Buck Converter ──► +5V (3A)
   │              │   (12V → 5V)                   │
   │              │                                 ├─► ESP32 VIN (5V)
   │              │                                 ├─► Hall Sensors (5V)
   │              │                                 ├─► TFT Display (5V)
   │              │                                 └─► SD Card (via 3.3V regulator)
   │              │
   │              └── 1000μF 16V capacitor (input filter)
   │
   └── Fuse 5A ───┬── Relay/MOSFET ────► Ignition Coil +12V
                  │
                  └── 1N4007 Diode (reverse polarity protection)

ESP32 Internal:
   5V (VIN) ──► LDO 3.3V ──► GPIO, ADC, Logic
```

**Component Selection:**
- **Buck Converter**: LM2596 module (Ebay/AliExpress ~$2)
  - Input: 4.5-40V
  - Output: 5V, 3A continuous
  - Efficiency: ~85%

- **Fuses**:
  - 5V Rail: 10A blade fuse (protects electronics)
  - 12V Coil: 5A blade fuse (protects coil circuit)

- **Capacitors**:
  - Input: 1000μF 16V (smooths battery ripple)
  - Output: 470μF 6.3V (reduces buck converter noise)
  - ESP32 VIN: 100μF 6.3V (local decoupling)

- **Protection**:
  - Reverse polarity: 1N4007 diode (or P-MOSFET for less drop)
  - TVS diode: P6KE15A (suppresses voltage spikes)
  - Fuses on all power rails

---

## 🛡️ EMI Protection & Grounding

### Noise Sources in Motorcycle:
1. **Ignition coil**: 20-40kV spikes
2. **Alternator**: AC ripple, commutator noise
3. **Fuel pump**: inductive kickback
4. **Radiator fan**: motor brush noise

### Protection Strategy:

```
Sensor Wiring (Hall, QS, etc.):
┌─────────┐    Shielded Cable    ┌─────────┐
│ Sensor  │════════════════════→ │  ESP32  │
│         │    ╔════════════╗    │         │
│  Signal ├────║   Signal   ║────┤ GPIO    │
│   GND   ├────║    GND     ║────┤  GND    │
│         │    ║   Shield   ║─┐  │         │
└─────────┘    ╚════════════╝ │  └─────────┘
                               └──── GND (one end only!)

Shield Grounding:
- Ground shield at ESP32 end ONLY
- Prevents ground loops
- Use twisted pair for signal + GND
```

**RC Filters on Inputs:**
```
From Sensor ──┬─── 100Ω ───┬─── GPIO
              │            │
             10nF         10nF
              │            │
             GND          GND

- 100Ω limits current
- First 10nF: HF noise filter
- Second 10nF: additional smoothing
- Cutoff freq: ~160kHz (fine for RPM signals)
```

**PCB Layout Tips:**
- Separate analog ground (AGND) and digital ground (DGND)
- Star grounding at power supply
- Keep high-voltage (coil) traces far from sensor traces
- Use ground plane on bottom layer
- Add ferrite beads on power supply lines

---

## 🔧 Assembly Checklist

### Before First Power-Up:

- [ ] Double-check ALL pin assignments match code
- [ ] Verify no short circuits (continuity test)
- [ ] Confirm power supply voltages:
  - [ ] 5V rail = 4.9-5.1V
  - [ ] 3.3V (ESP32 LDO) = 3.2-3.4V
- [ ] Check ignition circuit:
  - [ ] MOSFET gate resistor present (1kΩ)
  - [ ] Flyback diode correct polarity
  - [ ] Coil not connected yet (for testing)
- [ ] Sensor checks:
  - [ ] Hall sensors pull to 3.3V/5V (use multimeter)
  - [ ] AC sensor biased to ~1.65V
  - [ ] QS sensor reads mid-range at rest
- [ ] SD card:
  - [ ] Formatted FAT32
  - [ ] Files copied (maps, www)
- [ ] I2C devices:
  - [ ] MPU9250 powered from 3.3V (NOT 5V!)
  - [ ] Pullup resistors present

### Initial Testing Sequence:

1. **Power-up test** (no sensors):
   - USB power only
   - Serial monitor: check boot messages
   - TFT: should show "Smart CDI v2.0"

2. **SD card test**:
   - Insert SD card
   - Reboot
   - Serial: "SD Card OK"
   - Check maps loaded

3. **WiFi test**:
   - Connect to AP: `SmartCDI-Config`
   - Browse: `http://192.168.4.1`
   - Should load web interface

4. **Sensor test** (engine off):
   - Spin wheel by hand → Serial: "Front wheel pulse"
   - Pull clutch → Serial: check GPIO39 state
   - Press QS sensor → Serial: check ADC value

5. **Ignition test** (no coil!):
   - LED + 330Ω resistor on GPIO4
   - Crank engine → LED should blink at trigger rate

6. **Coil test** (spark plug grounded):
   - Connect ignition coil
   - Ground spark plug to engine
   - Crank → should see spark
   - Use timing light to verify timing

---

## ⚠️ Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| 5V to MPU9250 | Sensor dead, no I2C response | Use 3.3V only! |
| No flyback diode | ESP32 resets randomly | Add 1N4007 across coil |
| GPIO4 conflict (TFT_DC) | Ignition fires constantly | Move TFT_DC to GPIO16 |
| Wrong SPI pins | SD card not detected | Check wiring vs code |
| Pullup on analog pin | ADC reads 3.3V always | Remove pullup, use float |
| Shield grounded both ends | Noise worse, not better | Ground shield ONE end only |
| Weak 5V supply | Brownouts, resets | Use 3A capable regulator |
| Long wires, no shield | False triggers, noise | Use shielded twisted pair |

---

## 📐 Mechanical Mounting

### ESP32 Module Mounting
- Use M3 standoffs (10mm height)
- Anti-vibration: silicone washers under standoffs
- Waterproofing: conformal coating on PCB

### Sensor Mounting
- **Hall sensors**: Rigid bracket, no flex
- **IMU**: Solid mount to frame (not on handlebars!)
- **QS sensor**: Shifter rod or linkage
- **Clutch sensor**: Lever perch or cable mount

### Enclosure
- IP65 rated minimum (dust/water resistant)
- Aluminum or ABS plastic
- Ventilation holes with mesh (prevent moisture)
- Cable glands for all wires (PG7 size)

---

**End of Pinout Guide**

For configuration details, see `CONFIGURATION.md`
For troubleshooting, see `README.md` → Troubleshooting section
