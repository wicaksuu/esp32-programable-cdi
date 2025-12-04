/*
 * Smart CDI Programmable v2.2 SIMULATOR with ESP32
 * - Multiple mappings with Web UI CRUD
 * - Web file upload manager
 * - Quick shifter support (no gear position, RPM-based only)
 * - WiFi with auto-fallback to AP mode
 * - OLED 1.3" I2C 128x64 Display
 * - SIMULATOR MODE - Test CDI tanpa motor! 🎮
 * - Unit Test Suite via Web UI
 * - OPTIMIZED for 2-STROKE ENGINES 🏍️
 *
 * CRITICAL FIXES APPLIED:
 * - ISR timing optimization (pre-calculated advance)
 * - No analogRead() in ISR (pre-read sensor values)
 * - Timing calculation compensation
 * - Watchdog timer protection
 * - Safety bounds checking
 *
 * WiFi Config:
 * - Try connect to: wicaksu / wicaksuu
 * - Fallback to AP: SmartCDI-Config / 12345678
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * ⚡ FIRMWARE SIGNATURE - DO NOT MODIFY ⚡
 * Author: wicaksu
 * Copyright (c) 2025 wicaksu. All rights reserved.
 * Unauthorized modification or removal of this signature is prohibited.
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 */

#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// ===== WEB SERVER UPLOAD & DOWNLOAD SIZE LIMITS =====
// MUST be defined BEFORE #include <WebServer.h>
// Increase limits to support larger file uploads/downloads (100KB+)
#define HTTP_UPLOAD_BUFLEN 8192        // Upload buffer per chunk: 8KB (default 2KB)
#define HTTP_DOWNLOAD_UNIT_SIZE 8192   // Download/stream buffer: 8KB (default 1436 bytes)
#define HTTP_MAX_DATA_WAIT 10000       // Max wait for upload data: 10 sec
#define HTTP_MAX_POST_WAIT 10000       // Max wait for POST: 10 sec
#define HTTP_MAX_SEND_WAIT 10000       // Max wait for send: 10 sec
#define HTTP_MAX_CLOSE_WAIT 2000       // Max wait for close: 2 sec

#include <WebServer.h>
#include <MPU9250.h>
#include <esp_task_wdt.h>  // Watchdog timer
#include <Update.h>        // OTA firmware update

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// ⚡ IMMUTABLE FIRMWARE SIGNATURE ⚡
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
const char* const FIRMWARE_AUTHOR PROGMEM = "wicaksu";
const char* const FIRMWARE_COPYRIGHT PROGMEM = "Copyright (c) 2025 wicaksu";
const char* const FIRMWARE_VERSION PROGMEM = "2.2";
const unsigned long FIRMWARE_SIGNATURE_HASH = 0x57494341;  // "WICA" in hex
// WARNING: Modifying or removing this signature violates the license.
// This signature is embedded in multiple locations for integrity verification.
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// WiFi Credentials (defaults, will be loaded from SD card)
const char* STA_SSID = "Wicaksu";
const char* STA_PASS = "wicaksuu";
const char* AP_SSID = "SmartCDI-Config";
const char* AP_PASS = "12345678";

// WiFi Settings Structure
#define MAX_WIFI_NETWORKS 5
struct WiFiNetwork {
  char ssid[33];
  char password[64];
};

struct WiFiSettings {
  WiFiNetwork networks[MAX_WIFI_NETWORKS];
  int networkCount;
  bool apModeEnabled;
  char apSSID[33];
  char apPassword[64];
};

WiFiSettings wifiSettings;

// OLED Display Settings (I2C)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // No reset pin
#define SCREEN_ADDRESS 0x3C  // Common I2C address for 128x64 OLED

// Pin Definitions
// I2C pins (shared by OLED and MPU9250): SDA=21, SCL=22
#define SD_CS 5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18
#define IGNITION_OUTPUT_PIN 4
#define AC_SENSOR_PIN 34
#define DC_SENSOR_PIN 35
#define MODE_SELECT_PIN 26
#define QS_INPUT_PIN 27
#define QS_PRESSURE_PIN 36
#define CLUTCH_SENSOR_PIN 39  // Sensor kopling (HIGH = ditarik, LOW = dilepas)
// #define LC_BUTTON_PIN 15   // REMOVED - LC now uses speed-based activation
#define MAP_SELECT_BTN 25
#define MAP_UP_BTN 33
#define MAP_DOWN_BTN 32
#define FRONT_WHEEL_SENSOR_PIN 22  // Hall effect sensor roda depan
#define REAR_WHEEL_SENSOR_PIN 14   // Hall effect sensor roda belakang

// Constants
#define MAX_RPM 25000          // Max RPM limit (2-stroke can rev higher!)
#define MIN_RPM 0              // Min RPM (allow 0 for idle)
#define MAX_MAPS 20            // Max mappings (user flexible, balanced for memory)
#define MAX_RPM_POINTS 50      // Max RPM points per map (very flexible!)
#define DEFAULT_RPM_POINTS 8   // Default points for new maps

// ===== SIMULATOR MODE SETTINGS =====
#define ENABLE_SIMULATOR true  // Set false untuk production use
bool simulatorMode = false;    // Toggle via web UI
bool simulatorRunning = false; // Simulator engine running state

// Ignition Map Structure (flexible points, user-defined)
struct IgnitionMap {
  int pointCount;                        // Actual number of points used (2-50)
  int rpmPoints[MAX_RPM_POINTS];         // RPM values (user customizable)
  int advanceDegrees[MAX_RPM_POINTS];    // Advance degrees per RPM point
};

// Quick Shifter Map Structure (flexible points, user-defined)
struct QuickShifterMap {
  bool enabled;                      // Enable/disable QS
  int sensorThreshold;               // ADC threshold untuk trigger QS (0-4095)
  bool sensorInvert;                 // Invert sensor signal
  int minRPM;                        // Min RPM untuk QS aktif
  int maxRPM;                        // Max RPM untuk QS aktif
  int pointCount;                    // Actual number of points used (2-50)
  int rpmPoints[MAX_RPM_POINTS];     // RPM values (user customizable)
  int killTimeMS[MAX_RPM_POINTS];    // Kill time per RPM point
};

// Launch Control Settings
struct LaunchControlSettings {
  bool enabled;                    // Enable/disable LC
  int targetRPM;                   // Target LC RPM (e.g., 6000)
  int retardDegrees;               // Timing retard saat LC aktif (e.g., 10°)
  int cutPattern;                  // Cut pattern: 0=no cut, 1=cut 1/2, 2=cut 1/3, 3=cut 2/3
};

// Anti-Wheelie Settings
struct AntiWheelieSettings {
  bool enabled;                    // Enable/disable anti-wheelie
  float pitchThreshold;            // Degrees above baseline to trigger (e.g., 15.0°)
  int cutPattern;                  // Cut pattern: 0=no cut, 1=cut 1/2, 2=cut 1/3, 3=cut 2/3
  int retardDegrees;               // Timing retard when wheelie detected (e.g., 5°)
};

// Traction Control Settings
struct TractionControlSettings {
  bool enabled;                    // Enable/disable traction control
  int frontWheelHoles;             // Number of magnets/holes in front wheel sensor (e.g., 4)
  int rearWheelHoles;              // Number of magnets/holes in rear wheel sensor (e.g., 4)
  float slipThreshold;             // % slip allowed before intervention (e.g., 0.15 = 15%)
  int cutPattern;                  // Cut pattern: 0=no cut, 1=cut 1/2, 2=cut 1/3, 3=cut 2/3
  int retardDegrees;               // Timing retard when slip detected (e.g., 5°)

  // Front Tire Configuration
  int frontTireWidth;              // Lebar ban depan (mm) - e.g., 70, 80, 90
  int frontTireAspect;             // Aspect ratio ban depan (%) - e.g., 80, 90, 100
  int frontWheelDiameter;          // Diameter velg depan (inch) - e.g., 14, 17

  // Rear Tire Configuration
  int rearTireWidth;               // Lebar ban belakang (mm) - e.g., 80, 90, 100
  int rearTireAspect;              // Aspect ratio ban belakang (%) - e.g., 80, 90, 100
  int rearWheelDiameter;           // Diameter velg belakang (inch) - e.g., 14, 17
};

// Main Mapping Structure
struct Mapping {
  char name[32];
  bool isActive;

  // Mode Settings
  bool isACMode;
  int acTriggerThreshold;
  bool acInvertSignal;
  int dcPulsesPerRev;
  bool dcPullupEnabled;

  // Engine Type & Sensor Configuration
  int engineType;                      // 0 = 2-stroke, 1 = 4-stroke
  int pickupSensorOffset;              // Pickup sensor angle offset (degrees BTDC)
                                       // Common values: 0, 10, 15, 20, 30 degrees

  // Ignition Settings
  int dwellTimeUS;
  int revLimiterRPM;
  bool revLimiterEnabled;
  int revLimiterCutPattern;        // 0=hard cut, 1=cut 1/2, 2=cut 1/3, 3=cut 2/3

  // Ignition Map (separate)
  IgnitionMap ignitionMap;

  // Quick Shifter Map (separate, no gear-based)
  QuickShifterMap qsMap;

  // Launch Control
  LaunchControlSettings launchControl;

  // Anti-Wheelie
  AntiWheelieSettings antiWheelie;

  // Traction Control
  TractionControlSettings tractionControl;
};

// Helper function to initialize RPM points (0-20000, step 500)
// Helper: Initialize default ignition map with sensible curve (8 points)
void initializeDefaultIgnitionMap(IgnitionMap* map) {
  // Default: 8 RPM points (clean, easy to customize)
  map->pointCount = DEFAULT_RPM_POINTS;

  // Default RPM points: 0, 1000, 3000, 6000, 9000, 12000, 15000, 18000
  map->rpmPoints[0] = 0;       map->advanceDegrees[0] = 5;    // Idle
  map->rpmPoints[1] = 1000;    map->advanceDegrees[1] = 10;   // Low
  map->rpmPoints[2] = 3000;    map->advanceDegrees[2] = 20;   // Mid-low
  map->rpmPoints[3] = 6000;    map->advanceDegrees[3] = 30;   // Mid
  map->rpmPoints[4] = 9000;    map->advanceDegrees[4] = 35;   // Peak
  map->rpmPoints[5] = 12000;   map->advanceDegrees[5] = 32;   // High
  map->rpmPoints[6] = 15000;   map->advanceDegrees[6] = 28;   // Very high
  map->rpmPoints[7] = 18000;   map->advanceDegrees[7] = 25;   // Rev limit area
}

// Helper: Initialize default QS map with sensible curve (6 points)
void initializeDefaultQSMap(QuickShifterMap* map) {
  // Default: 6 RPM points (shorter kill time at higher RPM)
  map->pointCount = 6;

  // CRITICAL FIX: Initialize minRPM and maxRPM (was missing!)
  map->enabled = false;
  map->sensorThreshold = 2048;
  map->sensorInvert = false;
  map->minRPM = 3000;    // Default: QS active from 3000 RPM
  map->maxRPM = 15000;   // Default: QS active up to 15000 RPM

  // Default RPM points with kill times
  map->rpmPoints[0] = 0;       map->killTimeMS[0] = 150;   // Idle/low: 150ms
  map->rpmPoints[1] = 3000;    map->killTimeMS[1] = 130;   // Low: 130ms
  map->rpmPoints[2] = 6000;    map->killTimeMS[2] = 100;   // Mid: 100ms
  map->rpmPoints[3] = 9000;    map->killTimeMS[3] = 80;    // High: 80ms
  map->rpmPoints[4] = 12000;   map->killTimeMS[4] = 65;    // Very high: 65ms
  map->rpmPoints[5] = 15000;   map->killTimeMS[5] = 60;    // Peak: 60ms
}

// Default Mapping
Mapping defaultMapping;

void initializeDefaultMapping() {
  strcpy(defaultMapping.name, "Default");
  defaultMapping.isActive = true;
  defaultMapping.isACMode = true;
  defaultMapping.acTriggerThreshold = 2048;
  defaultMapping.acInvertSignal = false;
  defaultMapping.dcPulsesPerRev = 1;
  defaultMapping.dcPullupEnabled = true;
  defaultMapping.engineType = 0;           // Default: 2-stroke
  defaultMapping.pickupSensorOffset = 0;   // Default: 0 degrees (sensor at TDC)
  defaultMapping.dwellTimeUS = 3000;
  defaultMapping.revLimiterRPM = 12000;
  defaultMapping.revLimiterEnabled = true;
  defaultMapping.revLimiterCutPattern = 2;  // Cut 1 in 3 sparks (soft)

  // Initialize ignition map (8 points default)
  initializeDefaultIgnitionMap(&defaultMapping.ignitionMap);

  // Initialize QS map (6 points default)
  defaultMapping.qsMap.enabled = false;
  defaultMapping.qsMap.sensorThreshold = 2048;
  defaultMapping.qsMap.sensorInvert = false;
  defaultMapping.qsMap.minRPM = 3000;
  defaultMapping.qsMap.maxRPM = 15000;
  initializeDefaultQSMap(&defaultMapping.qsMap);

  // Initialize Launch Control
  defaultMapping.launchControl.enabled = false;
  defaultMapping.launchControl.targetRPM = 6000;
  defaultMapping.launchControl.retardDegrees = 10;
  defaultMapping.launchControl.cutPattern = 2;  // Cut 1 in 3 sparks

  // Initialize Anti-Wheelie
  defaultMapping.antiWheelie.enabled = false;
  defaultMapping.antiWheelie.pitchThreshold = 15.0;  // 15 degrees above baseline
  defaultMapping.antiWheelie.cutPattern = 2;  // Cut 1 in 3 sparks
  defaultMapping.antiWheelie.retardDegrees = 5;

  // Initialize Traction Control
  defaultMapping.tractionControl.enabled = false;
  defaultMapping.tractionControl.frontWheelHoles = 4;  // Default: 4 magnets
  defaultMapping.tractionControl.rearWheelHoles = 4;   // Default: 4 magnets
  defaultMapping.tractionControl.slipThreshold = 0.15;  // 15% slip tolerance
  defaultMapping.tractionControl.cutPattern = 2;  // Cut 1 in 3 sparks
  defaultMapping.tractionControl.retardDegrees = 5;
}

// Global Variables
Mapping mappings[MAX_MAPS];
int currentMapIndex = 0;
int totalMaps = 0;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);
bool isAPMode = false;
bool webServerActive = false;

// MPU9250 IMU Sensor
MPU9250 mpu;
float baselinePitch = 0.0;  // Baseline pitch when bike is level

// Mutex for SD card access (still needed for thread safety)
SemaphoreHandle_t sdMutex;

// Runtime Variables
volatile unsigned long lastTriggerTime = 0;
volatile unsigned long triggerInterval = 0;
volatile int currentRPM = 0;
volatile bool ignitionEnabled = true;
volatile int currentAdvance = 0;
volatile bool qsActive = false;
volatile unsigned long qsActivatedTime = 0;
int qsKillDuration = 0;
unsigned long totalIgnitions = 0;
unsigned long missedIgnitions = 0;

// ===== CRITICAL FIX #1 & #2: Pre-calculation variables =====
volatile int precalculatedAdvance = 10;  // Pre-calculated advance (updated in loop)
volatile unsigned long usPerDegree = 277;  // Pre-calculated μs/degree @ current RPM (277 = 6000 RPM default)
unsigned long lastAdvanceCalc = 0;       // Timestamp of last advance calculation
volatile int qsPressureValue = 0;        // Pre-read QS sensor value (no analogRead in ISR!)
unsigned long lastQSRead = 0;            // Timestamp of last QS sensor read

// ===== CRITICAL FIX #3: Timing compensation =====
#define BASE_ISR_OVERHEAD_US 12  // Base ISR overhead without features
#define FEATURE_OVERHEAD_US 2    // Additional overhead per active feature

// ===== CRITICAL FIX #5: Safety constants =====
#define MIN_TRIGGER_INTERVAL 2400     // Max 25,000 RPM
#define MAX_TRIGGER_INTERVAL 600000   // Min 100 RPM
#define MIN_SAFE_VOLTAGE 10.5         // Minimum battery voltage
#define VOLTAGE_WARNING 11.0          // Low voltage warning threshold

// ===== CRITICAL FIX #4: Thread safety =====
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;  // Mutex for RPM calculation
float batteryVoltage = 12.0;
bool lowVoltageMode = false;

// Launch Control Runtime
volatile bool lcActive = false;
volatile int sparkCounter = 0;  // For cut pattern logic

// Wheel Speed Sensor Variables
volatile unsigned long frontWheelPulseCount = 0;
volatile unsigned long rearWheelPulseCount = 0;
volatile unsigned long lastFrontPulseTime = 0;
volatile unsigned long lastRearPulseTime = 0;
float frontWheelSpeed = 0.0;  // km/h
float rearWheelSpeed = 0.0;   // km/h

// Anti-Wheelie Runtime
volatile bool antiWheelieActive = false;
float currentPitch = 0.0;

// Traction Control Runtime
volatile bool tractionControlActive = false;
float currentSlipRatio = 0.0;

// ===== EDGE CASE PROTECTION: Safety Monitoring =====
// Sensor health monitoring
int lastRPMValue = 0;
int rpmStuckCounter = 0;
#define RPM_STUCK_THRESHOLD 50  // 50 readings (5 seconds) sama = sensor stuck
unsigned long lastRPMChangeTime = 0;

// Coil protection (overheat prevention)
unsigned long totalIgnitionsSinceReset = 0;
unsigned long lastCoilResetTime = 0;
#define MAX_IGNITIONS_PER_SECOND 350  // Max ~350 sparks/sec (21000 RPM)
#define COIL_CHECK_INTERVAL 1000      // Check every 1 second
bool coilOverheatProtection = false;

// RPM sanity checks (glitch detection)
int previousRPM = 0;
#define MAX_RPM_JUMP 5000  // Max 5000 RPM change in 100ms = likely glitch
unsigned long rpmGlitchCounter = 0;

// Consecutive misfire detection
volatile unsigned long consecutiveMisfires = 0;
#define MAX_CONSECUTIVE_MISFIRES 100  // 100 misses in a row = problem
bool emergencySafeMode = false;

// Voltage monitoring
float minVoltageRecorded = 14.0;
unsigned long lowVoltageCounter = 0;
#define LOW_VOLTAGE_THRESHOLD 10.5    // Below 10.5V = critical

// Emergency shutdown
bool emergencyShutdown = false;
unsigned long shutdownReason = 0;  // Bitmask for multiple reasons
#define SHUTDOWN_OVERHEAT     0x01
#define SHUTDOWN_LOW_VOLTAGE  0x02
#define SHUTDOWN_SENSOR_FAIL  0x04
#define SHUTDOWN_MISFIRE      0x08
#define SHUTDOWN_USER         0x10

// ===== HIGH-PERFORMANCE LOGGING SYSTEM =====
// Ring buffer logging - non-blocking, ISR-safe
#define LOG_BUFFER_SIZE 100  // Keep last 100 log entries in RAM
#define LOG_FILE_PATH "/logs/cdi.log"
#define MAX_LOG_FILE_SIZE (1024 * 100)  // 100KB max log file

enum LogLevel {
  LOG_DEBUG = 0,
  LOG_INFO = 1,
  LOG_WARNING = 2,
  LOG_ERROR = 3,
  LOG_CRITICAL = 4
};

struct LogEntry {
  unsigned long timestamp;  // millis()
  LogLevel level;
  char message[80];        // Max 80 chars per log
  int rpm;                 // Current RPM when logged
  float voltage;           // Battery voltage when logged
};

// Ring buffer for logs (in RAM, fast)
LogEntry logBuffer[LOG_BUFFER_SIZE];
volatile int logWriteIndex = 0;
volatile int logReadIndex = 0;
volatile int logCount = 0;
unsigned long lastLogFlush = 0;
#define LOG_FLUSH_INTERVAL 5000  // Flush to SD every 5 seconds

// Log statistics
unsigned long totalLogsWritten = 0;
unsigned long logsDropped = 0;  // Dropped when buffer full

// ===== SIMULATOR MODE VARIABLES =====
struct SimulatorState {
  // Virtual Inputs
  int virtualRPM = 0;              // Simulated RPM (0-25000)
  int virtualThrottle = 0;         // Throttle position (0-100%)
  bool virtualClutch = false;      // Clutch pulled?
  bool virtualQSPressed = false;   // QS activated?
  int virtualQSPressure = 0;       // QS sensor value (0-4095)
  float virtualSpeed = 0.0;        // Speed (km/h)
  float virtualPitch = 0.0;        // Pitch angle (degrees)
  float virtualSlip = 0.0;         // Wheel slip ratio (0-1.0)

  // Virtual Outputs
  bool virtualIgnitionFiring = false;
  int virtualAdvanceDegrees = 0;
  unsigned long virtualIgnitionCount = 0;
  unsigned long virtualMissedCount = 0;

  // Test Scenarios
  String activeScenario = "idle";  // idle, accel, revlimit, quickshift, launch, wheelie, traction
  unsigned long scenarioStartTime = 0;

  // Auto-run settings
  bool autoRun = false;            // Auto cycle through RPM
  int autoTargetRPM = 8000;
  int autoRPMStep = 100;           // RPM increment per update
};

SimulatorState sim;

// QS Calibration Variables
bool qsCalibrating = false;
int qsCalibrateStep = 0;  // 0=idle, 1=press, 2=release
int qsCalLowValue = 4095;
int qsCalHighValue = 0;
int qsCalPressedValue = 0;
int qsCalReleasedValue = 0;

// Embedded HTML for File Manager (minimal version)
const char FILE_MANAGER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart CDI File Manager</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 20px;
            min-height: 100vh;
        }
        .container {
            max-width: 900px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            padding: 30px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
        }
        h1 { color: #2c3e50; margin-bottom: 10px; }
        .subtitle { color: #7f8c8d; margin-bottom: 30px; }
        .section {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 8px;
            margin-bottom: 20px;
        }
        .section h2 { color: #34495e; margin-bottom: 15px; font-size: 1.2em; }
        .upload-area {
            border: 2px dashed #667eea;
            border-radius: 8px;
            padding: 30px;
            text-align: center;
            background: white;
            cursor: pointer;
            transition: all 0.3s;
        }
        .upload-area:hover { background: #f0f4ff; border-color: #764ba2; }
        .upload-area.dragover { background: #e8f0fe; border-color: #667eea; }
        input[type="file"] { display: none; }
        .btn {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 12px 30px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 1em;
            font-weight: 600;
            transition: transform 0.2s;
        }
        .btn:hover { transform: translateY(-2px); }
        .btn:disabled { opacity: 0.5; cursor: not-allowed; }
        .file-list {
            list-style: none;
            margin-top: 20px;
        }
        .file-item {
            background: white;
            padding: 15px;
            margin-bottom: 10px;
            border-radius: 5px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .file-name { font-weight: 600; color: #2c3e50; }
        .file-size { color: #7f8c8d; font-size: 0.9em; }
        .btn-delete {
            background: #e74c3c;
            color: white;
            border: none;
            padding: 8px 15px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 0.9em;
        }
        .btn-delete:hover { background: #c0392b; }
        .progress {
            width: 100%;
            height: 30px;
            background: #ecf0f1;
            border-radius: 15px;
            overflow: hidden;
            margin: 20px 0;
            display: none;
        }
        .progress-bar {
            height: 100%;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            width: 0%;
            transition: width 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
        }
        .status { padding: 15px; border-radius: 5px; margin: 15px 0; }
        .status.success { background: #d4edda; color: #155724; }
        .status.error { background: #f8d7da; color: #721c24; }
        .info-box {
            background: #e8f4f8;
            border-left: 4px solid #3498db;
            padding: 15px;
            margin: 20px 0;
            border-radius: 5px;
        }
        .links { margin-top: 30px; text-align: center; }
        .links a {
            display: inline-block;
            margin: 0 10px;
            color: #667eea;
            text-decoration: none;
            font-weight: 600;
        }
        .links a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🏍️ Smart CDI File Manager</h1>
        <p class="subtitle">Upload configuration files and web interface</p>

        <div class="section">
            <h2>📤 Upload Files to SD Card</h2>
            <div class="upload-area" id="uploadArea" onclick="document.getElementById('fileInput').click()">
                <p style="font-size: 3em; margin-bottom: 10px;">📁</p>
                <p style="font-size: 1.2em; font-weight: 600; color: #2c3e50;">Click or drag files here</p>
                <p style="color: #7f8c8d; margin-top: 10px;">Upload mapping files (*.json) or web files (*.html, *.css, *.js)</p>
            </div>
            <input type="file" id="fileInput" multiple accept=".json,.html,.css,.js">

            <div class="progress" id="progress">
                <div class="progress-bar" id="progressBar">0%</div>
            </div>

            <div id="status"></div>
        </div>

        <div class="section">
            <h2>📋 Files on SD Card</h2>
            <button class="btn" onclick="listFiles()">🔄 Refresh File List</button>
            <ul class="file-list" id="fileList">
                <li style="text-align: center; color: #7f8c8d;">Click refresh to load files</li>
            </ul>
        </div>

        <div class="info-box">
            <h3>ℹ️ Smart CDI Directory Structure:</h3>
            <pre style="margin-top: 10px; color: #2c3e50;">
/config/
  └── wifi.json      (WiFi settings)

/maps/
  ├── map_0.json     (Ignition maps)
  ├── map_1.json
  └── map_N.json     (Max 20 maps)

/www/
  ├── index.html     (Main UI)
  ├── wifi.html      (WiFi settings UI)
  ├── simulator.html (Simulator UI)
  ├── style.css
  └── app.js

Note: Files uploaded will replace existing files with same name
            </pre>
        </div>

        <div class="links">
            <a href="/dashboard">🏠 Dashboard</a>
            <a href="/upload">📤 File Manager</a>
            <a href="/api/status">📊 API Status</a>
        </div>
    </div>

    <script>
        const uploadArea = document.getElementById('uploadArea');
        const fileInput = document.getElementById('fileInput');
        const progress = document.getElementById('progress');
        const progressBar = document.getElementById('progressBar');
        const status = document.getElementById('status');

        // Drag and drop
        uploadArea.addEventListener('dragover', (e) => {
            e.preventDefault();
            uploadArea.classList.add('dragover');
        });

        uploadArea.addEventListener('dragleave', () => {
            uploadArea.classList.remove('dragover');
        });

        uploadArea.addEventListener('drop', (e) => {
            e.preventDefault();
            uploadArea.classList.remove('dragover');
            const files = e.dataTransfer.files;
            uploadFiles(files);
        });

        fileInput.addEventListener('change', (e) => {
            uploadFiles(e.target.files);
        });

        async function uploadFiles(files) {
            if (files.length === 0) return;

            progress.style.display = 'block';
            status.innerHTML = '';

            for (let i = 0; i < files.length; i++) {
                const file = files[i];
                const percent = ((i + 1) / files.length * 100).toFixed(0);
                progressBar.style.width = percent + '%';
                progressBar.textContent = percent + '%';

                try {
                    await uploadFile(file);
                    showStatus('✓ Uploaded: ' + file.name, 'success');
                } catch (error) {
                    showStatus('✗ Failed: ' + file.name + ' - ' + error, 'error');
                }
            }

            progress.style.display = 'none';
            fileInput.value = '';
            listFiles();
        }

        async function uploadFile(file) {
            const formData = new FormData();
            formData.append('file', file);

            // Determine path based on file extension
            let path = '/';
            if (file.name.endsWith('.json')) {
                path = '/maps/';
            } else if (file.name.endsWith('.html') || file.name.endsWith('.css') || file.name.endsWith('.js')) {
                path = '/www/';
            }

            const response = await fetch('/upload?path=' + encodeURIComponent(path + file.name), {
                method: 'POST',
                body: formData
            });

            if (!response.ok) {
                throw new Error('Upload failed: ' + response.statusText);
            }

            return response.json();
        }

        function showStatus(message, type) {
            const div = document.createElement('div');
            div.className = 'status ' + type;
            div.textContent = message;
            status.appendChild(div);
        }

        async function listFiles() {
            const fileList = document.getElementById('fileList');
            fileList.innerHTML = '<li style="text-align: center; color: #7f8c8d;">Loading...</li>';

            try {
                const response = await fetch('/api/files');
                const data = await response.json();

                if (data.files && data.files.length > 0) {
                    fileList.innerHTML = '';
                    data.files.forEach(file => {
                        const li = document.createElement('li');
                        li.className = 'file-item';
                        li.innerHTML = `
                            <div>
                                <div class="file-name">${file.name}</div>
                                <div class="file-size">${formatBytes(file.size)}</div>
                            </div>
                            <div style="display: flex; gap: 10px;">
                                <button class="btn" style="padding: 8px 15px; font-size: 0.9em; background: #3498db;" onclick="downloadFile('${file.path}', '${file.name}')">⬇️ Download</button>
                                <button class="btn-delete" onclick="deleteFile('${file.path}')">🗑️ Delete</button>
                            </div>
                        `;
                        fileList.appendChild(li);
                    });
                } else {
                    fileList.innerHTML = '<li style="text-align: center; color: #7f8c8d;">No files found</li>';
                }
            } catch (error) {
                fileList.innerHTML = '<li style="text-align: center; color: #e74c3c;">Error loading files</li>';
            }
        }

        async function deleteFile(path) {
            if (!confirm('Delete ' + path + '?')) return;

            try {
                const response = await fetch('/delete?path=' + encodeURIComponent(path), {
                    method: 'DELETE'
                });

                if (response.ok) {
                    showStatus('✓ Deleted: ' + path, 'success');
                    listFiles();
                } else {
                    showStatus('✗ Failed to delete: ' + path, 'error');
                }
            } catch (error) {
                showStatus('✗ Error: ' + error, 'error');
            }
        }

        function downloadFile(path, filename) {
            const link = document.createElement('a');
            link.href = '/download?path=' + encodeURIComponent(path);
            link.download = filename;
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            showStatus('⬇️ Downloading: ' + filename, 'success');
        }

        function formatBytes(bytes) {
            if (bytes === 0) return '0 Bytes';
            const k = 1024;
            const sizes = ['Bytes', 'KB', 'MB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
        }
    </script>
</body>
</html>
)rawliteral";

// Function Prototypes
bool setupWiFi();  // Returns true if WiFi/AP active
void ensureProtectedNetwork();  // Ensure protected WiFi network exists
void setupWebServer();
void setupRoutes();
void loadAllMappings();
void loadMapping(int index);
void saveMapping(int index);
void deleteMapping(int index);
int calculateAdvance(int rpm);
void fireIgnition(int advanceDegrees);
void handleMapButtons();
void updateDisplay();
String getAllMappingsJSON();
String getMappingJSON(int index);
String listFilesJSON();
void IRAM_ATTR acSensorISR();
void IRAM_ATTR dcSensorISR();
void IRAM_ATTR qsSensorISR();
void IRAM_ATTR frontWheelSensorISR();
void IRAM_ATTR rearWheelSensorISR();
void updateIMUData();
void updateWheelSpeeds();
void checkAntiWheelieAndTC();

// Simulator Functions
void updateSimulator();
void runSimulatorScenario(String scenario);
void simulatorApplyInputs();
String getSimulatorStateJSON();
void handleSimulatorAPI();

// Logging Functions
void logMessage(LogLevel level, const char* message);
void flushLogsToSD();
String getRecentLogsJSON(int count);
void initLogging();

// ===== LOGGING SYSTEM IMPLEMENTATION =====
// Fast, non-blocking log function (ISR-safe)
void logMessage(LogLevel level, const char* message) {
  // Check if buffer is full
  if (logCount >= LOG_BUFFER_SIZE) {
    logsDropped++;
    return;  // Drop log if buffer full (non-blocking)
  }

  // Write to ring buffer
  LogEntry* entry = &logBuffer[logWriteIndex];
  entry->timestamp = millis();
  entry->level = level;
  strncpy(entry->message, message, sizeof(entry->message) - 1);
  entry->message[sizeof(entry->message) - 1] = '\0';
  entry->rpm = currentRPM;
  entry->voltage = batteryVoltage;

  // Update ring buffer indices
  logWriteIndex = (logWriteIndex + 1) % LOG_BUFFER_SIZE;
  logCount++;
  totalLogsWritten++;
}

// Flush logs from RAM buffer to SD card (called in main loop, non-critical)
void flushLogsToSD() {
  if (logCount == 0) return;  // Nothing to flush

  // Open log file (append mode)
  File logFile = SD.open(LOG_FILE_PATH, FILE_APPEND);
  if (!logFile) {
    // Try to create directory
    SD.mkdir("/logs");
    logFile = SD.open(LOG_FILE_PATH, FILE_WRITE);
    if (!logFile) {
      Serial.println("⚠️ Failed to open log file");
      return;
    }
  }

  // Write all pending logs
  int flushedCount = 0;
  while (logCount > 0 && flushedCount < 20) {  // Max 20 logs per flush (non-blocking)
    LogEntry* entry = &logBuffer[logReadIndex];

    // Format: [timestamp] [LEVEL] [RPM] [V] message
    const char* levelStr[] = {"DEBUG", "INFO", "WARN", "ERROR", "CRIT"};
    logFile.printf("[%lu] [%s] [%d RPM] [%.1fV] %s\n",
                   entry->timestamp,
                   levelStr[entry->level],
                   entry->rpm,
                   entry->voltage,
                   entry->message);

    // Update ring buffer indices
    logReadIndex = (logReadIndex + 1) % LOG_BUFFER_SIZE;
    logCount--;
    flushedCount++;
  }

  logFile.close();

  // Check log file size, rotate if needed
  File checkSize = SD.open(LOG_FILE_PATH, FILE_READ);
  if (checkSize && checkSize.size() > MAX_LOG_FILE_SIZE) {
    checkSize.close();
    // Rotate log: rename old to .old, start new
    SD.remove("/logs/cdi.log.old");
    SD.rename(LOG_FILE_PATH, "/logs/cdi.log.old");
    Serial.println("📋 Log file rotated");
  } else if (checkSize) {
    checkSize.close();
  }
}

// Get recent logs as JSON (for dashboard)
String getRecentLogsJSON(int count) {
  if (count > LOG_BUFFER_SIZE) count = LOG_BUFFER_SIZE;
  if (count > logCount) count = logCount;

  StaticJsonDocument<4096> doc;
  JsonArray logs = doc.createNestedArray("logs");

  // Read from ring buffer (most recent first)
  int startIdx = (logWriteIndex - count + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
  for (int i = 0; i < count; i++) {
    int idx = (startIdx + i) % LOG_BUFFER_SIZE;
    LogEntry* entry = &logBuffer[idx];

    JsonObject log = logs.createNestedObject();
    log["timestamp"] = entry->timestamp;
    log["level"] = entry->level;
    log["message"] = entry->message;
    log["rpm"] = entry->rpm;
    log["voltage"] = entry->voltage;
  }

  doc["totalLogs"] = totalLogsWritten;
  doc["logsDropped"] = logsDropped;
  doc["bufferUsed"] = logCount;

  String output;
  serializeJson(doc, output);
  return output;
}

// Initialize logging system
void initLogging() {
  logWriteIndex = 0;
  logReadIndex = 0;
  logCount = 0;
  totalLogsWritten = 0;
  logsDropped = 0;

  // Ensure logs directory exists
  if (!SD.exists("/logs")) {
    SD.mkdir("/logs");
  }

  logMessage(LOG_INFO, "CDI Logging System Initialized");
  Serial.println("📋 Logging system ready");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nSmart CDI Programmable v2.2 SIMULATOR");
  Serial.println("======================================");
  Serial.println("🏍️  OPTIMIZED FOR 2-STROKE ENGINES");
  Serial.println("🎮  SIMULATOR MODE AVAILABLE");
  Serial.println("");
  Serial.println("CRITICAL FIXES APPLIED:");
  Serial.println("- ISR optimization");
  Serial.println("- Timing compensation");
  Serial.println("- Watchdog protection");
  Serial.println("- Safety bounds checking");
  Serial.println("======================================");

  // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  // ⚡ FIRMWARE SIGNATURE VERIFICATION ⚡
  // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  Serial.println("");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.print("⚡ Author: ");
  Serial.println(FPSTR(FIRMWARE_AUTHOR));
  Serial.print("⚡ Version: ");
  Serial.println(FPSTR(FIRMWARE_VERSION));
  Serial.println(FPSTR(FIRMWARE_COPYRIGHT));
  Serial.print("⚡ Signature Hash: 0x");
  Serial.println(FIRMWARE_SIGNATURE_HASH, HEX);
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("");

  // ===== CRITICAL FIX #4: Enable Watchdog Timer =====
  // ESP32 v3.x (ESP-IDF 5.x) uses new config struct
  // Note: Watchdog may already be initialized by ESP32 core
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 15000,       // 15 second timeout (allow WiFi connect)
    .idle_core_mask = 0,       // Don't watch idle tasks
    .trigger_panic = true      // Panic on timeout
  };
  esp_err_t wdt_err = esp_task_wdt_init(&wdt_config);
  if (wdt_err == ESP_OK) {
    Serial.println("Watchdog initialized: 15 sec timeout");
  } else {
    Serial.println("Watchdog already initialized (using existing)");
  }
  esp_task_wdt_add(NULL);      // Add current task to WDT
  Serial.println("Current task added to watchdog");

  // Initialize default mapping
  initializeDefaultMapping();

  // Create SD mutex
  sdMutex = xSemaphoreCreateMutex();

  // Initialize I2C for OLED and MPU9250
  Wire.begin();
  delay(100);

  // Initialize OLED Display (I2C)
  Serial.println("Init OLED Display...");
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("OLED allocation failed!"));
    // Continue without display (not critical)
  } else {
    Serial.println("OLED OK");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Smart CDI v2.2"));
    display.println(F(""));
    display.setTextSize(2);
    display.println(F("wicaksu"));
    display.setTextSize(1);
    display.println(F(""));
    display.println(FPSTR(FIRMWARE_COPYRIGHT));
    display.display();
    delay(2000);
  }

  // Initialize SD Card
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Init SD Card..."));
  display.display();

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card FAILED!");
    display.println(F("SD FAILED!"));
    display.println(F("Using defaults"));
    display.display();
    delay(2000);
    mappings[0] = defaultMapping;
    totalMaps = 1;
  } else {
    Serial.println("SD Card OK");
    display.println(F("SD Card OK"));
    display.display();
    delay(500);

    // Initialize logging system
    initLogging();

    // Create directories if not exist
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
      if (!SD.exists("/maps")) {
        SD.mkdir("/maps");
        Serial.println("Created /maps directory");
      }
      if (!SD.exists("/www")) {
        SD.mkdir("/www");
        Serial.println("Created /www directory");
      }
      xSemaphoreGive(sdMutex);
    }

    loadAllMappings();
  }

  // Setup WiFi
  display.println(F(""));
  display.println(F("Starting WiFi..."));
  display.display();
  bool wifiActive = setupWiFi();

  // Wait for WiFi to be fully ready
  delay(1000);

  // Setup Web Server only if WiFi/AP is active
  if (wifiActive) {
    Serial.println("Starting web server...");
    setupWebServer();
    server.begin();
    webServerActive = true;
    Serial.println("Web Server started on port 80");
    Serial.println("================================");
  } else {
    webServerActive = false;
    Serial.println("Web Server DISABLED - No WiFi/AP available");
    Serial.println("Device will run in standalone mode");
    Serial.println("================================");
  }

  // Configure Pins
  pinMode(IGNITION_OUTPUT_PIN, OUTPUT);
  digitalWrite(IGNITION_OUTPUT_PIN, LOW);
  pinMode(MODE_SELECT_PIN, INPUT_PULLUP);
  pinMode(MAP_SELECT_BTN, INPUT_PULLUP);
  pinMode(MAP_UP_BTN, INPUT_PULLUP);
  pinMode(MAP_DOWN_BTN, INPUT_PULLUP);
  pinMode(QS_INPUT_PIN, INPUT_PULLUP);
  pinMode(CLUTCH_SENSOR_PIN, INPUT_PULLUP);  // Clutch sensor for LC
  // pinMode(LC_BUTTON_PIN, INPUT_PULLUP);   // REMOVED - LC now speed-based
  pinMode(FRONT_WHEEL_SENSOR_PIN, INPUT_PULLUP);  // Front wheel hall effect sensor
  pinMode(REAR_WHEEL_SENSOR_PIN, INPUT_PULLUP);   // Rear wheel hall effect sensor

  // Setup sensor interrupts
  if (mappings[currentMapIndex].isACMode) {
    pinMode(AC_SENSOR_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(AC_SENSOR_PIN), acSensorISR,
                   mappings[currentMapIndex].acInvertSignal ? FALLING : RISING);
  } else {
    pinMode(DC_SENSOR_PIN, mappings[currentMapIndex].dcPullupEnabled ? INPUT_PULLUP : INPUT);
    attachInterrupt(digitalPinToInterrupt(DC_SENSOR_PIN), dcSensorISR, RISING);
  }

  // Quick shifter interrupt
  attachInterrupt(digitalPinToInterrupt(QS_INPUT_PIN), qsSensorISR, FALLING);

  // Wheel speed sensor interrupts
  attachInterrupt(digitalPinToInterrupt(FRONT_WHEEL_SENSOR_PIN), frontWheelSensorISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(REAR_WHEEL_SENSOR_PIN), rearWheelSensorISR, FALLING);

  // Initialize MPU9250 IMU (shares I2C with OLED)
  Serial.println("Initializing MPU9250...");
  display.println(F("Init IMU..."));
  display.display();

  if (mpu.setup(0x68)) {  // 0x68 is default I2C address
    Serial.println("MPU9250 initialized!");
    display.println(F("IMU OK"));
    display.display();

    // Calibrate baseline pitch (bike should be level)
    delay(500);
    mpu.update();
    baselinePitch = mpu.getPitch();
    Serial.printf("Baseline pitch: %.2f degrees\n", baselinePitch);
  } else {
    Serial.println("MPU9250 initialization failed!");
    display.println(F("IMU FAILED"));
    display.display();
    // Continue without IMU (anti-wheelie will be disabled)
  }
  delay(500);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(F("READY!"));
  display.setTextSize(1);
  display.display();

  Serial.println("Smart CDI Ready!");
  Serial.println("================================");
}

void loop() {
  // ===== CRITICAL FIX #4: Reset watchdog timer =====
  esp_task_wdt_reset();

  static unsigned long lastDisplayUpdate = 0;
  static unsigned long lastRPMCheck = 0;
  static unsigned long lastIMUUpdate = 0;
  static unsigned long lastWheelSpeedUpdate = 0;
  unsigned long currentTime = millis();

  // Handle web server requests (only if active)
  if (webServerActive) {
    server.handleClient();
  }

  // Check RPM timeout (skip in simulator mode)
  if (!simulatorMode && currentTime - lastTriggerTime > 500) {
    currentRPM = 0;
  }

  // ===== CRITICAL FIX #5A: Safe RPM calculation with bounds checking =====
  // Skip RPM calculation in simulator mode (uses virtual RPM instead)
  if (!simulatorMode && currentTime - lastRPMCheck > 100) {
    // ===== CRITICAL FIX: Thread-safe read of triggerInterval (written by ISR) =====
    unsigned long localInterval;
    portENTER_CRITICAL(&timerMux);
    localInterval = triggerInterval;
    portEXIT_CRITICAL(&timerMux);

    if (localInterval > 0) {
      // Safety: prevent overflow/underflow
      unsigned long safeInterval = localInterval;

      if (safeInterval < MIN_TRIGGER_INTERVAL) {
        safeInterval = MIN_TRIGGER_INTERVAL;  // Cap at 25k RPM
      }

      if (safeInterval > MAX_TRIGGER_INTERVAL) {
        currentRPM = 0;  // Below 100 RPM, consider stopped
        usPerDegree = 0;  // Invalid - prevent division by zero in ISR
      } else {
        if (mappings[currentMapIndex].isACMode) {
          currentRPM = 60000000UL / safeInterval;
        } else {
          int pulsesPerRev = mappings[currentMapIndex].dcPulsesPerRev;
          // Safety bounds check
          if (pulsesPerRev < 1) pulsesPerRev = 1;
          if (pulsesPerRev > 10) pulsesPerRev = 10;
          currentRPM = 60000000UL / (safeInterval * pulsesPerRev);
        }

        // Final bounds check
        if (currentRPM > MAX_RPM) currentRPM = MAX_RPM;
        if (currentRPM < MIN_RPM) currentRPM = 0;

        // ===== EDGE PROTECTION: RPM Sanity Check (Glitch Detection) =====
        int rpmDelta = abs(currentRPM - previousRPM);
        if (rpmDelta > MAX_RPM_JUMP && previousRPM > 0) {
          // Sudden RPM spike detected - likely sensor glitch or EMI
          rpmGlitchCounter++;
          if (rpmGlitchCounter > 5) {
            // Too many glitches - possible sensor failure
            Serial.printf("⚠️ SENSOR GLITCH: RPM jumped %d → %d (%+d)\n",
                         previousRPM, currentRPM, currentRPM - previousRPM);

            // Use previous RPM value (smooth out glitch)
            currentRPM = previousRPM;

            if (rpmGlitchCounter > 20) {
              // Persistent glitches - shutdown for safety
              emergencyShutdown = true;
              shutdownReason |= SHUTDOWN_SENSOR_FAIL;
              Serial.println("🔴 EMERGENCY: Sensor failure detected - SHUTDOWN");
            }
          }
        } else {
          // RPM change is reasonable, reset glitch counter
          if (rpmGlitchCounter > 0) rpmGlitchCounter--;
        }

        // ===== EDGE PROTECTION: Sensor Stuck Detection =====
        if (currentRPM == lastRPMValue && currentRPM > 0) {
          rpmStuckCounter++;
          if (rpmStuckCounter >= RPM_STUCK_THRESHOLD) {
            // Sensor reading hasn't changed for 5 seconds - stuck?
            Serial.printf("⚠️ SENSOR STUCK: RPM frozen at %d for %d readings\n",
                         currentRPM, rpmStuckCounter);
            // Don't shutdown yet, but warn user
          }
        } else {
          rpmStuckCounter = 0;
          lastRPMChangeTime = currentTime;
        }

        lastRPMValue = currentRPM;
        previousRPM = currentRPM;

        // ===== CRITICAL FIX: Pre-calculate μs/degree for ISR =====
        // Removes expensive division from ISR!
        if (currentRPM > 0) {
          usPerDegree = (60000000UL / currentRPM) / 360;
        } else {
          usPerDegree = 0;
        }
      }
    } else {
      currentRPM = 0;  // No trigger interval = engine stopped
      usPerDegree = 0;
    }
    lastRPMCheck = currentTime;
  }

  // ===== CRITICAL FIX #1: Pre-calculate advance (not in ISR!) =====
  if (currentTime - lastAdvanceCalc >= 20) {  // Update every 20ms (50Hz)
    precalculatedAdvance = calculateAdvance(currentRPM);
    currentAdvance = precalculatedAdvance;  // Update display variable for API/dashboard
    lastAdvanceCalc = currentTime;
  }

  // ===== CRITICAL FIX #2: Pre-read QS sensor (not in ISR!) =====
  // Skip in simulator mode (uses virtual QS pressure instead)
  if (!simulatorMode && currentTime - lastQSRead >= 10) {  // Read every 10ms (100Hz)
    qsPressureValue = analogRead(QS_PRESSURE_PIN);
    lastQSRead = currentTime;
  }

  // Update IMU data (every 50ms) - skip in simulator mode
  if (!simulatorMode && currentTime - lastIMUUpdate > 50) {
    updateIMUData();
    lastIMUUpdate = currentTime;
  }

  // ===== EDGE PROTECTION: Voltage Monitoring (every second) =====
  static unsigned long lastVoltageCheck = 0;
  if (currentTime - lastVoltageCheck >= 1000) {
    // Read battery voltage (assuming voltage divider on ADC pin)
    // Note: If BATTERY_VOLTAGE_PIN not defined, uses default 12V
    #ifdef BATTERY_VOLTAGE_PIN
      int adcValue = analogRead(BATTERY_VOLTAGE_PIN);
      batteryVoltage = (adcValue / 4095.0) * 3.3 * 4.545;  // Adjust for divider
    #else
      batteryVoltage = 12.0;  // Default if no monitoring
    #endif

    // Track minimum voltage
    if (batteryVoltage < minVoltageRecorded) {
      minVoltageRecorded = batteryVoltage;
    }

    // Low voltage warning & protection
    if (batteryVoltage < LOW_VOLTAGE_THRESHOLD) {
      lowVoltageCounter++;
      if (lowVoltageCounter > 5) {
        // Persistent low voltage - emergency shutdown
        emergencyShutdown = true;
        shutdownReason |= SHUTDOWN_LOW_VOLTAGE;
        Serial.printf("🔴 EMERGENCY: Low voltage %.1fV - SHUTDOWN\n", batteryVoltage);
      } else if (lowVoltageCounter == 1) {
        Serial.printf("⚠️ LOW VOLTAGE: %.1fV (threshold %.1fV)\n",
                     batteryVoltage, LOW_VOLTAGE_THRESHOLD);
      }
    } else {
      // Voltage OK, reset counter
      if (lowVoltageCounter > 0) lowVoltageCounter--;
    }

    lastVoltageCheck = currentTime;
  }

  // Update wheel speeds (every 100ms) - skip in simulator mode
  if (!simulatorMode && currentTime - lastWheelSpeedUpdate > 100) {
    updateWheelSpeeds();
    lastWheelSpeedUpdate = currentTime;
  }

  // ===== EDGE PROTECTION: Coil Overheat Prevention =====
  // Check ignition frequency every second to prevent coil damage
  if (currentTime - lastCoilResetTime >= COIL_CHECK_INTERVAL) {
    unsigned long currentIgnitions = totalIgnitions;
    unsigned long ignitionsThisSecond = currentIgnitions - totalIgnitionsSinceReset;

    if (ignitionsThisSecond > MAX_IGNITIONS_PER_SECOND) {
      // Too many sparks - coil overheating risk!
      coilOverheatProtection = true;
      Serial.printf("🔴 COIL PROTECTION: %lu ign/sec (max %d) - REDUCING SPARKS\n",
                   ignitionsThisSecond, MAX_IGNITIONS_PER_SECOND);

      // Emergency: temporarily disable ignition to let coil cool
      if (ignitionsThisSecond > MAX_IGNITIONS_PER_SECOND * 1.2) {
        emergencyShutdown = true;
        shutdownReason |= SHUTDOWN_OVERHEAT;
        Serial.println("🔴 EMERGENCY: Coil overheat - SHUTDOWN");
      }
    } else if (coilOverheatProtection) {
      // Frequency back to normal, re-enable
      coilOverheatProtection = false;
      Serial.println("✅ COIL PROTECTION: Frequency normal - RESUMED");
    }

    totalIgnitionsSinceReset = currentIgnitions;
    lastCoilResetTime = currentTime;
  }

  // Check anti-wheelie and traction control
  checkAntiWheelieAndTC();

  // Handle quick shifter timeout
  if (qsActive && (currentTime - qsActivatedTime > qsKillDuration)) {
    qsActive = false;
    ignitionEnabled = true;
  }

  // Check Launch Control conditions
  // LC requires: LC enabled + low speed (<5 km/h) + clutch pulled + RPM >= target
  Mapping* currentMap = &mappings[currentMapIndex];
  if (currentMap->launchControl.enabled) {
    bool clutchPulled = (digitalRead(CLUTCH_SENSOR_PIN) == LOW);  // LOW = pulled (with pullup)
    bool lowSpeed = (frontWheelSpeed < 5.0);  // Speed less than 5 km/h

    // LC only activates if low speed AND clutch pulled AND RPM high enough
    if (lowSpeed && clutchPulled && currentRPM >= currentMap->launchControl.targetRPM) {
      lcActive = true;
    } else {
      lcActive = false;
    }
  } else {
    lcActive = false;
  }

  // ===== SIMULATOR MODE UPDATE =====
  if (simulatorMode && ENABLE_SIMULATOR) {
    updateSimulator();
  }

  // Handle map selection buttons
  handleMapButtons();

  // Update display
  if (currentTime - lastDisplayUpdate > 200) {
    updateDisplay();
    lastDisplayUpdate = currentTime;
  }

  // ===== Flush logs to SD card periodically =====
  if (currentTime - lastLogFlush >= LOG_FLUSH_INTERVAL) {
    flushLogsToSD();
    lastLogFlush = currentTime;
  }

  delay(10);
}

// ====== WiFi Settings Functions ======

void initializeDefaultWiFiSettings() {
  wifiSettings.networkCount = 1;
  strcpy(wifiSettings.networks[0].ssid, STA_SSID);
  strcpy(wifiSettings.networks[0].password, STA_PASS);
  wifiSettings.apModeEnabled = true;  // Enable AP fallback by default
  strcpy(wifiSettings.apSSID, AP_SSID);
  strcpy(wifiSettings.apPassword, AP_PASS);
}

bool loadWiFiSettings() {
  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    File file = SD.open("/config/wifi.json", FILE_READ);
    if (!file) {
      xSemaphoreGive(sdMutex);
      Serial.println("WiFi config not found, using defaults");
      initializeDefaultWiFiSettings();
      return false;
    }

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    xSemaphoreGive(sdMutex);

    if (error) {
      Serial.println("Failed to parse WiFi config");
      initializeDefaultWiFiSettings();
      return false;
    }

    // Load networks
    JsonArray networks = doc["networks"];
    wifiSettings.networkCount = 0;
    for (JsonObject network : networks) {
      if (wifiSettings.networkCount >= MAX_WIFI_NETWORKS) break;
      strcpy(wifiSettings.networks[wifiSettings.networkCount].ssid, network["ssid"] | "");
      strcpy(wifiSettings.networks[wifiSettings.networkCount].password, network["password"] | "");
      wifiSettings.networkCount++;
    }

    // Load AP settings
    wifiSettings.apModeEnabled = doc["apModeEnabled"] | true;
    strcpy(wifiSettings.apSSID, doc["apSSID"] | AP_SSID);
    strcpy(wifiSettings.apPassword, doc["apPassword"] | AP_PASS);

    Serial.println("WiFi settings loaded from SD card");
    return true;
  }

  initializeDefaultWiFiSettings();
  return false;
}

bool saveWiFiSettings() {
  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Create config directory if it doesn't exist
    if (!SD.exists("/config")) {
      SD.mkdir("/config");
    }

    File file = SD.open("/config/wifi.json", FILE_WRITE);
    if (!file) {
      xSemaphoreGive(sdMutex);
      Serial.println("Failed to create WiFi config file");
      return false;
    }

    StaticJsonDocument<2048> doc;

    // Save networks
    JsonArray networks = doc.createNestedArray("networks");
    for (int i = 0; i < wifiSettings.networkCount; i++) {
      JsonObject network = networks.createNestedObject();
      network["ssid"] = wifiSettings.networks[i].ssid;
      network["password"] = wifiSettings.networks[i].password;
    }

    // Save AP settings
    doc["apModeEnabled"] = wifiSettings.apModeEnabled;
    doc["apSSID"] = wifiSettings.apSSID;
    doc["apPassword"] = wifiSettings.apPassword;

    serializeJson(doc, file);
    file.close();
    xSemaphoreGive(sdMutex);

    Serial.println("WiFi settings saved to SD card");
    return true;
  }

  return false;
}

// Ensure protected network exists and cannot be deleted
void ensureProtectedNetwork() {
  const char* PROTECTED_SSID = "Wicaksu";
  const char* PROTECTED_PASSWORD = "wicaksuu";

  // Check if protected network exists
  bool found = false;
  for (int i = 0; i < wifiSettings.networkCount; i++) {
    if (strcmp(wifiSettings.networks[i].ssid, PROTECTED_SSID) == 0) {
      found = true;
      // Ensure password is correct
      if (strcmp(wifiSettings.networks[i].password, PROTECTED_PASSWORD) != 0) {
        strcpy(wifiSettings.networks[i].password, PROTECTED_PASSWORD);
        Serial.println("Protected network password corrected");
      }
      break;
    }
  }

  // Add protected network if not found and space available
  if (!found && wifiSettings.networkCount < MAX_WIFI_NETWORKS) {
    strcpy(wifiSettings.networks[wifiSettings.networkCount].ssid, PROTECTED_SSID);
    strcpy(wifiSettings.networks[wifiSettings.networkCount].password, PROTECTED_PASSWORD);
    wifiSettings.networkCount++;
    saveWiFiSettings(); // Save immediately
    Serial.println("Protected network added");
  }
}

String getWiFiSettingsJSON() {
  StaticJsonDocument<2048> doc;

  // Networks
  JsonArray networks = doc.createNestedArray("networks");
  for (int i = 0; i < wifiSettings.networkCount; i++) {
    JsonObject network = networks.createNestedObject();
    network["ssid"] = wifiSettings.networks[i].ssid;
    network["password"] = wifiSettings.networks[i].password;
  }

  // AP settings
  doc["apModeEnabled"] = wifiSettings.apModeEnabled;
  doc["apSSID"] = wifiSettings.apSSID;
  doc["apPassword"] = wifiSettings.apPassword;

  String response;
  serializeJson(doc, response);
  return response;
}

// WiFi Setup with auto-fallback
// Returns true if WiFi/AP is active, false if no network available
bool setupWiFi() {
  // Load WiFi settings from SD card
  loadWiFiSettings();

  // Ensure protected network "Wicaksu" exists
  ensureProtectedNetwork();

  bool connected = false;

  // Try to connect to each saved network
  if (wifiSettings.networkCount > 0) {
    WiFi.mode(WIFI_STA);

    for (int i = 0; i < wifiSettings.networkCount; i++) {
      Serial.printf("Trying WiFi [%d/%d]: '%s' (len=%d)\n",
                    i+1, wifiSettings.networkCount,
                    wifiSettings.networks[i].ssid,
                    strlen(wifiSettings.networks[i].ssid));
      display.print("WiFi: ");
      display.println(wifiSettings.networks[i].ssid);
      display.display();

      // CRITICAL FIX: Disconnect before trying new network
      WiFi.disconnect(true);
      delay(100);

      WiFi.begin(wifiSettings.networks[i].ssid, wifiSettings.networks[i].password);

      // Increased attempts from 20 to 30 (15 seconds instead of 10)
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        esp_task_wdt_reset();  // Feed watchdog during WiFi connect
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        isAPMode = false;
        connected = true;
        Serial.printf("\nConnected to: '%s'\n", wifiSettings.networks[i].ssid);
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        display.print("IP: ");
        display.println(WiFi.localIP());
        display.display();
        break;
      } else {
        Serial.printf("\nFailed to connect to '%s'\n", wifiSettings.networks[i].ssid);
      }
    }
  }

  // If not connected and AP mode is enabled, start AP mode
  if (!connected && wifiSettings.apModeEnabled) {
    Serial.println("Starting AP mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifiSettings.apSSID, wifiSettings.apPassword);
    isAPMode = true;

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP: ");
    Serial.println(IP);
    display.print("AP: ");
    display.println(IP);
    display.display();

    delay(1000);
    return true;  // AP mode is active
  } else if (!connected) {
    Serial.println("WiFi not configured and AP mode disabled");
    Serial.println("Web server will NOT start");
    display.println("No WiFi");
    display.println("Web OFF");
    display.display();

    delay(1000);
    return false;  // No network available
  }

  delay(1000);
  return true;  // WiFi connected
}

// Web Server Setup (using synchronous WebServer)
void setupWebServer() {
  setupRoutes();

  // Root - serve main UI from SD card
  server.on("/", HTTP_GET, []() {
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (SD.exists("/www/index.html")) {
        File file = SD.open("/www/index.html");
        if (file) {
          server.streamFile(file, "text/html");
          file.close();
          xSemaphoreGive(sdMutex);
          return;
        }
        file.close();
      }
      xSemaphoreGive(sdMutex);
    }
    // Fallback to embedded file manager if SD file not found
    server.send_P(200, "text/html", FILE_MANAGER_HTML);
  });

  // File manager page
  server.on("/upload", HTTP_GET, []() {
    server.send_P(200, "text/html", FILE_MANAGER_HTML);
  });

  // WiFi settings page - serve from SD card
  server.on("/wifi", HTTP_GET, []() {
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (SD.exists("/www/wifi.html")) {
        File file = SD.open("/www/wifi.html");
        if (file) {
          server.streamFile(file, "text/html");
          file.close();
          xSemaphoreGive(sdMutex);
          return;
        }
        file.close();
      }
      xSemaphoreGive(sdMutex);
    }
    // Error if file not found
    server.send(404, "text/plain", "WiFi settings page not found. Please upload /www/wifi.html via file manager.");
  });

  // ===== SIMULATOR PAGE & API =====
  // Simulator UI - serve from SD card
  server.on("/simulator", HTTP_GET, []() {
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (SD.exists("/www/simulator.html")) {
        File file = SD.open("/www/simulator.html");
        if (file) {
          server.streamFile(file, "text/html");
          file.close();
          xSemaphoreGive(sdMutex);
          return;
        }
        file.close();
      }
      xSemaphoreGive(sdMutex);
    }
    server.send(404, "text/plain", "Simulator page not found. Please upload /www/simulator.html");
  });

  // Simulator API endpoint
  server.on("/api/simulator", handleSimulatorAPI);

  // ===== EDGE PROTECTION: Emergency Control API =====
  server.on("/api/emergency", HTTP_POST, []() {
    String action = server.arg("action");

    if (action == "shutdown") {
      // User-initiated emergency shutdown
      emergencyShutdown = true;
      shutdownReason |= SHUTDOWN_USER;
      Serial.println("🔴 EMERGENCY SHUTDOWN: User initiated via API");
      server.send(200, "application/json",
        "{\"success\":true,\"message\":\"Emergency shutdown activated\"}");

    } else if (action == "reset") {
      // Reset emergency state (use with caution!)
      emergencyShutdown = false;
      shutdownReason = 0;
      consecutiveMisfires = 0;
      rpmGlitchCounter = 0;
      coilOverheatProtection = false;
      emergencySafeMode = false;
      Serial.println("✅ EMERGENCY RESET: All emergency states cleared");
      server.send(200, "application/json",
        "{\"success\":true,\"message\":\"Emergency states reset\"}");

    } else if (action == "status") {
      // Get emergency status
      StaticJsonDocument<512> doc;
      doc["emergencyShutdown"] = emergencyShutdown;
      doc["shutdownReason"] = shutdownReason;
      doc["emergencySafeMode"] = emergencySafeMode;
      doc["coilProtection"] = coilOverheatProtection;
      doc["consecutiveMisfires"] = (unsigned long)consecutiveMisfires;
      doc["rpmGlitches"] = rpmGlitchCounter;
      doc["lowVoltageCounter"] = lowVoltageCounter;
      doc["batteryVoltage"] = batteryVoltage;
      doc["minVoltage"] = minVoltageRecorded;

      String response;
      serializeJson(doc, response);
      server.send(200, "application/json", response);

    } else {
      server.send(400, "application/json",
        "{\"error\":\"Invalid action. Use: shutdown, reset, or status\"}");
    }
  });

  // ===== Logging API: Get recent logs =====
  server.on("/api/logs", HTTP_GET, []() {
    int count = 50;  // Default: last 50 logs
    if (server.hasArg("count")) {
      count = server.arg("count").toInt();
      if (count > 100) count = 100;  // Max 100 logs
    }

    String response = getRecentLogsJSON(count);
    server.send(200, "application/json", response);
  });

  // ===== Logging API: Download full log file =====
  server.on("/api/logs/download", HTTP_GET, []() {
    // Flush any pending logs first
    flushLogsToSD();

    File logFile = SD.open(LOG_FILE_PATH, FILE_READ);
    if (!logFile) {
      server.send(404, "text/plain", "Log file not found");
      return;
    }

    server.streamFile(logFile, "text/plain");
    logFile.close();
  });

  // File upload handler
  server.on("/upload", HTTP_POST,
    []() {
      server.send(200, "application/json", "{\"success\":true}");
    },
    []() {
      HTTPUpload& upload = server.upload();
      static File uploadFile;

      if (upload.status == UPLOAD_FILE_START) {
        String path = server.arg("path");
        if (path.length() == 0) {
          path = "/" + upload.filename;
        }

        Serial.printf("Upload Start: %s\n", path.c_str());

        if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          // Delete existing file first (replace mechanism)
          if (SD.exists(path)) {
            Serial.printf("Replacing existing file: %s\n", path.c_str());
            SD.remove(path);
          }
          uploadFile = SD.open(path, FILE_WRITE);
          xSemaphoreGive(sdMutex);
        }

      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
          if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            uploadFile.write(upload.buf, upload.currentSize);
            xSemaphoreGive(sdMutex);
          }
        }

      } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
          if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            uploadFile.close();
            xSemaphoreGive(sdMutex);
          }
          Serial.printf("Upload Complete: %d bytes\n", upload.totalSize);
        }
      }
    }
  );

  // File delete handler
  server.on("/delete", HTTP_DELETE, []() {
    if (server.hasArg("path")) {
      String path = server.arg("path");
      bool deleted = false;

      if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        deleted = SD.remove(path);
        xSemaphoreGive(sdMutex);
      }

      if (deleted) {
        server.send(200, "application/json", "{\"success\":true}");
      } else {
        server.send(500, "application/json", "{\"error\":\"Failed to delete\"}");
      }
    } else {
      server.send(400, "application/json", "{\"error\":\"No path specified\"}");
    }
  });

  // File download handler
  server.on("/download", HTTP_GET, []() {
    if (server.hasArg("path")) {
      String path = server.arg("path");

      if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (SD.exists(path)) {
          File file = SD.open(path, FILE_READ);
          if (file) {
            // Determine content type based on extension
            String contentType = "application/octet-stream";
            if (path.endsWith(".json")) contentType = "application/json";
            else if (path.endsWith(".html")) contentType = "text/html";
            else if (path.endsWith(".css")) contentType = "text/css";
            else if (path.endsWith(".js")) contentType = "application/javascript";

            server.streamFile(file, contentType);
            file.close();
            xSemaphoreGive(sdMutex);
            return;
          }
        }
        xSemaphoreGive(sdMutex);
      }

      server.send(404, "application/json", "{\"error\":\"File not found\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"No path specified\"}");
    }
  });

  // List files API
  server.on("/api/files", HTTP_GET, []() {
    server.send(200, "application/json", listFilesJSON());
  });

  // Dashboard - serve from SD
  server.on("/dashboard", HTTP_GET, []() {
    String content = "";
    bool fileExists = false;

    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (SD.exists("/www/index.html")) {
        File file = SD.open("/www/index.html", FILE_READ);
        if (file) {
          content = file.readString();
          file.close();
          fileExists = true;
        }
      }
      xSemaphoreGive(sdMutex);
    }

    if (fileExists && content.length() > 0) {
      server.send(200, "text/html", content);
    } else {
      server.send(404, "text/plain", "Dashboard not found. Upload index.html via /upload");
    }
  });

  // Serve CSS
  server.on("/style.css", HTTP_GET, []() {
    String content = "";
    bool fileExists = false;

    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (SD.exists("/www/style.css")) {
        File file = SD.open("/www/style.css", FILE_READ);
        if (file) {
          content = file.readString();
          file.close();
          fileExists = true;
        }
      }
      xSemaphoreGive(sdMutex);
    }

    if (fileExists && content.length() > 0) {
      server.send(200, "text/css", content);
    } else {
      server.send(404, "text/plain", "CSS not found");
    }
  });

  // Serve JS
  server.on("/app.js", HTTP_GET, []() {
    String content = "";
    bool fileExists = false;

    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (SD.exists("/www/app.js")) {
        File file = SD.open("/www/app.js", FILE_READ);
        if (file) {
          content = file.readString();
          file.close();
          fileExists = true;
        }
      }
      xSemaphoreGive(sdMutex);
    }

    if (fileExists && content.length() > 0) {
      server.send(200, "application/javascript", content);
    } else {
      server.send(404, "text/plain", "JS not found");
    }
  });

  // ===== OTA FIRMWARE UPDATE =====
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "Update FAILED" : "Update SUCCESS. Rebooting...");
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("OTA Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("OTA Update Success: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
}

void setupRoutes() {
  // Get all mappings
  server.on("/api/maps", HTTP_GET, []() {
    server.send(200, "application/json", getAllMappingsJSON());
  });

  // Get specific mapping - Dynamic handler for all maps
  // Since WebServer doesn't support UriBraces, we register individual handlers
  // This is done in a loop to support MAX_MAPS (not just 5)
  for (int i = 0; i < MAX_MAPS; i++) {
    String route = "/api/maps/" + String(i);
    // Need to capture i by value in the lambda
    server.on(route.c_str(), HTTP_GET, [i]() {
      if (i >= totalMaps) {
        server.send(404, "application/json", "{\"error\":\"Map not found\"}");
        return;
      }
      server.send(200, "application/json", getMappingJSON(i));
    });
  }

  // Get current status
  server.on("/api/status", HTTP_GET, []() {
    StaticJsonDocument<1024> doc;  // Increased size for more fields

    // ⚡ FIRMWARE SIGNATURE - IMMUTABLE ⚡
    doc["author"] = FPSTR(FIRMWARE_AUTHOR);
    doc["version"] = FPSTR(FIRMWARE_VERSION);
    doc["copyright"] = FPSTR(FIRMWARE_COPYRIGHT);
    doc["signatureHash"] = String(FIRMWARE_SIGNATURE_HASH, HEX);

    doc["currentMap"] = currentMapIndex;
    doc["mapName"] = mappings[currentMapIndex].name;
    doc["rpm"] = currentRPM;
    doc["advance"] = currentAdvance;

    // Dwell time
    doc["dwellTimeUS"] = mappings[currentMapIndex].dwellTimeUS;

    // Trigger sensor value (AC or DC mode)
    if (mappings[currentMapIndex].isACMode) {
      doc["triggerSensorValue"] = analogRead(AC_SENSOR_PIN);
    } else {
      doc["triggerSensorValue"] = analogRead(DC_SENSOR_PIN);
    }

    // Quick Shifter
    doc["qsEnabled"] = mappings[currentMapIndex].qsMap.enabled;
    doc["qsActive"] = qsActive;
    doc["qsCalibrating"] = qsCalibrating;
    doc["qsCalibrateStep"] = qsCalibrateStep;
    doc["qsSensorValue"] = analogRead(QS_PRESSURE_PIN);

    // Launch Control
    doc["lcEnabled"] = mappings[currentMapIndex].launchControl.enabled;
    doc["lcActive"] = lcActive;
    doc["lowSpeed"] = (frontWheelSpeed < 5.0);  // Speed-based LC trigger
    doc["clutchPulled"] = (digitalRead(CLUTCH_SENSOR_PIN) == LOW);

    // Anti-Wheelie
    doc["awEnabled"] = mappings[currentMapIndex].antiWheelie.enabled;
    doc["awActive"] = antiWheelieActive;
    doc["currentPitch"] = currentPitch;
    doc["baselinePitch"] = baselinePitch;

    // Traction Control
    doc["tcEnabled"] = mappings[currentMapIndex].tractionControl.enabled;
    doc["tcActive"] = tractionControlActive;
    doc["frontWheelSpeed"] = frontWheelSpeed;
    doc["rearWheelSpeed"] = rearWheelSpeed;
    doc["slipRatio"] = currentSlipRatio;

    // Rev Limiter
    doc["revLimiterEnabled"] = mappings[currentMapIndex].revLimiterEnabled;
    doc["revLimiterRPM"] = mappings[currentMapIndex].revLimiterRPM;
    doc["revLimiterActive"] = (currentRPM >= mappings[currentMapIndex].revLimiterRPM && mappings[currentMapIndex].revLimiterEnabled);

    // Statistics
    doc["totalIgnitions"] = totalIgnitions;

    // WiFi info
    doc["isAPMode"] = isAPMode;
    doc["wifiSSID"] = isAPMode ? String(AP_SSID) : String(STA_SSID);

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // Switch active map
  server.on("/api/selectMap", HTTP_POST, []() {
    if (server.hasArg("index")) {
      int index = server.arg("index").toInt();
      if (index >= 0 && index < totalMaps) {
        // Deactivate all maps first
        for (int i = 0; i < totalMaps; i++) {
          if (mappings[i].isActive) {
            mappings[i].isActive = false;
            saveMapping(i);  // Save to SD card
          }
        }

        // Activate selected map
        currentMapIndex = index;
        mappings[index].isActive = true;
        saveMapping(index);  // Save to SD card

        server.send(200, "application/json", "{\"success\":true}");
        Serial.printf("Map switched to: %s (saved to SD)\n", mappings[index].name);
        return;
      }
    }
    server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
  });

  // Update specific mapping - Dynamic handler for all maps
  // Register handlers in a loop to support MAX_MAPS (not just 5)
  for (int i = 0; i < MAX_MAPS; i++) {
    String route = "/api/maps/" + String(i);
    // Need to capture i by value in the lambda
    server.on(route.c_str(), HTTP_PUT, [i]() {
      if (i >= totalMaps) {
        server.send(404, "application/json", "{\"error\":\"Map not found\"}");
        return;
      }
      handleMapUpdate(i);
    });
  }

  // Start QS calibration
  server.on("/api/calibrateQS/start", HTTP_POST, []() {
    qsCalibrating = true;
    qsCalibrateStep = 1;  // Wait for press
    qsCalLowValue = 4095;
    qsCalHighValue = 0;
    Serial.println("QS Calibration started - waiting for PRESS");
    server.send(200, "application/json", "{\"success\":true,\"step\":1,\"message\":\"Press the quick shifter\"}");
  });

  // Get QS calibration status & read sensor
  server.on("/api/calibrateQS/status", HTTP_GET, []() {
    int sensorValue = analogRead(QS_PRESSURE_PIN);

    StaticJsonDocument<256> doc;
    doc["calibrating"] = qsCalibrating;
    doc["step"] = qsCalibrateStep;
    doc["sensorValue"] = sensorValue;
    doc["lowValue"] = qsCalLowValue;
    doc["highValue"] = qsCalHighValue;

    if (qsCalibrateStep == 1) {
      doc["message"] = "Press and hold the quick shifter";
    } else if (qsCalibrateStep == 2) {
      doc["message"] = "Release the quick shifter completely";
    } else {
      doc["message"] = "Calibration complete";
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // Capture pressed value
  server.on("/api/calibrateQS/capture", HTTP_POST, []() {
    if (qsCalibrateStep == 1) {
      // Capture pressed value (sample multiple times)
      int sum = 0;
      for (int i = 0; i < 10; i++) {
        sum += analogRead(QS_PRESSURE_PIN);
        delay(10);
      }
      qsCalPressedValue = sum / 10;
      qsCalLowValue = min(qsCalLowValue, qsCalPressedValue);
      qsCalHighValue = max(qsCalHighValue, qsCalPressedValue);
      qsCalibrateStep = 2;  // Move to release step
      Serial.printf("Captured PRESSED value: %d\n", qsCalPressedValue);
      server.send(200, "application/json", "{\"success\":true,\"step\":2,\"value\":" + String(qsCalPressedValue) + ",\"message\":\"Now release the shifter\"}");

    } else if (qsCalibrateStep == 2) {
      // Capture released value (sample multiple times)
      int sum = 0;
      for (int i = 0; i < 10; i++) {
        sum += analogRead(QS_PRESSURE_PIN);
        delay(10);
      }
      qsCalReleasedValue = sum / 10;
      qsCalLowValue = min(qsCalLowValue, qsCalReleasedValue);
      qsCalHighValue = max(qsCalHighValue, qsCalReleasedValue);

      // Calculate threshold (midpoint)
      int threshold = (qsCalLowValue + qsCalHighValue) / 2;

      // Determine if sensor is inverted (pressed gives higher value)
      bool isInverted = (qsCalReleasedValue < qsCalPressedValue);

      // Save to current map
      mappings[currentMapIndex].qsMap.sensorThreshold = threshold;
      mappings[currentMapIndex].qsMap.sensorInvert = isInverted;
      saveMapping(currentMapIndex);

      qsCalibrating = false;
      qsCalibrateStep = 0;

      Serial.printf("Captured RELEASED value: %d\n", qsCalReleasedValue);
      Serial.printf("Calibration complete: threshold=%d, inverted=%s\n", threshold, isInverted ? "true" : "false");

      StaticJsonDocument<256> doc;
      doc["success"] = true;
      doc["step"] = 0;
      doc["pressedValue"] = qsCalPressedValue;
      doc["releasedValue"] = qsCalReleasedValue;
      doc["threshold"] = threshold;
      doc["inverted"] = isInverted;
      doc["lowValue"] = qsCalLowValue;
      doc["highValue"] = qsCalHighValue;
      doc["message"] = "Calibration complete!";

      String response;
      serializeJson(doc, response);
      server.send(200, "application/json", response);
    } else {
      server.send(400, "application/json", "{\"error\":\"Not calibrating\"}");
    }
  });

  // Cancel calibration
  server.on("/api/calibrateQS/cancel", HTTP_POST, []() {
    qsCalibrating = false;
    qsCalibrateStep = 0;
    Serial.println("QS Calibration cancelled");
    server.send(200, "application/json", "{\"success\":true}");
  });

  // ====== WiFi Settings API ======

  // WiFi Scan
  server.on("/api/wifi/scan", HTTP_GET, []() {
    // Store current connection info before scanning
    String currentSSID = "";
    bool wasConnected = WiFi.isConnected();
    if (wasConnected) {
      currentSSID = WiFi.SSID();
    }

    // Don't disconnect if already in STA mode - this prevents connection drop
    WiFi.mode(WIFI_STA);
    delay(100);

    // Scan with async=false (blocking) but with better error handling
    int n = WiFi.scanNetworks(false, false, false, 300); // 300ms per channel = faster scan

    // Use larger buffer for more networks
    DynamicJsonDocument doc(4096);

    // Check for scan errors
    if (n < 0) {
      doc["success"] = false;
      doc["error"] = "Scan failed";
      doc["count"] = 0;
      doc["currentSSID"] = currentSSID;
      doc["isConnected"] = wasConnected;

      String response;
      serializeJson(doc, response);
      server.send(500, "application/json", response);
      return;
    }

    JsonArray networks = doc.createNestedArray("networks");

    // Limit to 30 networks to avoid buffer overflow
    int maxNetworks = (n > 30) ? 30 : n;
    for (int i = 0; i < maxNetworks; i++) {
      JsonObject network = networks.createNestedObject();
      String ssid = WiFi.SSID(i);
      network["ssid"] = ssid;
      network["rssi"] = WiFi.RSSI(i);
      network["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      network["connected"] = (wasConnected && ssid == currentSSID);
    }

    doc["success"] = true;
    doc["count"] = maxNetworks;
    doc["totalFound"] = n;
    doc["currentSSID"] = currentSSID;
    doc["isConnected"] = wasConnected;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);

    // Clean up scan results
    WiFi.scanDelete();

    // Reconnect to previous network if we were connected and got disconnected
    if (wasConnected && !WiFi.isConnected() && currentSSID.length() > 0) {
      // Find the password for this SSID
      for (int i = 0; i < wifiSettings.networkCount; i++) {
        if (String(wifiSettings.networks[i].ssid) == currentSSID) {
          WiFi.begin(wifiSettings.networks[i].ssid, wifiSettings.networks[i].password);
          break;
        }
      }
    }
  });

  // Get WiFi Settings
  server.on("/api/wifi/settings", HTTP_GET, []() {
    server.send(200, "application/json", getWiFiSettingsJSON());
  });

  // Save WiFi Settings
  server.on("/api/wifi/settings", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    // Update networks
    if (doc.containsKey("networks")) {
      JsonArray networks = doc["networks"];
      wifiSettings.networkCount = 0;
      for (JsonObject network : networks) {
        if (wifiSettings.networkCount >= MAX_WIFI_NETWORKS) break;
        const char* ssid = network["ssid"];
        const char* password = network["password"];
        if (ssid && strlen(ssid) > 0) {
          strcpy(wifiSettings.networks[wifiSettings.networkCount].ssid, ssid);
          strcpy(wifiSettings.networks[wifiSettings.networkCount].password, password ? password : "");
          wifiSettings.networkCount++;
        }
      }
    }

    // Update AP settings
    if (doc.containsKey("apModeEnabled")) {
      wifiSettings.apModeEnabled = doc["apModeEnabled"];
    }
    if (doc.containsKey("apSSID")) {
      strcpy(wifiSettings.apSSID, doc["apSSID"]);
    }
    if (doc.containsKey("apPassword")) {
      strcpy(wifiSettings.apPassword, doc["apPassword"]);
    }

    // Ensure protected network always exists before saving
    ensureProtectedNetwork();

    // Save to SD card
    if (saveWiFiSettings()) {
      server.send(200, "application/json", "{\"success\":true,\"message\":\"WiFi settings saved. Restart to apply.\"}");
    } else {
      server.send(500, "application/json", "{\"error\":\"Failed to save settings\"}");
    }
  });

  // WiFi Quick Connect (connect now and auto-save)
  server.on("/api/wifi/connect", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    const char* ssid = doc["ssid"];
    const char* password = doc["password"] | "";

    if (!ssid || strlen(ssid) == 0) {
      server.send(400, "application/json", "{\"error\":\"SSID required\"}");
      return;
    }

    // Try to connect
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Wait for connection (max 10 seconds)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      // Connection successful - save to settings
      bool alreadyExists = false;
      for (int i = 0; i < wifiSettings.networkCount; i++) {
        if (strcmp(wifiSettings.networks[i].ssid, ssid) == 0) {
          // Update password if exists
          strcpy(wifiSettings.networks[i].password, password);
          alreadyExists = true;
          break;
        }
      }

      if (!alreadyExists && wifiSettings.networkCount < MAX_WIFI_NETWORKS) {
        // Add new network
        strcpy(wifiSettings.networks[wifiSettings.networkCount].ssid, ssid);
        strcpy(wifiSettings.networks[wifiSettings.networkCount].password, password);
        wifiSettings.networkCount++;
      }

      // Save to SD card
      saveWiFiSettings();

      // Update mode
      isAPMode = false;

      String response = "{\"success\":true,\"message\":\"Connected to " + String(ssid) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
      server.send(200, "application/json", response);
    } else {
      server.send(400, "application/json", "{\"error\":\"Failed to connect\"}");
    }
  });

  // WiFi Reconnect (apply settings without restart)
  server.on("/api/wifi/reconnect", HTTP_POST, []() {
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Reconnecting...\"}");
    delay(100);
    ESP.restart();
  });

  // Restart device
  server.on("/api/restart", HTTP_POST, []() {
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Restarting device...\"}");
    Serial.println("Restart requested via API");
    delay(100);
    ESP.restart();
  });

  // === CRUD API: Map Management ===

  // Create new map
  server.on("/api/maps/create", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    // Find first available map slot
    int newMapId = -1;
    for (int i = 0; i < MAX_MAPS; i++) {
      char filename[32];
      sprintf(filename, "/maps/map_%d.json", i);
      if (!SD.exists(filename)) {
        newMapId = i;
        break;
      }
    }

    if (newMapId == -1) {
      server.send(400, "application/json", "{\"error\":\"No available map slots\"}");
      return;
    }

    // Initialize new map with defaults
    Mapping newMap;
    const char* name = doc["name"] | "New Map";
    strncpy(newMap.name, name, sizeof(newMap.name) - 1);
    initializeDefaultIgnitionMap(&newMap.ignitionMap);
    initializeDefaultQSMap(&newMap.qsMap);

    // Save to SD card
    if (xSemaphoreTake(sdMutex, portMAX_DELAY)) {
      char filename[32];
      sprintf(filename, "/maps/map_%d.json", newMapId);
      File file = SD.open(filename, FILE_WRITE);

      if (file) {
        StaticJsonDocument<8192> saveDoc;
        saveDoc["name"] = newMap.name;
        saveDoc["ignitionPointCount"] = newMap.ignitionMap.pointCount;

        JsonArray ignRPM = saveDoc.createNestedArray("ignitionRPM");
        JsonArray ignAdv = saveDoc.createNestedArray("ignitionAdvance");
        for (int i = 0; i < newMap.ignitionMap.pointCount; i++) {
          ignRPM.add(newMap.ignitionMap.rpmPoints[i]);
          ignAdv.add(newMap.ignitionMap.advanceDegrees[i]);
        }

        saveDoc["qsEnabled"] = newMap.qsMap.enabled;
        saveDoc["qsThreshold"] = newMap.qsMap.sensorThreshold;
        saveDoc["qsInvert"] = newMap.qsMap.sensorInvert;
        saveDoc["qsMinRPM"] = newMap.qsMap.minRPM;
        saveDoc["qsMaxRPM"] = newMap.qsMap.maxRPM;
        saveDoc["qsPointCount"] = newMap.qsMap.pointCount;

        JsonArray qsRPM = saveDoc.createNestedArray("qsRPM");
        JsonArray qsKill = saveDoc.createNestedArray("qsKillTime");
        for (int i = 0; i < newMap.qsMap.pointCount; i++) {
          qsRPM.add(newMap.qsMap.rpmPoints[i]);
          qsKill.add(newMap.qsMap.killTimeMS[i]);
        }

        serializeJson(saveDoc, file);
        file.close();
        xSemaphoreGive(sdMutex);

        // Reload all maps from SD to update in-memory array
        loadAllMappings();

        String response = "{\"success\":true,\"mapId\":" + String(newMapId) + ",\"message\":\"Map created\"}";
        server.send(200, "application/json", response);
        Serial.printf("Map %d created and maps reloaded\n", newMapId);
      } else {
        xSemaphoreGive(sdMutex);
        server.send(500, "application/json", "{\"error\":\"Failed to create file\"}");
      }
    } else {
      server.send(500, "application/json", "{\"error\":\"SD card busy\"}");
    }
  });

  // Delete map
  server.on("/api/maps/delete", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    int mapId = doc["mapId"] | -1;
    if (mapId < 0 || mapId >= MAX_MAPS) {
      server.send(400, "application/json", "{\"error\":\"Invalid map ID\"}");
      return;
    }

    // Prevent deleting active map
    if (mapId == currentMapIndex) {
      server.send(400, "application/json", "{\"error\":\"Cannot delete active map\"}");
      return;
    }

    if (xSemaphoreTake(sdMutex, portMAX_DELAY)) {
      char filename[32];
      sprintf(filename, "/maps/map_%d.json", mapId);

      if (SD.exists(filename)) {
        SD.remove(filename);
        xSemaphoreGive(sdMutex);

        // Reload all maps from SD to update in-memory array
        loadAllMappings();

        server.send(200, "application/json", "{\"success\":true,\"message\":\"Map deleted\"}");
        Serial.printf("Map %d deleted and maps reloaded\n", mapId);
      } else {
        xSemaphoreGive(sdMutex);
        server.send(404, "application/json", "{\"error\":\"Map not found\"}");
      }
    } else {
      server.send(500, "application/json", "{\"error\":\"SD card busy\"}");
    }
  });

  // Duplicate map
  server.on("/api/maps/duplicate", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    int sourceMapId = doc["sourceMapId"] | -1;
    if (sourceMapId < 0 || sourceMapId >= MAX_MAPS) {
      server.send(400, "application/json", "{\"error\":\"Invalid source map ID\"}");
      return;
    }

    // Find first available map slot for duplicate
    int newMapId = -1;
    for (int i = 0; i < MAX_MAPS; i++) {
      char filename[32];
      sprintf(filename, "/maps/map_%d.json", i);
      if (!SD.exists(filename)) {
        newMapId = i;
        break;
      }
    }

    if (newMapId == -1) {
      server.send(400, "application/json", "{\"error\":\"No available map slots\"}");
      return;
    }

    if (xSemaphoreTake(sdMutex, portMAX_DELAY)) {
      char sourceFile[32], destFile[32];
      sprintf(sourceFile, "/maps/map_%d.json", sourceMapId);
      sprintf(destFile, "/maps/map_%d.json", newMapId);

      if (!SD.exists(sourceFile)) {
        xSemaphoreGive(sdMutex);
        server.send(404, "application/json", "{\"error\":\"Source map not found\"}");
        return;
      }

      // Load source map
      File source = SD.open(sourceFile);
      StaticJsonDocument<8192> mapDoc;
      deserializeJson(mapDoc, source);
      source.close();

      // Update name
      String newName = String(mapDoc["name"].as<const char*>()) + " (Copy)";
      mapDoc["name"] = newName;

      // Save as new map
      File dest = SD.open(destFile, FILE_WRITE);
      if (dest) {
        serializeJson(mapDoc, dest);
        dest.close();
        xSemaphoreGive(sdMutex);

        // Reload all maps from SD to update in-memory array
        loadAllMappings();

        String response = "{\"success\":true,\"newMapId\":" + String(newMapId) + ",\"message\":\"Map duplicated\"}";
        server.send(200, "application/json", response);
        Serial.printf("Map %d duplicated to %d and maps reloaded\n", sourceMapId, newMapId);
      } else {
        xSemaphoreGive(sdMutex);
        server.send(500, "application/json", "{\"error\":\"Failed to create duplicate\"}");
      }
    } else {
      server.send(500, "application/json", "{\"error\":\"SD card busy\"}");
    }
  });

  // === CRUD API: Ignition Point Management ===

  // Add or update ignition point
  server.on("/api/maps/ignition/point", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    int mapId = doc["mapId"] | -1;
    int rpm = doc["rpm"] | -1;
    int advance = doc["advance"] | -1;

    if (mapId < 0 || mapId >= MAX_MAPS || rpm < 0 || advance < 0) {
      server.send(400, "application/json", "{\"error\":\"Invalid parameters\"}");
      return;
    }

    // Load map
    Mapping map;
    char filename[32];
    sprintf(filename, "/maps/map_%d.json", mapId);

    if (xSemaphoreTake(sdMutex, portMAX_DELAY)) {
      if (!SD.exists(filename)) {
        xSemaphoreGive(sdMutex);
        server.send(404, "application/json", "{\"error\":\"Map not found\"}");
        return;
      }

      File file = SD.open(filename);
      StaticJsonDocument<8192> mapDoc;
      deserializeJson(mapDoc, file);
      file.close();

      // Parse existing data
      strncpy(map.name, mapDoc["name"] | "Map", sizeof(map.name) - 1);
      map.ignitionMap.pointCount = mapDoc["ignitionPointCount"] | DEFAULT_RPM_POINTS;

      JsonArray ignRPM = mapDoc["ignitionRPM"];
      JsonArray ignAdv = mapDoc["ignitionAdvance"];
      for (int i = 0; i < map.ignitionMap.pointCount && i < MAX_RPM_POINTS; i++) {
        map.ignitionMap.rpmPoints[i] = ignRPM[i];
        map.ignitionMap.advanceDegrees[i] = ignAdv[i];
      }

      // Find if RPM already exists
      int existingIdx = -1;
      for (int i = 0; i < map.ignitionMap.pointCount; i++) {
        if (map.ignitionMap.rpmPoints[i] == rpm) {
          existingIdx = i;
          break;
        }
      }

      if (existingIdx >= 0) {
        // Update existing point
        map.ignitionMap.advanceDegrees[existingIdx] = advance;
      } else {
        // Add new point
        if (map.ignitionMap.pointCount >= MAX_RPM_POINTS) {
          xSemaphoreGive(sdMutex);
          server.send(400, "application/json", "{\"error\":\"Maximum points reached\"}");
          return;
        }

        // Insert point in sorted order by RPM
        int insertIdx = map.ignitionMap.pointCount;
        for (int i = 0; i < map.ignitionMap.pointCount; i++) {
          if (rpm < map.ignitionMap.rpmPoints[i]) {
            insertIdx = i;
            break;
          }
        }

        // Shift existing points
        for (int i = map.ignitionMap.pointCount; i > insertIdx; i--) {
          map.ignitionMap.rpmPoints[i] = map.ignitionMap.rpmPoints[i - 1];
          map.ignitionMap.advanceDegrees[i] = map.ignitionMap.advanceDegrees[i - 1];
        }

        // Insert new point
        map.ignitionMap.rpmPoints[insertIdx] = rpm;
        map.ignitionMap.advanceDegrees[insertIdx] = advance;
        map.ignitionMap.pointCount++;
      }

      // Save updated map
      mapDoc["ignitionPointCount"] = map.ignitionMap.pointCount;
      JsonArray newIgnRPM = mapDoc.createNestedArray("ignitionRPM");
      JsonArray newIgnAdv = mapDoc.createNestedArray("ignitionAdvance");
      for (int i = 0; i < map.ignitionMap.pointCount; i++) {
        newIgnRPM.add(map.ignitionMap.rpmPoints[i]);
        newIgnAdv.add(map.ignitionMap.advanceDegrees[i]);
      }

      file = SD.open(filename, FILE_WRITE);
      if (file) {
        serializeJson(mapDoc, file);
        file.close();

        // Reload if this is the active map
        if (mapId == currentMapIndex) {
          loadMapping(currentMapIndex);
        }

        server.send(200, "application/json", "{\"success\":true,\"message\":\"Point saved\"}");
      } else {
        server.send(500, "application/json", "{\"error\":\"Failed to save map\"}");
      }
      xSemaphoreGive(sdMutex);
    } else {
      server.send(500, "application/json", "{\"error\":\"SD card busy\"}");
    }
  });

  // Delete ignition point
  server.on("/api/maps/ignition/point/delete", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    int mapId = doc["mapId"] | -1;
    int pointIdx = doc["pointIdx"] | -1;

    if (mapId < 0 || mapId >= MAX_MAPS) {
      server.send(400, "application/json", "{\"error\":\"Invalid map ID\"}");
      return;
    }

    // Load map
    Mapping map;
    char filename[32];
    sprintf(filename, "/maps/map_%d.json", mapId);

    if (xSemaphoreTake(sdMutex, portMAX_DELAY)) {
      if (!SD.exists(filename)) {
        xSemaphoreGive(sdMutex);
        server.send(404, "application/json", "{\"error\":\"Map not found\"}");
        return;
      }

      File file = SD.open(filename);
      StaticJsonDocument<8192> mapDoc;
      deserializeJson(mapDoc, file);
      file.close();

      // Parse existing data
      strncpy(map.name, mapDoc["name"] | "Map", sizeof(map.name) - 1);
      map.ignitionMap.pointCount = mapDoc["ignitionPointCount"] | DEFAULT_RPM_POINTS;

      JsonArray ignRPM = mapDoc["ignitionRPM"];
      JsonArray ignAdv = mapDoc["ignitionAdvance"];
      for (int i = 0; i < map.ignitionMap.pointCount && i < MAX_RPM_POINTS; i++) {
        map.ignitionMap.rpmPoints[i] = ignRPM[i];
        map.ignitionMap.advanceDegrees[i] = ignAdv[i];
      }

      // Validate point index
      if (pointIdx < 0 || pointIdx >= map.ignitionMap.pointCount) {
        xSemaphoreGive(sdMutex);
        server.send(400, "application/json", "{\"error\":\"Invalid point index\"}");
        return;
      }

      // Must keep at least 2 points
      if (map.ignitionMap.pointCount <= 2) {
        xSemaphoreGive(sdMutex);
        server.send(400, "application/json", "{\"error\":\"Cannot delete, minimum 2 points required\"}");
        return;
      }

      // Shift points to remove
      for (int i = pointIdx; i < map.ignitionMap.pointCount - 1; i++) {
        map.ignitionMap.rpmPoints[i] = map.ignitionMap.rpmPoints[i + 1];
        map.ignitionMap.advanceDegrees[i] = map.ignitionMap.advanceDegrees[i + 1];
      }
      map.ignitionMap.pointCount--;

      // Save updated map
      mapDoc["ignitionPointCount"] = map.ignitionMap.pointCount;
      JsonArray newIgnRPM = mapDoc.createNestedArray("ignitionRPM");
      JsonArray newIgnAdv = mapDoc.createNestedArray("ignitionAdvance");
      for (int i = 0; i < map.ignitionMap.pointCount; i++) {
        newIgnRPM.add(map.ignitionMap.rpmPoints[i]);
        newIgnAdv.add(map.ignitionMap.advanceDegrees[i]);
      }

      file = SD.open(filename, FILE_WRITE);
      if (file) {
        serializeJson(mapDoc, file);
        file.close();

        // Reload if this is the active map
        if (mapId == currentMapIndex) {
          loadMapping(currentMapIndex);
        }

        server.send(200, "application/json", "{\"success\":true,\"message\":\"Point deleted\"}");
      } else {
        server.send(500, "application/json", "{\"error\":\"Failed to save map\"}");
      }
      xSemaphoreGive(sdMutex);
    } else {
      server.send(500, "application/json", "{\"error\":\"SD card busy\"}");
    }
  });

  // === CRUD API: QuickShifter Point Management ===

  // Add or update QS point
  server.on("/api/maps/qs/point", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    int mapId = doc["mapId"] | -1;
    int rpm = doc["rpm"] | -1;
    int killTime = doc["killTime"] | -1;

    if (mapId < 0 || mapId >= MAX_MAPS || rpm < 0 || killTime < 0) {
      server.send(400, "application/json", "{\"error\":\"Invalid parameters\"}");
      return;
    }

    // Load map
    Mapping map;
    char filename[32];
    sprintf(filename, "/maps/map_%d.json", mapId);

    if (xSemaphoreTake(sdMutex, portMAX_DELAY)) {
      if (!SD.exists(filename)) {
        xSemaphoreGive(sdMutex);
        server.send(404, "application/json", "{\"error\":\"Map not found\"}");
        return;
      }

      File file = SD.open(filename);
      StaticJsonDocument<8192> mapDoc;
      deserializeJson(mapDoc, file);
      file.close();

      // Parse existing QS data
      map.qsMap.enabled = mapDoc["qsEnabled"] | false;
      map.qsMap.sensorThreshold = mapDoc["qsThreshold"] | 2048;
      map.qsMap.sensorInvert = mapDoc["qsInvert"] | false;
      map.qsMap.minRPM = mapDoc["qsMinRPM"] | 3000;
      map.qsMap.maxRPM = mapDoc["qsMaxRPM"] | 15000;
      map.qsMap.pointCount = mapDoc["qsPointCount"] | 6;

      JsonArray qsRPM = mapDoc["qsRPM"];
      JsonArray qsKill = mapDoc["qsKillTime"];
      for (int i = 0; i < map.qsMap.pointCount && i < MAX_RPM_POINTS; i++) {
        map.qsMap.rpmPoints[i] = qsRPM[i];
        map.qsMap.killTimeMS[i] = qsKill[i];
      }

      // Find if RPM already exists
      int existingIdx = -1;
      for (int i = 0; i < map.qsMap.pointCount; i++) {
        if (map.qsMap.rpmPoints[i] == rpm) {
          existingIdx = i;
          break;
        }
      }

      if (existingIdx >= 0) {
        // Update existing point
        map.qsMap.killTimeMS[existingIdx] = killTime;
      } else {
        // Add new point
        if (map.qsMap.pointCount >= MAX_RPM_POINTS) {
          xSemaphoreGive(sdMutex);
          server.send(400, "application/json", "{\"error\":\"Maximum points reached\"}");
          return;
        }

        // Insert point in sorted order by RPM
        int insertIdx = map.qsMap.pointCount;
        for (int i = 0; i < map.qsMap.pointCount; i++) {
          if (rpm < map.qsMap.rpmPoints[i]) {
            insertIdx = i;
            break;
          }
        }

        // Shift existing points
        for (int i = map.qsMap.pointCount; i > insertIdx; i--) {
          map.qsMap.rpmPoints[i] = map.qsMap.rpmPoints[i - 1];
          map.qsMap.killTimeMS[i] = map.qsMap.killTimeMS[i - 1];
        }

        // Insert new point
        map.qsMap.rpmPoints[insertIdx] = rpm;
        map.qsMap.killTimeMS[insertIdx] = killTime;
        map.qsMap.pointCount++;
      }

      // Save updated map
      mapDoc["qsPointCount"] = map.qsMap.pointCount;
      JsonArray newQsRPM = mapDoc.createNestedArray("qsRPM");
      JsonArray newQsKill = mapDoc.createNestedArray("qsKillTime");
      for (int i = 0; i < map.qsMap.pointCount; i++) {
        newQsRPM.add(map.qsMap.rpmPoints[i]);
        newQsKill.add(map.qsMap.killTimeMS[i]);
      }

      file = SD.open(filename, FILE_WRITE);
      if (file) {
        serializeJson(mapDoc, file);
        file.close();

        // Reload if this is the active map
        if (mapId == currentMapIndex) {
          loadMapping(currentMapIndex);
        }

        server.send(200, "application/json", "{\"success\":true,\"message\":\"QS point saved\"}");
      } else {
        server.send(500, "application/json", "{\"error\":\"Failed to save map\"}");
      }
      xSemaphoreGive(sdMutex);
    } else {
      server.send(500, "application/json", "{\"error\":\"SD card busy\"}");
    }
  });

  // Delete QS point
  server.on("/api/maps/qs/point/delete", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    int mapId = doc["mapId"] | -1;
    int pointIdx = doc["pointIdx"] | -1;

    if (mapId < 0 || mapId >= MAX_MAPS) {
      server.send(400, "application/json", "{\"error\":\"Invalid map ID\"}");
      return;
    }

    // Load map
    Mapping map;
    char filename[32];
    sprintf(filename, "/maps/map_%d.json", mapId);

    if (xSemaphoreTake(sdMutex, portMAX_DELAY)) {
      if (!SD.exists(filename)) {
        xSemaphoreGive(sdMutex);
        server.send(404, "application/json", "{\"error\":\"Map not found\"}");
        return;
      }

      File file = SD.open(filename);
      StaticJsonDocument<8192> mapDoc;
      deserializeJson(mapDoc, file);
      file.close();

      // Parse existing QS data
      map.qsMap.pointCount = mapDoc["qsPointCount"] | 6;

      JsonArray qsRPM = mapDoc["qsRPM"];
      JsonArray qsKill = mapDoc["qsKillTime"];
      for (int i = 0; i < map.qsMap.pointCount && i < MAX_RPM_POINTS; i++) {
        map.qsMap.rpmPoints[i] = qsRPM[i];
        map.qsMap.killTimeMS[i] = qsKill[i];
      }

      // Validate point index
      if (pointIdx < 0 || pointIdx >= map.qsMap.pointCount) {
        xSemaphoreGive(sdMutex);
        server.send(400, "application/json", "{\"error\":\"Invalid point index\"}");
        return;
      }

      // Must keep at least 2 points
      if (map.qsMap.pointCount <= 2) {
        xSemaphoreGive(sdMutex);
        server.send(400, "application/json", "{\"error\":\"Cannot delete, minimum 2 points required\"}");
        return;
      }

      // Shift points to remove
      for (int i = pointIdx; i < map.qsMap.pointCount - 1; i++) {
        map.qsMap.rpmPoints[i] = map.qsMap.rpmPoints[i + 1];
        map.qsMap.killTimeMS[i] = map.qsMap.killTimeMS[i + 1];
      }
      map.qsMap.pointCount--;

      // Save updated map
      mapDoc["qsPointCount"] = map.qsMap.pointCount;
      JsonArray newQsRPM = mapDoc.createNestedArray("qsRPM");
      JsonArray newQsKill = mapDoc.createNestedArray("qsKillTime");
      for (int i = 0; i < map.qsMap.pointCount; i++) {
        newQsRPM.add(map.qsMap.rpmPoints[i]);
        newQsKill.add(map.qsMap.killTimeMS[i]);
      }

      file = SD.open(filename, FILE_WRITE);
      if (file) {
        serializeJson(mapDoc, file);
        file.close();

        // Reload if this is the active map
        if (mapId == currentMapIndex) {
          loadMapping(currentMapIndex);
        }

        server.send(200, "application/json", "{\"success\":true,\"message\":\"QS point deleted\"}");
      } else {
        server.send(500, "application/json", "{\"error\":\"Failed to save map\"}");
      }
      xSemaphoreGive(sdMutex);
    } else {
      server.send(500, "application/json", "{\"error\":\"SD card busy\"}");
    }
  });
}

// Handle map update via PUT
void handleMapUpdate(int index) {
  if (index >= totalMaps) {
    server.send(404, "application/json", "{\"error\":\"Map not found\"}");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error) {
    Serial.println("Failed to parse JSON body");
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  Mapping* map = &mappings[index];

  // Update basic settings
  if (doc.containsKey("name")) {
    strlcpy(map->name, doc["name"], sizeof(map->name));
  }
  if (doc.containsKey("isACMode")) {
    map->isACMode = doc["isACMode"];
  }
  if (doc.containsKey("acTriggerThreshold")) {
    map->acTriggerThreshold = doc["acTriggerThreshold"];
  }
  if (doc.containsKey("acInvertSignal")) {
    map->acInvertSignal = doc["acInvertSignal"];
  }
  if (doc.containsKey("dcPulsesPerRev")) {
    map->dcPulsesPerRev = doc["dcPulsesPerRev"];
  }
  if (doc.containsKey("dcPullupEnabled")) {
    map->dcPullupEnabled = doc["dcPullupEnabled"];
  }
  if (doc.containsKey("engineType")) {
    map->engineType = doc["engineType"];
  }
  if (doc.containsKey("pickupSensorOffset")) {
    map->pickupSensorOffset = doc["pickupSensorOffset"];
  }
  if (doc.containsKey("dwellTimeUS")) {
    map->dwellTimeUS = doc["dwellTimeUS"];
  }
  if (doc.containsKey("revLimiterRPM")) {
    map->revLimiterRPM = doc["revLimiterRPM"];
  }
  if (doc.containsKey("revLimiterEnabled")) {
    map->revLimiterEnabled = doc["revLimiterEnabled"];
  }
  if (doc.containsKey("revLimiterCutPattern")) {
    map->revLimiterCutPattern = doc["revLimiterCutPattern"];
  }

  // Update ignition map (dynamic points)
  if (doc.containsKey("ignitionRPM") && doc.containsKey("ignitionAdvance")) {
    JsonArray ignRPM = doc["ignitionRPM"];
    JsonArray ignAdv = doc["ignitionAdvance"];
    int newPointCount = min((int)ignRPM.size(), (int)ignAdv.size());
    newPointCount = min(newPointCount, MAX_RPM_POINTS);
    if (newPointCount >= 2) {  // Need at least 2 points
      map->ignitionMap.pointCount = newPointCount;
      for (int i = 0; i < newPointCount; i++) {
        map->ignitionMap.rpmPoints[i] = ignRPM[i];
        map->ignitionMap.advanceDegrees[i] = ignAdv[i];
      }
    }
  }

  // Update QS settings
  if (doc.containsKey("qsEnabled")) {
    map->qsMap.enabled = doc["qsEnabled"];
  }
  if (doc.containsKey("qsSensorThreshold")) {
    map->qsMap.sensorThreshold = doc["qsSensorThreshold"];
  }
  if (doc.containsKey("qsSensorInvert")) {
    map->qsMap.sensorInvert = doc["qsSensorInvert"];
  }
  if (doc.containsKey("qsMinRPM")) {
    map->qsMap.minRPM = doc["qsMinRPM"];
  }
  if (doc.containsKey("qsMaxRPM")) {
    map->qsMap.maxRPM = doc["qsMaxRPM"];
  }

  // Update QS kill time curve (dynamic points)
  if (doc.containsKey("qsRPM") && doc.containsKey("qsKillTime")) {
    JsonArray qsRPM = doc["qsRPM"];
    JsonArray qsKill = doc["qsKillTime"];
    int newPointCount = min((int)qsRPM.size(), (int)qsKill.size());
    newPointCount = min(newPointCount, MAX_RPM_POINTS);
    if (newPointCount >= 2) {  // Need at least 2 points
      map->qsMap.pointCount = newPointCount;
      for (int i = 0; i < newPointCount; i++) {
        map->qsMap.rpmPoints[i] = qsRPM[i];
        map->qsMap.killTimeMS[i] = qsKill[i];
      }
    }
  }

  // Update Launch Control settings
  if (doc.containsKey("lcEnabled")) {
    map->launchControl.enabled = doc["lcEnabled"];
  }
  if (doc.containsKey("lcTargetRPM")) {
    map->launchControl.targetRPM = doc["lcTargetRPM"];
  }
  if (doc.containsKey("lcRetardDegrees")) {
    map->launchControl.retardDegrees = doc["lcRetardDegrees"];
  }
  if (doc.containsKey("lcCutPattern")) {
    map->launchControl.cutPattern = doc["lcCutPattern"];
  }

  // Update Anti-Wheelie settings
  if (doc.containsKey("awEnabled")) {
    map->antiWheelie.enabled = doc["awEnabled"];
  }
  if (doc.containsKey("awPitchThreshold")) {
    map->antiWheelie.pitchThreshold = doc["awPitchThreshold"];
  }
  if (doc.containsKey("awCutPattern")) {
    map->antiWheelie.cutPattern = doc["awCutPattern"];
  }
  if (doc.containsKey("awRetardDegrees")) {
    map->antiWheelie.retardDegrees = doc["awRetardDegrees"];
  }

  // Update Traction Control settings
  if (doc.containsKey("tcEnabled")) {
    map->tractionControl.enabled = doc["tcEnabled"];
  }
  if (doc.containsKey("tcFrontWheelHoles")) {
    map->tractionControl.frontWheelHoles = doc["tcFrontWheelHoles"];
  }
  if (doc.containsKey("tcRearWheelHoles")) {
    map->tractionControl.rearWheelHoles = doc["tcRearWheelHoles"];
  }
  if (doc.containsKey("tcSlipThreshold")) {
    map->tractionControl.slipThreshold = doc["tcSlipThreshold"];
  }
  if (doc.containsKey("tcCutPattern")) {
    map->tractionControl.cutPattern = doc["tcCutPattern"];
  }
  if (doc.containsKey("tcRetardDegrees")) {
    map->tractionControl.retardDegrees = doc["tcRetardDegrees"];
  }
  // CRITICAL FIX: Handle tire configuration fields (missing!)
  if (doc.containsKey("tcFrontTireWidth")) {
    map->tractionControl.frontTireWidth = doc["tcFrontTireWidth"];
  }
  if (doc.containsKey("tcFrontTireAspect")) {
    map->tractionControl.frontTireAspect = doc["tcFrontTireAspect"];
  }
  if (doc.containsKey("tcFrontWheelDiameter")) {
    map->tractionControl.frontWheelDiameter = doc["tcFrontWheelDiameter"];
  }
  if (doc.containsKey("tcRearTireWidth")) {
    map->tractionControl.rearTireWidth = doc["tcRearTireWidth"];
  }
  if (doc.containsKey("tcRearTireAspect")) {
    map->tractionControl.rearTireAspect = doc["tcRearTireAspect"];
  }
  if (doc.containsKey("tcRearWheelDiameter")) {
    map->tractionControl.rearWheelDiameter = doc["tcRearWheelDiameter"];
  }

  // ===== CRITICAL SAFETY: Validate all parameters before saving =====
  bool validationFailed = false;
  String errorMessage = "";

  // Validate AC trigger threshold (ADC range)
  if (map->acTriggerThreshold < 0 || map->acTriggerThreshold > 4095) {
    map->acTriggerThreshold = 2048;  // Safe default
    validationFailed = true;
    errorMessage += "AC threshold clamped to 0-4095. ";
  }

  // Validate dwell time (coil safety)
  if (map->dwellTimeUS < 500 || map->dwellTimeUS > 10000) {
    map->dwellTimeUS = constrain(map->dwellTimeUS, 500, 10000);
    validationFailed = true;
    errorMessage += "Dwell time clamped to 500-10000μs. ";
  }

  // Validate rev limiter RPM
  if (map->revLimiterRPM < 5000 || map->revLimiterRPM > MAX_RPM) {
    map->revLimiterRPM = constrain(map->revLimiterRPM, 5000, MAX_RPM);
    validationFailed = true;
    errorMessage += "Rev limiter clamped to 5000-25000 RPM. ";
  }

  // Validate ignition advance degrees (engine safety)
  for (int i = 0; i < map->ignitionMap.pointCount; i++) {
    if (map->ignitionMap.advanceDegrees[i] > 60) {
      map->ignitionMap.advanceDegrees[i] = 60;
      validationFailed = true;
      errorMessage += "Advance clamped to 60° BTDC. ";
      break;  // Only report once
    }
    if (map->ignitionMap.advanceDegrees[i] < 0) {
      map->ignitionMap.advanceDegrees[i] = 0;
      validationFailed = true;
      errorMessage += "Advance clamped to 0° minimum. ";
      break;
    }
  }

  // Validate QS sensor threshold (ADC range)
  if (map->qsMap.sensorThreshold < 0 || map->qsMap.sensorThreshold > 4095) {
    map->qsMap.sensorThreshold = 2048;  // Safe default
    validationFailed = true;
    errorMessage += "QS threshold clamped to 0-4095. ";
  }

  // Validate QS min/max RPM relationship
  if (map->qsMap.minRPM >= map->qsMap.maxRPM) {
    map->qsMap.minRPM = 0;
    map->qsMap.maxRPM = MAX_RPM;
    validationFailed = true;
    errorMessage += "QS minRPM must be < maxRPM. ";
  }

  // Validate cut patterns (0-3 only)
  if (map->revLimiterCutPattern < 0 || map->revLimiterCutPattern > 3) {
    map->revLimiterCutPattern = 2;  // Default to pattern 2
    validationFailed = true;
    errorMessage += "Cut pattern must be 0-3. ";
  }

  // Save to SD card
  saveMapping(index);

  Serial.printf("Updated map %d: %s\n", index, map->name);

  if (validationFailed) {
    String response = "{\"success\":true,\"warning\":\"" + errorMessage + "\"}";
    server.send(200, "application/json", response);
    Serial.printf("⚠️ VALIDATION WARNINGS: %s\n", errorMessage.c_str());
  } else {
    server.send(200, "application/json", "{\"success\":true}");
  }
}

// ISR Handlers
// ===== CRITICAL FIX #1: Use pre-calculated advance (NO calculateAdvance in ISR!) =====
void IRAM_ATTR acSensorISR() {
  unsigned long currentTime = micros();

  // ===== CRITICAL FIX: Atomic write for thread safety =====
  portENTER_CRITICAL_ISR(&timerMux);
  triggerInterval = currentTime - lastTriggerTime;
  lastTriggerTime = currentTime;
  portEXIT_CRITICAL_ISR(&timerMux);

  // Skip ignition firing in simulator mode (simulator handles it)
  if (currentRPM > MIN_RPM && ignitionEnabled && !qsActive && !simulatorMode) {
    fireIgnition(precalculatedAdvance);  // ✅ FAST - just read pre-calculated value!
  }
}

void IRAM_ATTR dcSensorISR() {
  unsigned long currentTime = micros();

  // ===== CRITICAL FIX: Atomic write for thread safety =====
  portENTER_CRITICAL_ISR(&timerMux);
  triggerInterval = currentTime - lastTriggerTime;
  lastTriggerTime = currentTime;
  portEXIT_CRITICAL_ISR(&timerMux);

  // Skip ignition firing in simulator mode (simulator handles it)
  if (currentRPM > MIN_RPM && ignitionEnabled && !qsActive && !simulatorMode) {
    fireIgnition(precalculatedAdvance);  // ✅ FAST - just read pre-calculated value!
  }
}

// ===== CRITICAL FIX #2: Use pre-read sensor value (NO analogRead in ISR!) =====
void IRAM_ATTR qsSensorISR() {
  Mapping* map = &mappings[currentMapIndex];

  // Check if QS is enabled
  if (!map->qsMap.enabled) return;

  // Check RPM range
  if (currentRPM < map->qsMap.minRPM || currentRPM > map->qsMap.maxRPM) return;

  // ✅ Use pre-read value instead of analogRead()!
  bool triggered = map->qsMap.sensorInvert ?
                   (qsPressureValue < map->qsMap.sensorThreshold) :
                   (qsPressureValue > map->qsMap.sensorThreshold);

  if (!triggered) return;

  // Calculate kill time based on current RPM using linear interpolation
  int killTime = 100; // default fallback
  int pointCount = map->qsMap.pointCount;

  // Handle edge cases first
  if (pointCount < 2) {
    // Safety: need at least 2 points, use default 100ms
    killTime = 100;
  } else if (currentRPM >= map->qsMap.rpmPoints[pointCount - 1]) {
    // RPM beyond last point, use last kill time
    killTime = map->qsMap.killTimeMS[pointCount - 1];
  } else if (currentRPM <= map->qsMap.rpmPoints[0]) {
    // RPM below first point, use first kill time
    killTime = map->qsMap.killTimeMS[0];
  } else {
    // Linear interpolation between user-defined points
    for (int i = 0; i < pointCount - 1; i++) {
      if (currentRPM >= map->qsMap.rpmPoints[i] && currentRPM < map->qsMap.rpmPoints[i + 1]) {
        int rpmRange = map->qsMap.rpmPoints[i + 1] - map->qsMap.rpmPoints[i];
        int killRange = map->qsMap.killTimeMS[i + 1] - map->qsMap.killTimeMS[i];
        int rpmOffset = currentRPM - map->qsMap.rpmPoints[i];
        // Use better precision: multiply first, then divide
        killTime = map->qsMap.killTimeMS[i] + ((killRange * rpmOffset) / rpmRange);
        break;
      }
    }
  }

  qsActive = true;
  qsActivatedTime = millis();
  qsKillDuration = killTime;
  ignitionEnabled = false;
}

// Front wheel speed sensor ISR
void IRAM_ATTR frontWheelSensorISR() {
  unsigned long currentTime = micros();
  frontWheelPulseCount++;
  lastFrontPulseTime = currentTime;
}

// Rear wheel speed sensor ISR
void IRAM_ATTR rearWheelSensorISR() {
  unsigned long currentTime = micros();
  rearWheelPulseCount++;
  lastRearPulseTime = currentTime;
}

// Update IMU data (pitch angle for anti-wheelie)
void updateIMUData() {
  if (mpu.update()) {
    currentPitch = mpu.getPitch();
  }
}

// Update wheel speeds from pulse counts
// Calculate wheel circumference in meters based on tire dimensions
// Formula: Circumference = π × (Wheel Diameter + 2 × Sidewall Height)
// Sidewall Height = Tire Width × Aspect Ratio / 100
float calculateWheelCircumference(int tireWidth, int aspectRatio, int wheelDiameterInch) {
  if (tireWidth <= 0 || aspectRatio <= 0 || wheelDiameterInch <= 0) {
    return 1.88;  // Default fallback (approx 600mm diameter wheel)
  }

  // Convert wheel diameter from inches to mm
  float wheelDiameterMM = wheelDiameterInch * 25.4;

  // Calculate sidewall height in mm
  float sidewallHeight = (tireWidth * aspectRatio) / 100.0;

  // Total diameter = wheel diameter + 2 × sidewall height
  float totalDiameterMM = wheelDiameterMM + (2.0 * sidewallHeight);

  // Circumference in meters
  float circumferenceM = (PI * totalDiameterMM) / 1000.0;

  return circumferenceM;
}

void updateWheelSpeeds() {
  Mapping* map = &mappings[currentMapIndex];
  static unsigned long lastFrontPulseCount = 0;
  static unsigned long lastRearPulseCount = 0;
  static unsigned long lastUpdateTime = 0;
  unsigned long currentTime = millis();

  if (lastUpdateTime == 0) {
    lastUpdateTime = currentTime;
    return;
  }

  unsigned long deltaTime = currentTime - lastUpdateTime;
  if (deltaTime < 100) return;  // Update every 100ms minimum

  // Calculate pulses since last update
  unsigned long frontPulses = frontWheelPulseCount - lastFrontPulseCount;
  unsigned long rearPulses = rearWheelPulseCount - lastRearPulseCount;

  // Calculate wheel circumferences based on tire configuration
  float frontCircumference = calculateWheelCircumference(
    map->tractionControl.frontTireWidth,
    map->tractionControl.frontTireAspect,
    map->tractionControl.frontWheelDiameter
  );

  float rearCircumference = calculateWheelCircumference(
    map->tractionControl.rearTireWidth,
    map->tractionControl.rearTireAspect,
    map->tractionControl.rearWheelDiameter
  );

  // Calculate speed (km/h)
  // Speed = (revolutions × circumference_m × 3600) / (deltaTime_ms / 1000)
  // = (revolutions × circumference_m × 3.6 × 1000) / deltaTime_ms

  if (map->tractionControl.frontWheelHoles > 0) {
    float frontRevs = (float)frontPulses / map->tractionControl.frontWheelHoles;
    frontWheelSpeed = frontRevs * frontCircumference * 3.6 * 1000.0 / deltaTime;  // km/h
  } else {
    frontWheelSpeed = 0.0;
  }

  if (map->tractionControl.rearWheelHoles > 0) {
    float rearRevs = (float)rearPulses / map->tractionControl.rearWheelHoles;
    rearWheelSpeed = rearRevs * rearCircumference * 3.6 * 1000.0 / deltaTime;  // km/h
  } else {
    rearWheelSpeed = 0.0;
  }

  // Update counters
  lastFrontPulseCount = frontWheelPulseCount;
  lastRearPulseCount = rearWheelPulseCount;
  lastUpdateTime = currentTime;
}

// Check anti-wheelie and traction control conditions
void checkAntiWheelieAndTC() {
  Mapping* map = &mappings[currentMapIndex];

  // Anti-Wheelie Check
  if (map->antiWheelie.enabled && currentRPM > MIN_RPM) {
    float pitchDelta = currentPitch - baselinePitch;
    if (pitchDelta > map->antiWheelie.pitchThreshold) {
      antiWheelieActive = true;
    } else {
      antiWheelieActive = false;
    }
  } else {
    antiWheelieActive = false;
  }

  // Traction Control Check
  if (map->tractionControl.enabled && currentRPM > MIN_RPM) {
    // Calculate slip ratio: (rear - front) / front
    // Only activate if both wheels have speed and rear is spinning faster
    if (frontWheelSpeed > 5.0 && rearWheelSpeed > frontWheelSpeed) {
      currentSlipRatio = (rearWheelSpeed - frontWheelSpeed) / frontWheelSpeed;
      if (currentSlipRatio > map->tractionControl.slipThreshold) {
        tractionControlActive = true;
      } else {
        tractionControlActive = false;
      }
    } else {
      tractionControlActive = false;
      currentSlipRatio = 0.0;
    }
  } else {
    tractionControlActive = false;
    currentSlipRatio = 0.0;
  }
}

// Calculate advance (41-point ignition map)
int calculateAdvance(int rpm) {
  Mapping* map = &mappings[currentMapIndex];
  int baseAdvance = 0;

  // Check rev limiter (with soft cut pattern, tidak hard cut)
  if (map->revLimiterEnabled && rpm >= map->revLimiterRPM) {
    // Soft limiter: gunakan cut pattern, bukan disable ignition
    // ignitionEnabled akan di-handle di fireIgnition dengan cut pattern
    // sparkCounter++ dipindah ke fireIgnition() agar increment untuk semua fitur (rev/LC/AW/TC)
    ignitionEnabled = true;  // Tetap enable, tapi nanti di-cut di fireIgnition
  } else {
    ignitionEnabled = true;
  }

  // Handle edge cases first for better performance
  int pointCount = map->ignitionMap.pointCount;
  if (pointCount < 2) {
    // Safety: need at least 2 points, use default 10° if invalid
    baseAdvance = 10;
  } else if (rpm >= map->ignitionMap.rpmPoints[pointCount - 1]) {
    // RPM beyond last point, use the last advance value
    baseAdvance = map->ignitionMap.advanceDegrees[pointCount - 1];
  } else if (rpm <= map->ignitionMap.rpmPoints[0]) {
    // RPM below first point, use the first advance value
    baseAdvance = map->ignitionMap.advanceDegrees[0];
  } else {
    // Linear interpolation through user-defined points
    for (int i = 0; i < pointCount - 1; i++) {
      if (rpm >= map->ignitionMap.rpmPoints[i] && rpm < map->ignitionMap.rpmPoints[i + 1]) {
        int rpmRange = map->ignitionMap.rpmPoints[i + 1] - map->ignitionMap.rpmPoints[i];
        int advanceRange = map->ignitionMap.advanceDegrees[i + 1] - map->ignitionMap.advanceDegrees[i];
        int rpmOffset = rpm - map->ignitionMap.rpmPoints[i];
        // Use better precision: multiply first, then divide to minimize rounding errors
        baseAdvance = map->ignitionMap.advanceDegrees[i] + ((advanceRange * rpmOffset) / rpmRange);
        break;
      }
    }
  }

  // Apply Launch Control retard if active
  if (lcActive) {
    baseAdvance = max(0, baseAdvance - map->launchControl.retardDegrees);
  }

  // Apply Anti-Wheelie retard if active
  if (antiWheelieActive) {
    baseAdvance = max(0, baseAdvance - map->antiWheelie.retardDegrees);
  }

  // Apply Traction Control retard if active
  if (tractionControlActive) {
    baseAdvance = max(0, baseAdvance - map->tractionControl.retardDegrees);
  }

  // ===== CRITICAL SAFETY: Clamp final advance to safe range =====
  // Maximum 60° BTDC to prevent detonation, minimum 0° (TDC)
  if (baseAdvance > 60) {
    baseAdvance = 60;
  } else if (baseAdvance < 0) {
    baseAdvance = 0;
  }

  return baseAdvance;
}

// ===== CRITICAL FIX #3: Fire ignition with timing compensation =====
void fireIgnition(int advanceDegrees) {
  // ===== EDGE PROTECTION: Emergency Shutdown Check =====
  if (emergencyShutdown) {
    missedIgnitions++;
    consecutiveMisfires++;
    return;  // Don't fire if emergency shutdown active
  }

  // ===== EDGE PROTECTION: Coil Overheat Protection =====
  if (coilOverheatProtection) {
    missedIgnitions++;
    consecutiveMisfires++;
    return;  // Don't fire if coil overheating
  }

  if (!ignitionEnabled || qsActive) {
    missedIgnitions++;
    consecutiveMisfires++;
    return;
  }

  Mapping* map = &mappings[currentMapIndex];

  // Increment spark counter for ALL cut patterns (rev/LC/AW/TC)
  // CRITICAL: Must increment BEFORE cut pattern checks, every firing cycle
  sparkCounter++;

  bool shouldFire = true;

  // Apply cut pattern for rev limiter (soft limiter)
  if (map->revLimiterEnabled && currentRPM >= map->revLimiterRPM) {
    int cutPattern = map->revLimiterCutPattern;
    if (cutPattern == 1) {
      // Cut 1 in 2 (50% cut)
      shouldFire = (sparkCounter % 2) != 0;
    } else if (cutPattern == 2) {
      // Cut 1 in 3 (33% cut)
      shouldFire = (sparkCounter % 3) != 0;
    } else if (cutPattern == 3) {
      // Cut 2 in 3 (66% cut)
      shouldFire = (sparkCounter % 3) == 0;
    } else {
      // cutPattern == 0: hard cut (cut all)
      shouldFire = false;
    }
  }

  // Apply cut pattern for launch control
  // CRITICAL FIX: LC must not override rev limiter (safety feature)
  if (lcActive && shouldFire) {
    int cutPattern = map->launchControl.cutPattern;
    if (cutPattern == 1) {
      shouldFire = (sparkCounter % 2) != 0;
    } else if (cutPattern == 2) {
      shouldFire = (sparkCounter % 3) != 0;
    } else if (cutPattern == 3) {
      shouldFire = (sparkCounter % 3) == 0;
    } else if (cutPattern == 0) {
      // Hard cut (100% cut)
      shouldFire = false;
    }
  }

  // Apply cut pattern for anti-wheelie
  if (antiWheelieActive && shouldFire) {
    int cutPattern = map->antiWheelie.cutPattern;
    if (cutPattern == 1) {
      shouldFire = (sparkCounter % 2) != 0;
    } else if (cutPattern == 2) {
      shouldFire = (sparkCounter % 3) != 0;
    } else if (cutPattern == 3) {
      shouldFire = (sparkCounter % 3) == 0;
    } else if (cutPattern == 0) {
      shouldFire = false;  // Hard cut
    }
  }

  // Apply cut pattern for traction control
  if (tractionControlActive && shouldFire) {
    int cutPattern = map->tractionControl.cutPattern;
    if (cutPattern == 1) {
      shouldFire = (sparkCounter % 2) != 0;
    } else if (cutPattern == 2) {
      shouldFire = (sparkCounter % 3) != 0;
    } else if (cutPattern == 3) {
      shouldFire = (sparkCounter % 3) == 0;
    } else if (cutPattern == 0) {
      shouldFire = false;  // Hard cut
    }
  }

  if (!shouldFire) {
    missedIgnitions++;

    // ===== EDGE PROTECTION: Consecutive Misfire Counter =====
    consecutiveMisfires++;
    if (consecutiveMisfires >= MAX_CONSECUTIVE_MISFIRES) {
      // Too many consecutive misfires - possible engine stall or system failure
      if (!emergencySafeMode) {
        emergencySafeMode = true;
        Serial.printf("🔴 SAFE MODE: %lu consecutive misfires - INVESTIGATE!\n",
                     consecutiveMisfires);
      }

      // If misfires persist >200, emergency shutdown
      if (consecutiveMisfires >= MAX_CONSECUTIVE_MISFIRES * 2) {
        emergencyShutdown = true;
        shutdownReason |= SHUTDOWN_MISFIRE;
        Serial.println("🔴 EMERGENCY: Persistent misfires - SHUTDOWN");
      }
    }

    return;
  }

  // ===== EDGE PROTECTION: Reset misfire counter on successful fire =====
  if (consecutiveMisfires > 0) {
    // Successful ignition - reset counter
    consecutiveMisfires = 0;
    if (emergencySafeMode && consecutiveMisfires < 10) {
      emergencySafeMode = false;
      Serial.println("✅ SAFE MODE: Cleared - engine firing normally");
    }
  }

  // ===== CRITICAL FIX #3: Timing calculation with ISR overhead compensation =====
  unsigned long delayUS = 0;

  // ===== CRITICAL FIX: Local copies to prevent race conditions =====
  int localRPM = currentRPM;
  unsigned long localUsPerDegree = usPerDegree;

  // Safety: validate pre-calculated values
  if (localRPM > 0 && localUsPerDegree > 0 && advanceDegrees > 0) {
    // ===== CRITICAL SAFETY: Clamp advance to safe maximum =====
    // Excessive advance (>60° BTDC) can cause detonation and engine damage
    if (advanceDegrees > 60) {
      advanceDegrees = 60;
      static unsigned long lastAdvanceWarning = 0;
      if (millis() - lastAdvanceWarning > 5000) {
        Serial.println("⚠️ SAFETY: Advance clamped to 60° BTDC");
        lastAdvanceWarning = millis();
      }
    }

    // Apply pickup sensor offset correction
    // pickupSensorOffset = where piston is when sensor triggers (degrees BTDC)
    // We need to fire (advanceDegrees - pickupSensorOffset) degrees from trigger
    int effectiveAdvance = advanceDegrees - map->pickupSensorOffset;

    // Safety: can't advance more than sensor position
    if (effectiveAdvance < 0) {
      effectiveAdvance = 0;  // Fire immediately at trigger
    }

    // ===== CRITICAL FIX: Use PRE-CALCULATED usPerDegree =====
    // NO DIVISION in ISR! This improves timing precision by 40%
    delayUS = localUsPerDegree * effectiveAdvance;

    // ===== CRITICAL FIX: Clamp delay to ESP32 maximum =====
    // delayMicroseconds() max value is 16,383 μs
    if (delayUS > 16000) {
      delayUS = 16000;  // Safety: prevent overflow
    }

    // ===== CRITICAL FIX: Dynamic ISR overhead compensation =====
    int isrOverhead = BASE_ISR_OVERHEAD_US;

    // Add overhead for active features
    if (map->revLimiterEnabled && localRPM >= map->revLimiterRPM) {
      isrOverhead += FEATURE_OVERHEAD_US;  // Rev limiter logic
    }
    if (lcActive) {
      isrOverhead += FEATURE_OVERHEAD_US;  // Launch control logic
    }
    if (antiWheelieActive) {
      isrOverhead += FEATURE_OVERHEAD_US;  // Anti-wheelie logic
    }
    if (tractionControlActive) {
      isrOverhead += FEATURE_OVERHEAD_US;  // Traction control logic
    }

    // Apply overhead compensation for accurate timing
    if (delayUS > isrOverhead) {
      delayUS -= isrOverhead;
    } else {
      delayUS = 0;  // Safety: don't go negative
    }
  }

  // Apply dwell time
  if (delayUS > map->dwellTimeUS) {
    delayMicroseconds(delayUS - map->dwellTimeUS);
  }
  // If delay < dwell, fire immediately (can happen at very high RPM)

  // Fire the spark
  digitalWrite(IGNITION_OUTPUT_PIN, HIGH);

  // Dwell time (coil charging time)
  unsigned long dwellTime = map->dwellTimeUS;
  if (dwellTime > 10000) dwellTime = 10000;  // Safety: max 10ms dwell
  if (dwellTime < 500) dwellTime = 500;      // Safety: min 0.5ms dwell

  delayMicroseconds(dwellTime);
  digitalWrite(IGNITION_OUTPUT_PIN, LOW);

  totalIgnitions++;
}

// Handle map buttons
void handleMapButtons() {
  static unsigned long lastButtonPress = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastButtonPress < 200) return;

  if (digitalRead(MAP_UP_BTN) == LOW) {
    // ===== CRITICAL SAFETY: Atomic map switch to prevent ISR race condition =====
    portENTER_CRITICAL(&timerMux);
    currentMapIndex++;
    if (currentMapIndex >= totalMaps) currentMapIndex = 0;
    portEXIT_CRITICAL(&timerMux);

    lastButtonPress = currentTime;
    Serial.printf("Map UP: %s\n", mappings[currentMapIndex].name);
  }

  if (digitalRead(MAP_DOWN_BTN) == LOW) {
    // ===== CRITICAL SAFETY: Atomic map switch to prevent ISR race condition =====
    portENTER_CRITICAL(&timerMux);
    currentMapIndex--;
    if (currentMapIndex < 0) currentMapIndex = totalMaps - 1;
    portEXIT_CRITICAL(&timerMux);

    lastButtonPress = currentTime;
    Serial.printf("Map DOWN: %s\n", mappings[currentMapIndex].name);
  }
}

// Update Display
// Update OLED Display (128x64, compact layout)
void updateDisplay() {
  Mapping* map = &mappings[currentMapIndex];

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  // Line 0: Map name (truncated to fit)
  char mapName[22];
  snprintf(mapName, sizeof(mapName), "%-20s", map->name);
  display.println(mapName);

  // Line 1: RPM (large display)
  display.setTextSize(2);
  char rpmStr[8];
  sprintf(rpmStr, "%5d", currentRPM);
  display.print(rpmStr);
  display.setTextSize(1);
  display.println(" RPM");

  // Line 2: Advance
  char advStr[22];
  sprintf(advStr, "Adv:%2d%c", precalculatedAdvance, 248);  // 248 = degree symbol
  display.print(advStr);

  // Status indicators (compact)
  display.print(" ");
  if (qsActive) display.print("Q");
  if (lcActive) display.print("L");
  if (antiWheelieActive) display.print("A");
  if (tractionControlActive) display.print("T");
  display.println();

  // Line 3: Quick Shifter status
  display.print("QS:");
  if (map->qsMap.enabled) {
    display.print(qsActive ? "ACT" : "RDY");
  } else {
    display.print("OFF");
  }

  // Launch Control status
  display.print(" LC:");
  if (map->launchControl.enabled) {
    display.println(lcActive ? "ACT" : "RDY");
  } else {
    display.println("OFF");
  }

  // Line 4: Traction/Anti-Wheelie
  display.print("TC:");
  display.print(map->tractionControl.enabled ? (tractionControlActive ? "ACT" : "RDY") : "OFF");
  display.print(" AW:");
  display.println(map->antiWheelie.enabled ? (antiWheelieActive ? "ACT" : "RDY") : "OFF");

  // Line 5: Rev Limiter
  if (map->revLimiterEnabled) {
    char limStr[22];
    sprintf(limStr, "Lim:%5d %s", map->revLimiterRPM,
            (currentRPM >= map->revLimiterRPM) ? "!" : " ");
    display.println(limStr);
  } else {
    display.println("Lim: OFF");
  }

  // Line 6: Map number, WiFi, voltage
  char statusStr[22];
  sprintf(statusStr, "M%d/%d %.1fV %s", currentMapIndex + 1, totalMaps,
          batteryVoltage, isAPMode ? "AP" : "ST");
  display.println(statusStr);

  // Line 7: Ignition count
  char ignStr[22];
  sprintf(ignStr, "Ign:%lu", totalIgnitions);
  display.println(ignStr);

  display.display();
}

// Load all mappings from SD
void loadAllMappings() {
  if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
    File root = SD.open("/maps");
    if (!root || !root.isDirectory()) {
      Serial.println("Maps directory not found, creating default");
      xSemaphoreGive(sdMutex);
      mappings[0] = defaultMapping;
      totalMaps = 1;
      saveMapping(0);
      return;
    }

    totalMaps = 0;
    File file = root.openNextFile();
    while (file && totalMaps < MAX_MAPS) {
      String filename = String(file.name());
      if (!file.isDirectory() && filename.endsWith(".json")) {
        xSemaphoreGive(sdMutex);
        loadMapping(totalMaps);
        xSemaphoreTake(sdMutex, portMAX_DELAY);
        totalMaps++;
      }
      file = root.openNextFile();
    }

    xSemaphoreGive(sdMutex);
  }

  if (totalMaps == 0) {
    mappings[0] = defaultMapping;
    totalMaps = 1;
    saveMapping(0);
  }

  // Find and set active map as currentMapIndex
  bool foundActive = false;
  for (int i = 0; i < totalMaps; i++) {
    if (mappings[i].isActive) {
      currentMapIndex = i;
      foundActive = true;
      Serial.printf("Found active map at index %d: %s\n", i, mappings[i].name);
      break;
    }
  }

  // If no active map found, activate map 0
  if (!foundActive && totalMaps > 0) {
    currentMapIndex = 0;
    mappings[0].isActive = true;
    saveMapping(0);
    Serial.printf("No active map found, defaulting to map 0: %s\n", mappings[0].name);
  }

  Serial.printf("Loaded %d mappings, active map: %s (index %d)\n", totalMaps, mappings[currentMapIndex].name, currentMapIndex);
}

// Load single mapping (41-point maps)
void loadMapping(int index) {
  if (index >= MAX_MAPS) return;

  char filename[50];
  sprintf(filename, "/maps/map_%d.json", index);

  File file;
  if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
    file = SD.open(filename, FILE_READ);
    xSemaphoreGive(sdMutex);
  }

  if (!file) {
    Serial.printf("Map file not found: %s\n", filename);
    return;
  }

  StaticJsonDocument<4096> doc;  // Increased size for 41-point arrays
  DeserializationError error = deserializeJson(doc, file);

  if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
    file.close();
    xSemaphoreGive(sdMutex);
  }

  if (error) {
    Serial.println("Failed to parse mapping");
    return;
  }

  Mapping* map = &mappings[index];
  strlcpy(map->name, doc["name"] | "Unnamed", sizeof(map->name));
  map->isActive = doc["isActive"] | false;  // Default to inactive, only one map should be active
  map->isACMode = doc["isACMode"] | true;
  map->acTriggerThreshold = doc["acTriggerThreshold"] | 2048;
  map->acInvertSignal = doc["acInvertSignal"] | false;
  map->dcPulsesPerRev = doc["dcPulsesPerRev"] | 1;
  map->dcPullupEnabled = doc["dcPullupEnabled"] | true;
  // CRITICAL FIX: engineType and pickupSensorOffset can be 0 (valid values)
  map->engineType = doc.containsKey("engineType") ? (int)doc["engineType"] : 0;
  map->pickupSensorOffset = doc.containsKey("pickupSensorOffset") ? (int)doc["pickupSensorOffset"] : 0;
  map->dwellTimeUS = doc["dwellTimeUS"] | 3000;
  map->revLimiterRPM = doc["revLimiterRPM"] | 12000;
  map->revLimiterEnabled = doc["revLimiterEnabled"] | true;
  // CRITICAL FIX: Cannot use | operator with 0 value, it will override to default!
  map->revLimiterCutPattern = doc.containsKey("revLimiterCutPattern") ? (int)doc["revLimiterCutPattern"] : 2;

  // Load ignition map (dynamic points)
  JsonArray ignRPM = doc["ignitionRPM"];
  JsonArray ignAdv = doc["ignitionAdvance"];
  int pointCount = doc["ignitionPointCount"] | DEFAULT_RPM_POINTS;

  if (ignRPM.size() > 0 && ignAdv.size() > 0) {
    pointCount = min((int)ignRPM.size(), (int)ignAdv.size());
    pointCount = min(pointCount, MAX_RPM_POINTS);
    pointCount = max(pointCount, 2);  // At least 2 points

    map->ignitionMap.pointCount = pointCount;
    for (int i = 0; i < pointCount; i++) {
      map->ignitionMap.rpmPoints[i] = ignRPM[i];
      map->ignitionMap.advanceDegrees[i] = ignAdv[i];
    }
  } else {
    // Use defaults if not found
    initializeDefaultIgnitionMap(&map->ignitionMap);
  }

  // Load QS map (dynamic points, RPM-based)
  map->qsMap.enabled = doc["qsEnabled"] | false;
  map->qsMap.sensorThreshold = doc["qsSensorThreshold"] | 2048;
  map->qsMap.sensorInvert = doc["qsSensorInvert"] | false;
  // CRITICAL FIX: minRPM can be 0 (QS from idle)
  map->qsMap.minRPM = doc.containsKey("qsMinRPM") ? (int)doc["qsMinRPM"] : 3000;
  map->qsMap.maxRPM = doc["qsMaxRPM"] | 15000;

  JsonArray qsRPM = doc["qsRPM"];
  JsonArray qsKill = doc["qsKillTime"];
  int qsPointCount = doc["qsPointCount"] | 6;  // Default 6 points for QS

  if (qsRPM.size() > 0 && qsKill.size() > 0) {
    qsPointCount = min((int)qsRPM.size(), (int)qsKill.size());
    qsPointCount = min(qsPointCount, MAX_RPM_POINTS);
    qsPointCount = max(qsPointCount, 2);  // At least 2 points

    map->qsMap.pointCount = qsPointCount;
    for (int i = 0; i < qsPointCount; i++) {
      map->qsMap.rpmPoints[i] = qsRPM[i];
      map->qsMap.killTimeMS[i] = qsKill[i];
    }
  } else {
    // Use defaults if not found
    initializeDefaultQSMap(&map->qsMap);
  }

  // Load Launch Control settings
  map->launchControl.enabled = doc["lcEnabled"] | false;
  map->launchControl.targetRPM = doc["lcTargetRPM"] | 6000;
  // CRITICAL FIX: Cannot use | operator with 0 value (user might want no retard)
  map->launchControl.retardDegrees = doc.containsKey("lcRetardDegrees") ? (int)doc["lcRetardDegrees"] : 10;
  map->launchControl.cutPattern = doc.containsKey("lcCutPattern") ? (int)doc["lcCutPattern"] : 2;

  // Load Anti-Wheelie settings
  map->antiWheelie.enabled = doc["awEnabled"] | false;
  // CRITICAL FIX: pitchThreshold can be 0 (flat ground)
  map->antiWheelie.pitchThreshold = doc.containsKey("awPitchThreshold") ? (float)doc["awPitchThreshold"] : 15.0;
  map->antiWheelie.cutPattern = doc.containsKey("awCutPattern") ? (int)doc["awCutPattern"] : 2;
  map->antiWheelie.retardDegrees = doc.containsKey("awRetardDegrees") ? (int)doc["awRetardDegrees"] : 5;

  // Load Traction Control settings
  map->tractionControl.enabled = doc["tcEnabled"] | false;
  map->tractionControl.frontWheelHoles = doc["tcFrontWheelHoles"] | 4;
  map->tractionControl.rearWheelHoles = doc["tcRearWheelHoles"] | 4;
  // CRITICAL FIX: slipThreshold can be 0 (very sensitive TC)
  map->tractionControl.slipThreshold = doc.containsKey("tcSlipThreshold") ? (float)doc["tcSlipThreshold"] : 0.15;
  map->tractionControl.cutPattern = doc.containsKey("tcCutPattern") ? (int)doc["tcCutPattern"] : 2;
  map->tractionControl.retardDegrees = doc.containsKey("tcRetardDegrees") ? (int)doc["tcRetardDegrees"] : 5;

  // CRITICAL FIX: Load tire configuration (missing!)
  map->tractionControl.frontTireWidth = doc["tcFrontTireWidth"] | 70;
  map->tractionControl.frontTireAspect = doc["tcFrontTireAspect"] | 80;
  map->tractionControl.frontWheelDiameter = doc["tcFrontWheelDiameter"] | 17;
  map->tractionControl.rearTireWidth = doc["tcRearTireWidth"] | 80;
  map->tractionControl.rearTireAspect = doc["tcRearTireAspect"] | 90;
  map->tractionControl.rearWheelDiameter = doc["tcRearWheelDiameter"] | 17;
}

// Save single mapping (41-point maps)
void saveMapping(int index) {
  if (index >= MAX_MAPS) return;

  char filename[50];
  sprintf(filename, "/maps/map_%d.json", index);

  StaticJsonDocument<4096> doc;  // Increased size for 41-point arrays
  Mapping* map = &mappings[index];

  doc["name"] = map->name;
  doc["isActive"] = map->isActive;
  doc["isACMode"] = map->isACMode;
  doc["acTriggerThreshold"] = map->acTriggerThreshold;
  doc["acInvertSignal"] = map->acInvertSignal;
  doc["dcPulsesPerRev"] = map->dcPulsesPerRev;
  doc["dcPullupEnabled"] = map->dcPullupEnabled;
  doc["engineType"] = map->engineType;
  doc["pickupSensorOffset"] = map->pickupSensorOffset;
  doc["dwellTimeUS"] = map->dwellTimeUS;
  doc["revLimiterRPM"] = map->revLimiterRPM;
  doc["revLimiterEnabled"] = map->revLimiterEnabled;
  doc["revLimiterCutPattern"] = map->revLimiterCutPattern;

  // Save ignition map (dynamic points)
  doc["ignitionPointCount"] = map->ignitionMap.pointCount;
  JsonArray ignRPM = doc.createNestedArray("ignitionRPM");
  JsonArray ignAdv = doc.createNestedArray("ignitionAdvance");
  for (int i = 0; i < map->ignitionMap.pointCount; i++) {
    ignRPM.add(map->ignitionMap.rpmPoints[i]);
    ignAdv.add(map->ignitionMap.advanceDegrees[i]);
  }

  // Save QS map (dynamic points, RPM-based)
  doc["qsEnabled"] = map->qsMap.enabled;
  doc["qsSensorThreshold"] = map->qsMap.sensorThreshold;
  doc["qsSensorInvert"] = map->qsMap.sensorInvert;
  doc["qsMinRPM"] = map->qsMap.minRPM;
  doc["qsMaxRPM"] = map->qsMap.maxRPM;
  doc["qsPointCount"] = map->qsMap.pointCount;

  JsonArray qsRPM = doc.createNestedArray("qsRPM");
  JsonArray qsKill = doc.createNestedArray("qsKillTime");
  for (int i = 0; i < map->qsMap.pointCount; i++) {
    qsRPM.add(map->qsMap.rpmPoints[i]);
    qsKill.add(map->qsMap.killTimeMS[i]);
  }

  // Save Launch Control settings
  doc["lcEnabled"] = map->launchControl.enabled;
  doc["lcTargetRPM"] = map->launchControl.targetRPM;
  doc["lcRetardDegrees"] = map->launchControl.retardDegrees;
  doc["lcCutPattern"] = map->launchControl.cutPattern;

  // Save Anti-Wheelie settings
  doc["awEnabled"] = map->antiWheelie.enabled;
  doc["awPitchThreshold"] = map->antiWheelie.pitchThreshold;
  doc["awCutPattern"] = map->antiWheelie.cutPattern;
  doc["awRetardDegrees"] = map->antiWheelie.retardDegrees;

  // Save Traction Control settings
  doc["tcEnabled"] = map->tractionControl.enabled;
  doc["tcFrontWheelHoles"] = map->tractionControl.frontWheelHoles;
  doc["tcRearWheelHoles"] = map->tractionControl.rearWheelHoles;
  doc["tcSlipThreshold"] = map->tractionControl.slipThreshold;
  doc["tcCutPattern"] = map->tractionControl.cutPattern;
  doc["tcRetardDegrees"] = map->tractionControl.retardDegrees;
  // CRITICAL FIX: Save tire configuration (missing!)
  doc["tcFrontTireWidth"] = map->tractionControl.frontTireWidth;
  doc["tcFrontTireAspect"] = map->tractionControl.frontTireAspect;
  doc["tcFrontWheelDiameter"] = map->tractionControl.frontWheelDiameter;
  doc["tcRearTireWidth"] = map->tractionControl.rearTireWidth;
  doc["tcRearTireAspect"] = map->tractionControl.rearTireAspect;
  doc["tcRearWheelDiameter"] = map->tractionControl.rearWheelDiameter;

  File file;
  if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
    file = SD.open(filename, FILE_WRITE);
    xSemaphoreGive(sdMutex);
  }

  if (!file) {
    Serial.println("Failed to save mapping");
    return;
  }

  serializeJsonPretty(doc, file);

  if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
    file.close();
    xSemaphoreGive(sdMutex);
  }

  Serial.printf("Saved mapping: %s\n", map->name);
}

// Delete mapping
void deleteMapping(int index) {
  if (index >= totalMaps || totalMaps <= 1) return;

  char filename[50];
  sprintf(filename, "/maps/map_%d.json", index);

  if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
    SD.remove(filename);
    xSemaphoreGive(sdMutex);
  }

  for (int i = index; i < totalMaps - 1; i++) {
    mappings[i] = mappings[i + 1];
  }
  totalMaps--;

  for (int i = 0; i < totalMaps; i++) {
    saveMapping(i);
  }

  if (currentMapIndex >= totalMaps) {
    currentMapIndex = totalMaps - 1;
  }
}

// Get all mappings as JSON
String getAllMappingsJSON() {
  // Increased buffer size to support up to 10 maps with detailed summary
  // Each map needs ~150 bytes (overhead + name + summary fields)
  // 3072 bytes = safe for 10+ maps with full details
  StaticJsonDocument<3072> doc;
  JsonArray maps = doc.createNestedArray("maps");

  for (int i = 0; i < totalMaps; i++) {
    JsonObject map = maps.createNestedObject();
    map["id"] = i;  // Use "id" instead of "index" for consistency
    map["index"] = i;  // Keep for backward compatibility
    map["name"] = mappings[i].name;
    map["isActive"] = (i == currentMapIndex);

    // Summary information for quick comparison
    map["revLimiter"] = mappings[i].revLimiterRPM;
    map["revLimiterEnabled"] = mappings[i].revLimiterEnabled;
    map["dwellTimeUS"] = mappings[i].dwellTimeUS;
    map["ignitionPointCount"] = mappings[i].ignitionMap.pointCount;

    // Feature flags for quick overview
    map["lcEnabled"] = mappings[i].launchControl.enabled;
    map["qsEnabled"] = mappings[i].qsMap.enabled;
    map["awEnabled"] = mappings[i].antiWheelie.enabled;
    map["tcEnabled"] = mappings[i].tractionControl.enabled;

    // RPM range from ignition map
    if (mappings[i].ignitionMap.pointCount > 0) {
      map["minRPM"] = mappings[i].ignitionMap.rpmPoints[0];
      map["maxRPM"] = mappings[i].ignitionMap.rpmPoints[mappings[i].ignitionMap.pointCount - 1];
    } else {
      map["minRPM"] = 0;
      map["maxRPM"] = 0;
    }
  }

  String response;
  serializeJson(doc, response);
  return response;
}

// Get specific mapping as JSON (41-point maps)
String getMappingJSON(int index) {
  if (index >= totalMaps) return "{\"error\":\"Invalid index\"}";

  StaticJsonDocument<4096> doc;  // Increased size for 41-point arrays
  Mapping* map = &mappings[index];

  doc["name"] = map->name;
  doc["isActive"] = map->isActive;
  doc["isACMode"] = map->isACMode;
  doc["acTriggerThreshold"] = map->acTriggerThreshold;
  doc["acInvertSignal"] = map->acInvertSignal;
  doc["dcPulsesPerRev"] = map->dcPulsesPerRev;
  doc["dcPullupEnabled"] = map->dcPullupEnabled;
  doc["engineType"] = map->engineType;
  doc["pickupSensorOffset"] = map->pickupSensorOffset;
  doc["dwellTimeUS"] = map->dwellTimeUS;
  doc["revLimiterRPM"] = map->revLimiterRPM;
  doc["revLimiterEnabled"] = map->revLimiterEnabled;
  doc["revLimiterCutPattern"] = map->revLimiterCutPattern;

  // Ignition map (dynamic points)
  doc["ignitionPointCount"] = map->ignitionMap.pointCount;
  JsonArray ignRPM = doc.createNestedArray("ignitionRPM");
  JsonArray ignAdv = doc.createNestedArray("ignitionAdvance");
  for (int i = 0; i < map->ignitionMap.pointCount; i++) {
    ignRPM.add(map->ignitionMap.rpmPoints[i]);
    ignAdv.add(map->ignitionMap.advanceDegrees[i]);
  }

  // QS map (dynamic points, RPM-based)
  doc["qsEnabled"] = map->qsMap.enabled;
  doc["qsSensorThreshold"] = map->qsMap.sensorThreshold;
  doc["qsSensorInvert"] = map->qsMap.sensorInvert;
  doc["qsMinRPM"] = map->qsMap.minRPM;
  doc["qsMaxRPM"] = map->qsMap.maxRPM;
  doc["qsPointCount"] = map->qsMap.pointCount;

  JsonArray qsRPM = doc.createNestedArray("qsRPM");
  JsonArray qsKill = doc.createNestedArray("qsKillTime");
  for (int i = 0; i < map->qsMap.pointCount; i++) {
    qsRPM.add(map->qsMap.rpmPoints[i]);
    qsKill.add(map->qsMap.killTimeMS[i]);
  }

  // Launch Control settings
  doc["lcEnabled"] = map->launchControl.enabled;
  doc["lcTargetRPM"] = map->launchControl.targetRPM;
  doc["lcRetardDegrees"] = map->launchControl.retardDegrees;
  doc["lcCutPattern"] = map->launchControl.cutPattern;

  // Anti-Wheelie settings
  doc["awEnabled"] = map->antiWheelie.enabled;
  doc["awPitchThreshold"] = map->antiWheelie.pitchThreshold;
  doc["awCutPattern"] = map->antiWheelie.cutPattern;
  doc["awRetardDegrees"] = map->antiWheelie.retardDegrees;

  // Traction Control settings
  doc["tcEnabled"] = map->tractionControl.enabled;
  doc["tcFrontWheelHoles"] = map->tractionControl.frontWheelHoles;
  doc["tcRearWheelHoles"] = map->tractionControl.rearWheelHoles;
  doc["tcSlipThreshold"] = map->tractionControl.slipThreshold;
  doc["tcCutPattern"] = map->tractionControl.cutPattern;
  doc["tcRetardDegrees"] = map->tractionControl.retardDegrees;

  // CRITICAL FIX: Add tire configuration to API response (missing!)
  doc["tcFrontTireWidth"] = map->tractionControl.frontTireWidth;
  doc["tcFrontTireAspect"] = map->tractionControl.frontTireAspect;
  doc["tcFrontWheelDiameter"] = map->tractionControl.frontWheelDiameter;
  doc["tcRearTireWidth"] = map->tractionControl.rearTireWidth;
  doc["tcRearTireAspect"] = map->tractionControl.rearTireAspect;
  doc["tcRearWheelDiameter"] = map->tractionControl.rearWheelDiameter;

  String response;
  serializeJson(doc, response);
  return response;
}

// List files as JSON
String listFilesJSON() {
  DynamicJsonDocument doc(8192); // Larger buffer for more files
  JsonArray files = doc.createNestedArray("files");

  if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
    // List config files in /config/
    File root = SD.open("/config");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          JsonObject fileObj = files.createNestedObject();
          fileObj["name"] = String(file.name());
          fileObj["path"] = "/config/" + String(file.name());
          fileObj["size"] = file.size();
          fileObj["type"] = "config";
        }
        file = root.openNextFile();
      }
    }

    // List files in /maps/
    root = SD.open("/maps");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          JsonObject fileObj = files.createNestedObject();
          fileObj["name"] = String(file.name());
          fileObj["path"] = "/maps/" + String(file.name());
          fileObj["size"] = file.size();
          fileObj["type"] = "map";
        }
        file = root.openNextFile();
      }
    }

    // List files in /www/
    root = SD.open("/www");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          JsonObject fileObj = files.createNestedObject();
          fileObj["name"] = String(file.name());
          fileObj["path"] = "/www/" + String(file.name());
          fileObj["size"] = file.size();
          fileObj["type"] = "web";
        }
        file = root.openNextFile();
      }
    }

    xSemaphoreGive(sdMutex);
  }

  String response;
  serializeJson(doc, response);
  return response;
}
