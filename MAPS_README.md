# Smart CDI Maps Documentation

## 📁 Available Maps

### 1. Default Optimal (Map ID: 1)
**Purpose:** Balanced all-around performance
**Rev Limiter:** 11,500 RPM
**Cut Pattern:** 1 (50% fire)
**Features:**
- ✅ Quick Shifter: Active 3,000-11,500 RPM (progressive kill time 150-60ms)
- ✅ Launch Control: 6,000 RPM target, 8° retard, soft cut
- ✅ Anti-Wheelie: 20° pitch threshold, 5° retard, soft cut
- ✅ Traction Control: 15% slip threshold, 5° retard, soft cut

**Ignition Curve:** Progressive advance 5°→36° peaking at 7,500-8,000 RPM

**Best For:** General purpose racing, track days, versatile setup

---

### 2. Street (Map ID: 2)
**Purpose:** Daily riding, smooth & safe
**Rev Limiter:** 10,500 RPM
**Cut Pattern:** 2 (66% fire - smoother)
**Features:**
- ✅ Quick Shifter: Active 2,500-10,500 RPM (gentle kill time 150-90ms)
- ❌ Launch Control: Disabled (not needed for street)
- ✅ Anti-Wheelie: 25° pitch threshold (more tolerant), 8° retard
- ✅ Traction Control: 20% slip threshold (less aggressive), 8° retard

**Ignition Curve:** Conservative advance 5°→33° for smooth power delivery

**Best For:** Daily commuting, street riding, beginner-friendly

---

### 3. Race (Map ID: 3)
**Purpose:** Maximum track performance
**Rev Limiter:** 12,100 RPM
**Cut Pattern:** 1 (50% fire)
**Features:**
- ✅ Quick Shifter: Active 3,500-12,100 RPM (aggressive kill time 150-55ms)
- ✅ Launch Control: 6,500 RPM target, 10° retard, 66% cut pattern
- ✅ Anti-Wheelie: 18° pitch threshold (aggressive), 5° retard
- ✅ Traction Control: 12% slip threshold (tight), 4° retard

**Ignition Curve:** Aggressive advance 5°→38° peaking at 7,000-8,500 RPM

**Best For:** Circuit racing, competitive track use, experienced riders

---

### 4. Drag (Map ID: 4)
**Purpose:** Drag racing, launch focus
**Rev Limiter:** 11,800 RPM
**Cut Pattern:** 1 (50% fire)
**Features:**
- ✅ Quick Shifter: Active 4,000-11,800 RPM (fast kill time 150-55ms)
- ✅ Launch Control: **7,500 RPM target**, 15° retard, hard cut (critical for launch)
- ✅ Anti-Wheelie: 15° pitch threshold (very sensitive), hard cut pattern
- ✅ Traction Control: 18% slip threshold (allows controlled slip), 6° retard

**Ignition Curve:** Optimized for acceleration 5°→36° with smooth mid-range

**Best For:** Drag racing, standing start acceleration, launch control focused

---

## 🎛️ Cut Pattern Reference

| Pattern | Type | Fire Rate | Description | Best Use |
|---------|------|-----------|-------------|----------|
| 0 | Hard Cut | 0% | Complete spark cut | Emergency only |
| 1 | Soft Cut | 50% | Fire every 2nd spark | Balanced control |
| 2 | Soft Cut | 66% | Skip 1 of 3 sparks | Smooth limiting |
| 3 | Soft Cut | 33% | Fire 1 of 3 sparks | Gentle limiting |

---

## 📊 Quick Comparison

| Feature | Default | Street | Race | Drag |
|---------|---------|--------|------|------|
| **Max RPM** | 11,500 | 10,500 | 12,100 | 11,800 |
| **Peak Advance** | 36° | 33° | 38° | 36° |
| **QS Min RPM** | 3,000 | 2,500 | 3,500 | 4,000 |
| **QS Fastest Kill** | 60ms | 90ms | 55ms | 55ms |
| **LC Target** | 6,000 | N/A | 6,500 | **7,500** |
| **AW Threshold** | 20° | 25° | 18° | 15° |
| **TC Slip %** | 15% | 20% | 12% | 18% |
| **Aggressiveness** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |

---

## 🔧 Tuning Guidelines

### Adjusting Rev Limiter
- **+500 RPM:** More top-end power (engine must be built for it)
- **-500 RPM:** Safer for stock engines, longer lifespan

### Adjusting Cut Patterns
- **Pattern 0 (Hard Cut):** Use for critical safety limits only
- **Pattern 1 (50%):** Best balance of control & engine safety
- **Pattern 2 (66%):** Smoother for street riding
- **Pattern 3 (33%):** Gentlest cut, use for sensitive situations

### Quick Shifter Kill Time
- **Longer (>100ms):** Smoother shifts, less wear, street use
- **Shorter (<70ms):** Faster shifts, race use, requires good clutchless technique

### Launch Control RPM
- **Lower (5,000-6,000):** Better traction on slippery surfaces
- **Higher (7,000-8,000):** Maximum acceleration, requires good traction

### Traction Control Slip Threshold
- **Lower (10-12%):** Aggressive intervention, dry conditions
- **Higher (18-25%):** Allows more slip, wet/loose surfaces

---

## ⚠️ Safety Notes

1. **Always test on dyno first** before track/street use
2. **Rev limiter is engine protection** - don't exceed engine capability
3. **Cut patterns affect engine loading** - harder cuts = more stress
4. **Quick shifter requires proper sensor** - test at low speeds first
5. **Launch control can cause wheelspin** - use on good traction only
6. **Monitor engine temps** during aggressive tuning
7. **Log data** and review for issues

---

## 🔄 Map Switching

Maps can be switched via:
1. **Physical buttons** on CDI unit
2. **Web dashboard** at http://[ESP32_IP]
3. **Mobile app** (if enabled)

**Active map** shows in dashboard and saves to EEPROM.

---

## 📝 Notes

- All maps tested with **2-stroke single cylinder** engines
- Adjust **tire sizes** in TC settings for your specific bike
- **Wheel sensor holes** affect TC calculation (default: 4 holes)
- Maps auto-save to SD card on ESP32
- **Pickup sensor offset** may need calibration per engine

---

**Version:** 2.2
**Last Updated:** 2025-12-04
**Author:** Smart CDI Development Team
