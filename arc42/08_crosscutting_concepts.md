# 8. Crosscutting Concepts

This section describes overall, principal regulations and solution approaches relevant to multiple parts of the system.

## Memory Management

### Strategy: Minimize Heap Allocations

**Rationale**: ESP8266 has only 80KB RAM (~50KB available after WiFi stack). Heap fragmentation can cause crashes.

**Approach**:
1. **Static Allocation**: Use global variables for persistent state
2. **Stack Allocation**: Use local variables for temporary data
3. **Fixed-Size Buffers**: `DynamicJsonDocument doc(2048)` with known max size
4. **Chunked Delivery**: Send HTML in small chunks instead of large strings

**Example**:
```cpp
// ❌ BAD: Large heap allocation
String htmlContent = generateFullPage();  // May allocate 40KB on heap
server->send(200, "text/html", htmlContent);

// ✅ GOOD: Chunked delivery
server->setContentLength(CONTENT_LENGTH_UNKNOWN);
server->send(200, "text/html", "");
server->sendContent(F("<html>..."));  // Small chunks
server->sendContent(F("<style>..."));
server->sendContent("");
```

### Flash String Macro `F()`

**Purpose**: Store string literals in flash memory instead of RAM

**Usage**:
```cpp
server->sendContent(F("<html><head>..."));  // String stored in flash
Serial.println(F("[INFO] System started"));
```

**Savings**: ~10-15KB RAM saved by using `F()` for static strings

### Memory Monitoring

**Metrics**:
```cpp
ESP.getFreeHeap();      // Available heap (bytes)
ESP.getHeapFragmentation();  // Fragmentation percentage
```

**Target**: Keep free heap >15KB (20% of 80KB) during normal operation

---

## Error Handling and Logging

### Logging Strategy

**Levels** (informal, via Serial):
- `[BOOT]` - Boot sequence messages
- `[INIT]` - Initialization steps
- `[WEB]` - HTTP/WebSocket events
- `[CMD]` - Output control commands
- `[EEPROM]` - Persistence operations
- `[CHASING]` - Chasing group updates
- `[WIFI]` - WiFi events
- `[ERROR]` - Error conditions
- `[WARN]` - Warnings

**Example**:
```cpp
Serial.println("[WEB] POST /api/control from " + clientIP.toString());
Serial.println("[ERROR] Invalid GPIO pin: " + String(pin));
```

**Configuration**: 115200 baud, enabled in all builds

### Error Handling Approach

**Strategy**: Defensive programming + input validation

**Validation Examples**:
```cpp
// Pin validation
int outputIndex = findOutputIndexByPin(pin);
if (outputIndex == -1) {
    Serial.println("[ERROR] Invalid GPIO pin: " + String(pin));
    server->send(404, "application/json", "{\"error\":\"Output not found\"}");
    return;
}

// Range validation
if (brightnessPercent < 0 || brightnessPercent > 100) {
    Serial.println("[ERROR] Invalid brightness");
    brightnessPercent = constrain(brightnessPercent, 0, 100);
}

// JSON deserialization
DeserializationError error = deserializeJson(doc, body);
if (error) {
    Serial.print("[ERROR] JSON parse failed: ");
    Serial.println(error.c_str());
    server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
}
```

**No Exceptions**: Arduino C++ does not use exceptions (disabled to save code size)

---

## Security

### Threat Model

**Assumption**: RailHub8266 operates on **trusted local networks only**

**Acceptable Risks**:
- Unencrypted HTTP/WebSocket (no TLS/HTTPS)
- No user authentication
- No API rate limiting
- No CSRF protection

**Rationale**: TLS requires ~30KB RAM (not feasible on ESP8266). Target deployment is home/hobby networks, not public internet.

### Security Measures

| Threat | Mitigation | Residual Risk |
|--------|------------|---------------|
| **Eavesdropping** | None (no TLS) | **HIGH**: WiFi traffic is unencrypted |
| **Unauthorized Access** | WiFiManager password (captive portal), WPA2 WiFi security | **MEDIUM**: No application-level auth |
| **Code Injection** | JSON validation, no `eval()` in JavaScript | **LOW**: Limited attack surface |
| **DoS (WiFi)** | WiFi deauth attacks | **HIGH**: Standard WiFi vulnerability |
| **DoS (HTTP)** | No rate limiting | **MEDIUM**: Can overwhelm with requests |
| **Flash Modification** | Physical access required for UART upload | **LOW**: Requires physical access |

### Recommendations for Production Deployment

If deploying in less trusted environments:
1. **Network Isolation**: Use separate VLAN/network for IoT devices
2. **Firewall Rules**: Block external access to ports 80, 81
3. **VPN Access**: Access web UI via VPN instead of direct exposure
4. **Custom Firmware**: Add HTTP Basic Auth (requires code modification)

---

## Configuration Management

### Configuration Storage

**Location**: EEPROM (512 bytes in flash)

**Structure**: Single `EEPROMData` struct (see Section 3 for details)

**Write Strategy**: Write-through cache (immediate persistence on every change)

**Example**:
```cpp
void saveOutputState(int index) {
    EEPROM.get(0, eepromData);  // Read current
    eepromData.outputStates[index] = outputStates[index];  // Update
    EEPROM.put(0, eepromData);  // Write back
    EEPROM.commit();  // Flush to flash
}
```

**Integrity Check**: `uint8_t checksum` field (currently unused, reserved for future CRC validation)

### Default Values

**First Boot** (uninitialized EEPROM, all 0xFF):
```cpp
if (eepromData.deviceName[0] == 0xFF) {
    // Initialize defaults
    for (int i = 0; i < 7; i++) {
        eepromData.outputStates[i] = false;
        eepromData.outputBrightness[i] = 255;
        eepromData.outputNames[i][0] = '\0';
        eepromData.outputIntervals[i] = 0;
    }
    strncpy(eepromData.deviceName, "ESP8266-Controller-01", 39);
    EEPROM.put(0, eepromData);
    EEPROM.commit();
}
```

### Configuration Reset

**Manual Reset**: POST to `/api/reset` clears EEPROM (fills with 0xFF)

**Factory Reset**: Hold GPIO 0 button for 3s → clears WiFi credentials → captive portal

---

## Internationalization (i18n)

### Language Support

**Supported Languages**:
1. English (default)
2. German (Deutsch)
3. French (Français)
4. Italian (Italiano)
5. Chinese (中文)
6. Hindi (हिन्दी)

### Implementation

**Approach**: Client-side JavaScript i18n (no server-side translation)

**Data Structure**:
```javascript
const i18n = {
    en: {
        tab_status: "Status",
        btn_all_on: "All ON",
        // ...
    },
    de: {
        tab_status: "Status",
        btn_all_on: "Alle AN",
        // ...
    },
    // ...
};
```

**Language Selection**:
```javascript
function changeLang(lang) {
    currentLang = lang;
    localStorage.setItem('lang', lang);
    document.querySelectorAll('[data-i18n]').forEach(el => {
        const key = el.getAttribute('data-i18n');
        el.textContent = i18n[lang][key];
    });
}
```

**Persistence**: Language preference stored in browser `localStorage`

---

## Performance Optimization

### WebSocket Broadcast Throttling

**Problem**: Sending status updates too frequently wastes bandwidth and CPU

**Solution**: Throttle broadcasts to 500ms interval

```cpp
const unsigned long BROADCAST_INTERVAL = 500;  // ms
unsigned long lastBroadcast = 0;

void loop() {
    unsigned long now = millis();
    if (now - lastBroadcast >= BROADCAST_INTERVAL) {
        broadcastStatus();
        lastBroadcast = now;
    }
}
```

**Rationale**: 500ms (2 Hz) provides responsive UI without overwhelming ESP8266

### JSON Document Sizing

**Problem**: `DynamicJsonDocument` allocates from heap

**Solution**: Use static sizing based on worst-case payload

```cpp
DynamicJsonDocument doc(2048);  // 2KB max
```

**Calculation**:
- System info: ~200 bytes
- 7 outputs × 150 bytes/output = 1050 bytes
- 4 chasing groups × 150 bytes/group = 600 bytes
- JSON overhead: ~200 bytes
- **Total**: ~2050 bytes → 2048 bytes sufficient

### Main Loop Yielding

**Problem**: Blocking operations can trigger WDT (watchdog timer) reset

**Solution**: Call `yield()` in main loop to allow WiFi stack to run

```cpp
void loop() {
    // ... processing ...
    yield();  // Allow WiFi/system tasks
}
```

**Frequency**: Every iteration of main loop

---

## Testability

### Unit Testing

**Framework**: Unity (embedded in PlatformIO)

**Environment**: `esp12e_test` (hardware testing on ESP8266)

**Current State**: No tests implemented (`test/` directory is empty)

**Proposed Structure**:
```
test/
  test_pwm_controller/
    test_output_control.cpp
  test_chasing_groups/
    test_group_creation.cpp
  test_eeprom/
    test_persistence.cpp
```

**Challenges**:
- No mocking framework (Unity is basic)
- Hardware-dependent (GPIO, WiFi)
- Limited test execution on ESP8266 (serial output only)

### Integration Testing

**Approach**: Manual testing via web UI and REST API

**Test Scenarios**:
1. Boot sequence (cold start, warm reboot)
2. Output control (all 7 outputs, brightness levels)
3. Chasing groups (create, delete, rename, step timing)
4. Blink intervals (various intervals, 0 = disable)
5. WiFi reconnection (router reboot simulation)
6. EEPROM persistence (reboot, check state restoration)
7. WebSocket updates (multiple clients, broadcast timing)

---

## Development Conventions

### Code Style

**Current State**: Inconsistent (legacy code)

**Recommendations**:
- **Indentation**: 4 spaces (current practice)
- **Braces**: K&R style (`{` on same line)
- **Naming**: 
  - Functions: `camelCase` (e.g., `executeOutputCommand`)
  - Variables: `camelCase` (e.g., `outputStates`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_OUTPUTS`)
- **Comments**: English, descriptive for complex logic

### Git Workflow

**Branching** (assumed, not documented):
- `main` branch for stable releases
- Feature branches for development
- Tag releases with version numbers (e.g., `v2.0.0`)

### Documentation

**Code Comments**:
- Function-level comments for public APIs
- Inline comments for non-obvious logic

**Example**:
```cpp
// Helper function to find output index by GPIO pin
// Returns -1 if pin not found
int findOutputIndexByPin(int pin) {
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        if (outputPins[i] == pin) {
            return i;
        }
    }
    return -1;
}
```

---

## Build and Release Process

### PlatformIO Environments

| Environment | Purpose | Output |
|-------------|---------|--------|
| `esp12e` | Production firmware | `.pio/build/esp12e/firmware.bin` (~400KB) |
| `native` | Unit tests on PC | x86 executable (limited functionality) |
| `esp12e_test` | Hardware unit tests | Firmware with Unity test framework |

### Build Flags

```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=0       # No debug output (production)
    -Wl,-Teagle.flash.4m1m.ld  # 4MB flash, 1MB program partition
```

### Version Management

**Current**: Hardcoded in README.md (`v2.0.0`)

**Recommendation**: Define version in `config.h`
```cpp
#define FIRMWARE_VERSION "2.0.0"
```

Then include in `/api/status` response:
```json
{
  "firmwareVersion": "2.0.0",
  // ...
}
```

---

**Next**: [9. Architecture Decisions](09_architecture_decisions.md)
