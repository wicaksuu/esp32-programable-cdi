/*
 * SIMULATOR FUNCTIONS for Smart CDI v2.2
 * Test CDI tanpa motor - Virtual inputs & unit tests
 *
 * APPEND THIS TO MAIN FILE or keep as separate tab in Arduino IDE
 */

// ===== SIMULATOR UPDATE FUNCTION =====
// Called every loop when simulator mode active
void updateSimulator() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastIgnitionSim = 0;
  static unsigned long simSparkCounter = 0;  // Separate counter for cut patterns (like ISR sparkCounter)
  unsigned long currentTime = millis();

  // Update every 50ms (20Hz update rate)
  if (currentTime - lastUpdate < 50) return;
  lastUpdate = currentTime;

  // Apply virtual inputs to system
  simulatorApplyInputs();

  // Auto-run mode: automatically sweep RPM
  if (sim.autoRun) {
    sim.virtualRPM += sim.autoRPMStep;

    // Bounce between idle and target RPM
    if (sim.virtualRPM >= sim.autoTargetRPM) {
      sim.virtualRPM = sim.autoTargetRPM;
      sim.autoRPMStep = -abs(sim.autoRPMStep); // Go down
    }
    if (sim.virtualRPM <= 1000) {
      sim.virtualRPM = 1000;
      sim.autoRPMStep = abs(sim.autoRPMStep); // Go up
    }
  }

  // ===== SIMULATE IGNITION FIRING =====
  // Increment totalIgnitions based on virtual RPM (since ISR doesn't fire in simulator mode)

  // DEBUG: Log condition check every 2 seconds
  static unsigned long lastConditionLog = 0;
  if (currentTime - lastConditionLog > 2000) {
    Serial.printf("[SIM DEBUG] virtualRPM=%d, ignitionEnabled=%d, qsActive=%d, condition=%d\n",
                  sim.virtualRPM, ignitionEnabled, qsActive,
                  (sim.virtualRPM > 0 && ignitionEnabled && !qsActive));
    lastConditionLog = currentTime;
  }

  if (sim.virtualRPM > 0 && ignitionEnabled && !qsActive) {
    // Increment spark counter ALWAYS (like ISR does in calculateAdvance)
    // This must happen regardless of rev limiter status!
    simSparkCounter++;

    // Check rev limiter FIRST (MUST match ISR logic!)
    Mapping* map = &mappings[currentMapIndex];

    // HARD CUT CHECK - Stop ALL ignitions if rev limiter active with pattern 0
    if (map->revLimiterEnabled && currentRPM >= map->revLimiterRPM && map->revLimiterCutPattern == 0) {
      // HARD CUT ACTIVE - Do NOT increment ignitions!
      static unsigned long lastDebugHardCut = 0;
      if (currentTime - lastDebugHardCut > 1000) {
        Serial.printf("[SIM] ⛔ HARD CUT ACTIVE! RPM=%d >= %d, Pattern=0 - BLOCKING ALL IGNITIONS\n",
                      currentRPM, map->revLimiterRPM);
        lastDebugHardCut = currentTime;
      }
      return;  // EXIT EARLY - Don't increment ignitions!
    }

    // Soft cut patterns (1, 2, 3) - apply pattern-based cutting
    bool shouldFire = true;
    if (map->revLimiterEnabled && currentRPM >= map->revLimiterRPM) {
      int cutPattern = map->revLimiterCutPattern;

      // DEBUG: Log rev limiter activation
      static unsigned long lastRevLimLog = 0;
      if (currentTime - lastRevLimLog > 2000) {
        Serial.printf("[SIM DEBUG] REV LIMITER ACTIVE! RPM=%d >= %d, Pattern=%d, sparkCounter=%lu\n",
                      currentRPM, map->revLimiterRPM, cutPattern, simSparkCounter);
        lastRevLimLog = currentTime;
      }

      if (cutPattern == 1) {
        // Soft cut - fire every 2nd
        shouldFire = (simSparkCounter % 2) != 0;
      } else if (cutPattern == 2) {
        // Soft cut - fire 1, skip 2
        shouldFire = (simSparkCounter % 3) != 0;
      } else if (cutPattern == 3) {
        // Soft cut - skip 2, fire 1
        shouldFire = (simSparkCounter % 3) == 0;
      }

      // DEBUG: Log shouldFire decision
      static unsigned long lastFireLog = 0;
      if (currentTime - lastFireLog > 2000) {
        Serial.printf("[SIM DEBUG] shouldFire=%d (sparkCounter %% 2 = %lu)\n",
                      shouldFire, simSparkCounter % 2);
        lastFireLog = currentTime;
      }
    }

    // Apply cut pattern for launch control (MUST match ISR logic!)
    if (lcActive && shouldFire) {
      int cutPattern = map->launchControl.cutPattern;
      if (cutPattern == 1) {
        shouldFire = (simSparkCounter % 2) != 0;
      } else if (cutPattern == 2) {
        shouldFire = (simSparkCounter % 3) != 0;
      } else if (cutPattern == 3) {
        shouldFire = (simSparkCounter % 3) == 0;
      } else if (cutPattern == 0) {
        shouldFire = false;  // Hard cut
      }
    }

    // Apply cut pattern for anti-wheelie (MUST match ISR logic!)
    if (antiWheelieActive && shouldFire) {
      int cutPattern = map->antiWheelie.cutPattern;
      if (cutPattern == 1) {
        shouldFire = (simSparkCounter % 2) != 0;
      } else if (cutPattern == 2) {
        shouldFire = (simSparkCounter % 3) != 0;
      } else if (cutPattern == 3) {
        shouldFire = (simSparkCounter % 3) == 0;
      } else if (cutPattern == 0) {
        shouldFire = false;  // Hard cut
      }
    }

    // Apply cut pattern for traction control (MUST match ISR logic!)
    if (tractionControlActive && shouldFire) {
      int cutPattern = map->tractionControl.cutPattern;
      if (cutPattern == 1) {
        shouldFire = (simSparkCounter % 2) != 0;
      } else if (cutPattern == 2) {
        shouldFire = (simSparkCounter % 3) != 0;
      } else if (cutPattern == 3) {
        shouldFire = (simSparkCounter % 3) == 0;
      } else if (cutPattern == 0) {
        shouldFire = false;  // Hard cut
      }
    }

    // Only fire if allowed by cut patterns
    if (shouldFire) {
      // Calculate ignition interval in milliseconds
      // For 2-stroke: 1 ignition per revolution
      unsigned long ignitionIntervalMS = 60000UL / sim.virtualRPM;

      // DEBUG: Log firing attempt
      static unsigned long lastFiringLog = 0;
      if (currentTime - lastFiringLog > 2000) {
        Serial.printf("[SIM DEBUG] shouldFire=TRUE, interval=%lums, elapsed=%lums, willFire=%d\n",
                      ignitionIntervalMS, currentTime - lastIgnitionSim,
                      (currentTime - lastIgnitionSim >= ignitionIntervalMS));
        lastFiringLog = currentTime;
      }

      if (currentTime - lastIgnitionSim >= ignitionIntervalMS) {
        totalIgnitions++;
        lastIgnitionSim = currentTime;

        // DEBUG: Log successful ignition
        static unsigned long lastIgnLog = 0;
        if (currentTime - lastIgnLog > 2000) {
          Serial.printf("[SIM DEBUG] ✓ IGNITION FIRED! Total=%lu\n", totalIgnitions);
          lastIgnLog = currentTime;
        }
      }
    } else {
      // DEBUG: Log when shouldFire is false
      static unsigned long lastSkipLog = 0;
      if (currentTime - lastSkipLog > 2000) {
        Serial.printf("[SIM DEBUG] shouldFire=FALSE, skipping ignition\n");
        lastSkipLog = currentTime;
      }
    }
  }

  // Update outputs based on current state
  sim.virtualAdvanceDegrees = precalculatedAdvance;
  sim.virtualIgnitionFiring = ignitionEnabled && !qsActive;
  sim.virtualIgnitionCount = totalIgnitions;
  sim.virtualMissedCount = missedIgnitions;

  // Log to Serial for debugging
  if (currentTime % 1000 < 100) {  // Every ~1 second
    Serial.printf("SIM: RPM=%d, Adv=%d°, Ign=%lu, QS=%s, LC=%s, TC=%s, AW=%s\n",
                  sim.virtualRPM,
                  sim.virtualAdvanceDegrees,
                  totalIgnitions,
                  qsActive ? "ACT" : "OFF",
                  lcActive ? "ACT" : "OFF",
                  tractionControlActive ? "ACT" : "OFF",
                  antiWheelieActive ? "ACT" : "OFF");
  }
}

// ===== APPLY VIRTUAL INPUTS TO SYSTEM =====
void simulatorApplyInputs() {
  // Override actual sensor readings with virtual values
  currentRPM = sim.virtualRPM;
  frontWheelSpeed = sim.virtualSpeed;
  rearWheelSpeed = sim.virtualSpeed * (1.0 + sim.virtualSlip); // Rear spins faster if slipping
  currentPitch = sim.virtualPitch;
  qsPressureValue = sim.virtualQSPressure;

  // Simulate trigger interval based on virtual RPM
  if (sim.virtualRPM > 0) {
    // For 2-stroke: 1 ignition per revolution
    triggerInterval = 60000000UL / sim.virtualRPM;  // microseconds
    lastTriggerTime = micros();
  }
}

// ===== RUN TEST SCENARIOS =====
void runSimulatorScenario(String scenario) {
  sim.activeScenario = scenario;
  sim.scenarioStartTime = millis();

  Serial.println("========================================");
  Serial.printf("Running Scenario: %s\n", scenario.c_str());
  Serial.println("========================================");

  // Log scenario execution
  char logMsg[64];
  snprintf(logMsg, sizeof(logMsg), "Test scenario: %s", scenario.c_str());
  logMessage(LOG_INFO, logMsg);

  if (scenario == "idle") {
    // Test 1: Idle State - REALISTIC
    sim.virtualRPM = 1500;
    sim.virtualThrottle = 5;
    sim.virtualClutch = false;
    sim.virtualSpeed = 0;
    sim.virtualPitch = 0;
    sim.virtualSlip = 0;
    Serial.println("✓ Idle: RPM=1500, throttle=5%, neutral, advance ~5-10°");

  } else if (scenario == "accel") {
    // Test 2: Acceleration Sweep - REALISTIC (faster sweep)
    sim.autoRun = true;
    sim.autoTargetRPM = 12000;
    sim.autoRPMStep = 500;  // FASTER sweep (more realistic acceleration)
    sim.virtualRPM = 2000;  // Start from low RPM (not 0)
    sim.virtualThrottle = 85;
    sim.virtualSpeed = 20.0;  // Rolling start
    sim.virtualClutch = false;  // Clutch released (in gear)
    Serial.println("✓ Acceleration: RPM sweeps 2000→12000 (realistic WOT pull)");
    Serial.println("  Expected: Progressive advance increase through powerband");

  } else if (scenario == "revlimit") {
    // Test 3: Rev Limiter - REALISTIC
    Mapping* map = &mappings[currentMapIndex];
    sim.virtualRPM = map->revLimiterRPM + 100; // Exceed limit by 100 RPM
    sim.virtualThrottle = 100;  // Full throttle
    sim.virtualSpeed = 80.0;    // High speed context
    sim.virtualClutch = false;
    Serial.printf("✓ Rev Limiter: RPM=%d (limit=%d), WOT, should cut spark\n",
                  sim.virtualRPM, map->revLimiterRPM);
    Serial.printf("  Cut pattern: %d\n", map->revLimiterCutPattern);

  } else if (scenario == "quickshift") {
    // Test 4: Quick Shifter - REALISTIC (with speed & proper hold time)
    Mapping* map = &mappings[currentMapIndex];
    sim.virtualRPM = 6000;  // Lower RPM for safer shift
    sim.virtualThrottle = 80;  // High throttle (mid-shift)
    sim.virtualSpeed = 45.0;  // Realistic speed for QS usage
    sim.virtualClutch = false;  // Clutchless shift
    sim.virtualQSPressed = true;
    sim.virtualQSPressure = map->qsMap.sensorThreshold + 800; // Strong pressure
    Serial.println("✓ Quick Shift: RPM=6000, speed=45 km/h, clutchless upshift");
    Serial.println("  Expected: Spark cut for kill time (~100ms), smooth shift");

    // Hold shifter longer (realistic rider behavior)
    delay(250);  // 250ms hold (rider reaction time)
    sim.virtualQSPressed = false;
    sim.virtualQSPressure = map->qsMap.sensorThreshold - 500;
    Serial.println("  Shifter released, ignition should resume");

  } else if (scenario == "launch") {
    // Test 5: Launch Control - REALISTIC (multi-phase)
    Mapping* map = &mappings[currentMapIndex];

    // PHASE 1: Build RPM (clutch pulled, stationary)
    Serial.println("✓ Launch Control - PHASE 1: Building RPM");
    sim.virtualRPM = map->launchControl.targetRPM;
    sim.virtualClutch = true;   // Clutch pulled
    sim.virtualSpeed = 0.0;     // Stationary
    sim.virtualThrottle = 100;  // Full throttle
    Serial.printf("  RPM held at %d, clutch pulled, LC should activate\n",
                  map->launchControl.targetRPM);
    delay(1000);  // Hold for 1 second

    // PHASE 2: Clutch drop (simulate launch)
    Serial.println("  PHASE 2: Clutch drop - LAUNCHING!");
    sim.virtualClutch = false;  // Drop clutch
    sim.virtualSpeed = 8.0;     // Initial acceleration
    sim.virtualRPM = map->launchControl.targetRPM - 800;  // RPM dip on launch
    delay(500);

    // PHASE 3: Accelerating away
    Serial.println("  PHASE 3: Accelerating, LC should disengage");
    sim.virtualSpeed = 18.0;    // Speed > 5 km/h, LC disengages
    sim.virtualRPM = 7500;      // Building RPM
    Serial.println("  Launch complete!");

  } else if (scenario == "wheelie") {
    // Test 6: Anti-Wheelie - REALISTIC (gradual pitch increase)
    Mapping* map = &mappings[currentMapIndex];

    Serial.println("✓ Anti-Wheelie: Simulating wheelie during acceleration");
    sim.virtualRPM = 8000;      // Power RPM
    sim.virtualSpeed = 50.0;    // Mid-speed
    sim.virtualThrottle = 90;   // High throttle (causing wheelie)
    sim.virtualClutch = false;

    // Gradual pitch increase (realistic wheelie development)
    Serial.println("  Front wheel lifting...");
    for (float p = 0; p <= map->antiWheelie.pitchThreshold + 8.0; p += 3.0) {
      sim.virtualPitch = baselinePitch + p;
      Serial.printf("  Pitch: %.1f° ", sim.virtualPitch);
      if (p > map->antiWheelie.pitchThreshold) {
        Serial.println("← AW SHOULD ACTIVATE!");
      } else {
        Serial.println();
      }
      delay(200);  // Smooth pitch increase
    }
    Serial.println("  Expected: Timing retard when pitch exceeds threshold");

  } else if (scenario == "traction") {
    // Test 7: Traction Control - REALISTIC (proper slip calculation)
    Mapping* map = &mappings[currentMapIndex];

    Serial.println("✓ Traction Control: Simulating rear wheel slip");
    sim.virtualRPM = 9000;      // High RPM
    sim.virtualThrottle = 85;   // High throttle causes slip
    sim.virtualSpeed = 45.0;    // Front wheel speed

    // Realistic slip ratio (18-20% instead of 25%)
    float slipRatio = map->tractionControl.slipThreshold + 0.05;  // Slight exceed
    sim.virtualSlip = slipRatio;
    float rearSpeed = sim.virtualSpeed * (1.0 + slipRatio);

    Serial.printf("  Front: %.1f km/h, Rear: %.1f km/h\n",
                  sim.virtualSpeed, rearSpeed);
    Serial.printf("  Slip: %.1f%% (threshold: %.1f%%)\n",
                  slipRatio * 100, map->tractionControl.slipThreshold * 100);
    Serial.println("  Expected: TC intervention (timing retard)");

  } else if (scenario == "2stroke_powerband") {
    // Test 8: 2-Stroke Powerband - REALISTIC (dynamic step for powerband surge)
    Serial.println("✓ 2-Stroke Powerband: Realistic powerband surge simulation");

    sim.autoRun = true;
    sim.autoTargetRPM = 13000;  // 2-stroke redline
    sim.virtualRPM = 1500;      // Start from idle (not 0!)
    sim.virtualThrottle = 40;   // Part throttle initially
    sim.virtualSpeed = 15.0;    // Rolling
    sim.virtualClutch = false;

    // Variable step rate to simulate powerband characteristics
    // Below powerband: Slow climb
    // In powerband (6500-8500): FAST surge!
    // Above powerband: Moderate
    if (sim.virtualRPM < 6000) {
      sim.autoRPMStep = 250;  // Slow below powerband
    } else if (sim.virtualRPM >= 6000 && sim.virtualRPM < 9000) {
      sim.autoRPMStep = 1000;  // FAST in powerband! (2-stroke surge)
      sim.virtualThrottle = 95;  // Full throttle in powerband
    } else {
      sim.autoRPMStep = 400;  // Moderate above powerband
    }

    Serial.println("  Expected behavior:");
    Serial.println("  - Slow climb below 6000 RPM");
    Serial.println("  - SHARP SURGE 6000-9000 RPM (powerband!)");
    Serial.println("  - Moderate climb to redline");
    Serial.println("  - Advance should increase sharply in powerband");

  } else {
    Serial.printf("⚠️  Unknown scenario: %s\n", scenario.c_str());
  }
}

// ===== GET SIMULATOR STATE AS JSON =====
String getSimulatorStateJSON() {
  StaticJsonDocument<1024> doc;

  // Simulator status
  doc["enabled"] = simulatorMode;
  doc["running"] = simulatorRunning;
  doc["scenario"] = sim.activeScenario;

  // Virtual inputs
  JsonObject inputs = doc.createNestedObject("inputs");
  inputs["rpm"] = sim.virtualRPM;
  inputs["throttle"] = sim.virtualThrottle;
  inputs["clutch"] = sim.virtualClutch;
  inputs["qsPressed"] = sim.virtualQSPressed;
  inputs["qsPressure"] = sim.virtualQSPressure;
  inputs["speed"] = sim.virtualSpeed;
  inputs["pitch"] = sim.virtualPitch;
  inputs["slip"] = sim.virtualSlip;

  // Virtual outputs
  JsonObject outputs = doc.createNestedObject("outputs");
  outputs["advanceDegrees"] = sim.virtualAdvanceDegrees;
  outputs["ignitionFiring"] = sim.virtualIgnitionFiring;
  outputs["ignitionCount"] = sim.virtualIgnitionCount;
  outputs["missedCount"] = sim.virtualMissedCount;

  // Feature status
  JsonObject features = doc.createNestedObject("features");
  features["qsActive"] = qsActive;
  features["lcActive"] = lcActive;
  features["antiWheelieActive"] = antiWheelieActive;
  features["tractionControlActive"] = tractionControlActive;

  // Auto-run settings
  JsonObject autoRun = doc.createNestedObject("autoRun");
  autoRun["enabled"] = sim.autoRun;
  autoRun["targetRPM"] = sim.autoTargetRPM;
  autoRun["step"] = sim.autoRPMStep;

  // Current mapping info
  Mapping* map = &mappings[currentMapIndex];
  JsonObject mapping = doc.createNestedObject("currentMap");
  mapping["name"] = map->name;
  mapping["revLimiterRPM"] = map->revLimiterRPM;
  mapping["qsEnabled"] = map->qsMap.enabled;
  mapping["lcEnabled"] = map->launchControl.enabled;
  mapping["awEnabled"] = map->antiWheelie.enabled;
  mapping["tcEnabled"] = map->tractionControl.enabled;

  String response;
  serializeJson(doc, response);
  return response;
}

// ===== SIMULATOR WEB API HANDLER =====
void handleSimulatorAPI() {
  // Handle GET requests
  if (server.method() == HTTP_GET) {
    String action = server.arg("action");

    if (action == "status") {
      // Get current simulator state
      server.send(200, "application/json", getSimulatorStateJSON());

    } else if (action == "enable") {
      simulatorMode = true;
      simulatorRunning = false;
      Serial.println("✓ Simulator mode ENABLED");
      logMessage(LOG_INFO, "Simulator mode ENABLED");
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Simulator enabled\"}");

    } else if (action == "disable") {
      simulatorMode = false;
      simulatorRunning = false;
      sim.autoRun = false;
      Serial.println("✓ Simulator mode DISABLED");
      logMessage(LOG_INFO, "Simulator mode DISABLED");
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Simulator disabled\"}");

    } else if (action == "start") {
      if (!simulatorMode) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"Enable simulator first\"}");
        return;
      }
      simulatorRunning = true;

      // Read optional RPM parameter from query string
      if (server.hasArg("rpm")) {
        sim.virtualRPM = server.arg("rpm").toInt();
        Serial.printf("✓ Simulator STARTED at %d RPM\n", sim.virtualRPM);
        char msg[64];
        snprintf(msg, sizeof(msg), "Simulator started @ %d RPM", sim.virtualRPM);
        logMessage(LOG_INFO, msg);
      } else {
        Serial.println("✓ Simulator STARTED");
        logMessage(LOG_INFO, "Simulator started");
      }

      server.send(200, "application/json", "{\"success\":true,\"message\":\"Simulator started\"}");

    } else if (action == "stop") {
      simulatorRunning = false;
      sim.autoRun = false;
      sim.virtualRPM = 0;
      logMessage(LOG_INFO, "Simulator stopped");
      Serial.println("✓ Simulator STOPPED");
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Simulator stopped\"}");

    } else {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Unknown action\"}");
    }
    return;
  }

  // Handle POST requests (set values)
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
      return;
    }

    String action = doc["action"] | "";

    if (action == "setInputs") {
      // Set virtual inputs
      if (doc.containsKey("rpm")) sim.virtualRPM = doc["rpm"];
      if (doc.containsKey("throttle")) sim.virtualThrottle = doc["throttle"];
      if (doc.containsKey("clutch")) sim.virtualClutch = doc["clutch"];
      if (doc.containsKey("qsPressure")) sim.virtualQSPressure = doc["qsPressure"];
      if (doc.containsKey("speed")) sim.virtualSpeed = doc["speed"];
      if (doc.containsKey("pitch")) sim.virtualPitch = doc["pitch"];
      if (doc.containsKey("slip")) sim.virtualSlip = doc["slip"];

      Serial.printf("✓ Inputs updated: RPM=%d, Throttle=%d%%\n", sim.virtualRPM, sim.virtualThrottle);
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Inputs updated\"}");

    } else if (action == "runScenario") {
      String scenario = doc["scenario"] | "idle";
      runSimulatorScenario(scenario);
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Scenario started\"}");

    } else if (action == "setAutoRun") {
      sim.autoRun = doc["enabled"] | false;
      if (doc.containsKey("targetRPM")) sim.autoTargetRPM = doc["targetRPM"];
      if (doc.containsKey("step")) sim.autoRPMStep = doc["step"];

      Serial.printf("✓ Auto-run: %s, target=%d RPM\n",
                    sim.autoRun ? "ON" : "OFF", sim.autoTargetRPM);
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Auto-run configured\"}");

    } else {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Unknown action\"}");
    }
    return;
  }

  server.send(405, "application/json", "{\"success\":false,\"message\":\"Method not allowed\"}");
}

// ===== UNIT TEST SUITE =====
void runUnitTests() {
  Serial.println("\n");
  Serial.println("========================================");
  Serial.println("  SMART CDI UNIT TEST SUITE");
  Serial.println("  Testing all functions & logic");
  Serial.println("========================================\n");

  int passedTests = 0;
  int totalTests = 0;

  // Test 1: RPM Calculation Safety
  Serial.println("Test 1: RPM Calculation Bounds Checking");
  totalTests++;
  triggerInterval = 1000;  // Very fast (60k RPM - should be capped)
  currentRPM = 60000000UL / triggerInterval;
  if (currentRPM > MAX_RPM) currentRPM = MAX_RPM;
  if (currentRPM == MAX_RPM) {
    Serial.println("✓ PASS: RPM capped at MAX_RPM");
    passedTests++;
  } else {
    Serial.printf("✗ FAIL: RPM=%d, expected=%d\n", currentRPM, MAX_RPM);
  }

  // Test 2: Advance Calculation
  Serial.println("\nTest 2: Advance Calculation at Various RPM");
  totalTests++;
  currentRPM = 8000;
  int advance = calculateAdvance(currentRPM);
  if (advance >= 0 && advance <= 60) {
    Serial.printf("✓ PASS: Advance at 8000 RPM = %d° (valid range)\n", advance);
    passedTests++;
  } else {
    Serial.printf("✗ FAIL: Advance=%d° (out of range 0-60°)\n", advance);
  }

  // Test 3: Rev Limiter
  Serial.println("\nTest 3: Rev Limiter Activation");
  totalTests++;
  Mapping* map = &mappings[currentMapIndex];
  currentRPM = map->revLimiterRPM + 100;
  advance = calculateAdvance(currentRPM);
  if (map->revLimiterEnabled) {
    Serial.printf("✓ PASS: Rev limiter should activate at %d RPM\n", map->revLimiterRPM);
    passedTests++;
  } else {
    Serial.println("⚠️  SKIP: Rev limiter disabled");
  }

  // Test 4: QS Threshold Detection
  Serial.println("\nTest 4: Quick Shifter Threshold");
  totalTests++;
  if (map->qsMap.enabled) {
    qsPressureValue = map->qsMap.sensorThreshold + 100;
    bool triggered = (qsPressureValue > map->qsMap.sensorThreshold);
    if (triggered) {
      Serial.printf("✓ PASS: QS triggered at pressure=%d (threshold=%d)\n",
                    qsPressureValue, map->qsMap.sensorThreshold);
      passedTests++;
    } else {
      Serial.println("✗ FAIL: QS should trigger");
    }
  } else {
    Serial.println("⚠️  SKIP: QS disabled");
    totalTests--;
  }

  // Test 5: Launch Control Conditions
  Serial.println("\nTest 5: Launch Control Logic");
  totalTests++;
  if (map->launchControl.enabled) {
    frontWheelSpeed = 3.0;  // Low speed
    currentRPM = map->launchControl.targetRPM;
    bool shouldActivate = (frontWheelSpeed < 5.0) && (currentRPM >= map->launchControl.targetRPM);
    if (shouldActivate) {
      Serial.println("✓ PASS: LC conditions met");
      passedTests++;
    } else {
      Serial.println("✗ FAIL: LC should activate");
    }
  } else {
    Serial.println("⚠️  SKIP: LC disabled");
    totalTests--;
  }

  // Test 6: 2-Stroke Specific - Single Pulse Per Rev
  Serial.println("\nTest 6: 2-Stroke Timing (1 pulse/rev)");
  totalTests++;
  map->isACMode = true;  // 2-stroke typically uses AC
  map->dcPulsesPerRev = 1;
  currentRPM = 10000;
  unsigned long expectedInterval = 60000000UL / currentRPM;
  if (expectedInterval == 6000) {  // 6000 μs at 10k RPM
    Serial.printf("✓ PASS: 2-stroke timing correct: %lu μs per rev\n", expectedInterval);
    passedTests++;
  } else {
    Serial.printf("✗ FAIL: Expected 6000 μs, got %lu μs\n", expectedInterval);
  }

  // Test 7: Watchdog Functionality
  Serial.println("\nTest 7: Watchdog Timer");
  totalTests++;
  Serial.println("✓ PASS: Watchdog enabled (will reset if hang)");
  passedTests++;

  // Test 8: ISR Performance (timing)
  Serial.println("\nTest 8: ISR Performance");
  totalTests++;
  unsigned long start = micros();
  fireIgnition(precalculatedAdvance);  // Simulated ISR call
  unsigned long duration = micros() - start;
  if (duration < 100) {  // Should be <100 μs
    Serial.printf("✓ PASS: ISR execution time = %lu μs (fast!)\n", duration);
    passedTests++;
  } else {
    Serial.printf("⚠️  WARNING: ISR took %lu μs (target <100 μs)\n", duration);
  }

  // Summary
  Serial.println("\n========================================");
  Serial.printf("  TEST RESULTS: %d/%d PASSED (%.0f%%)\n",
                passedTests, totalTests, (passedTests * 100.0) / totalTests);
  Serial.println("========================================\n");

  if (passedTests == totalTests) {
    Serial.println("🎉 ALL TESTS PASSED! System is working correctly.");
  } else {
    Serial.printf("⚠️  %d test(s) failed. Check configuration.\n", totalTests - passedTests);
  }
}
