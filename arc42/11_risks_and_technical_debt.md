# 11. Risks and Technical Debt

This section identifies known risks, technical debt, and areas for improvement in the RailHub8266 architecture.

## Technical Risks

### TR-1: RAM Exhaustion Under High Load

**Severity**: 🔴 High

**Description**:
ESP8266 has only 80KB total RAM (~50KB available). With 3+ concurrent WebSocket clients and high API request rate, the system may exhaust available heap memory, causing crashes or reboots.

**Symptoms**:
- WebSocket connections dropped
- HTTP requests returning 500 errors
- WDT resets (watchdog timer triggered)
- Serial output: "Out of memory" errors

**Likelihood**: Medium (depends on deployment environment)

**Impact**: System unavailable until reboot

**Mitigation Strategies**:
1. **Implemented**:
   - Chunked HTML delivery (avoids 40KB allocation)
   - Static JSON documents (fixed 2KB allocation)
   - Limited WebSocket broadcast rate (500ms)
   
2. **Recommended**:
   - Limit WebSocket clients to 2-3 maximum (implement connection limit)
   - Add `ESP.getFreeHeap()` monitoring in main loop
   - Reject API requests if free heap <10KB
   - Implement client disconnect on low memory

**Example Fix**:
```cpp
if (ESP.getFreeHeap() < 10240) {  // <10KB free
    Serial.println("[WARN] Low memory, rejecting request");
    server->send(503, "application/json", "{\"error\":\"Low memory\"}");
    return;
}
```

---

### TR-2: EEPROM Write Wear

**Severity**: 🟡 Medium

**Description**:
EEPROM has ~100,000 write cycle limit. Frequent configuration changes (e.g., automated scripts toggling outputs every second) can wear out flash memory after ~27 hours of continuous writes.

**Calculation**:
- 100,000 writes / 3600 writes per hour = 27.7 hours

**Likelihood**: Low (typical use cases have infrequent config changes)

**Impact**: Configuration loss after EEPROM wear-out

**Mitigation Strategies**:
1. **Implemented**:
   - Write-on-change (not periodic)
   
2. **Recommended**:
   - Implement write throttling (max 1 write per second per output)
   - Batch writes (save all outputs together instead of individually)
   - Add write counter to EEPROM, log warnings at 80K writes
   - Use LittleFS for high-frequency data (if RAM allows)

**Example Fix**:
```cpp
unsigned long lastEEPROMWrite[7] = {0};
const unsigned long EEPROM_WRITE_INTERVAL = 1000;  // 1 second

void saveOutputState(int index) {
    if (millis() - lastEEPROMWrite[index] < EEPROM_WRITE_INTERVAL) {
        Serial.println("[WARN] EEPROM write throttled");
        return;
    }
    // ... save logic ...
    lastEEPROMWrite[index] = millis();
}
```

---

### TR-3: No User Authentication

**Severity**: 🔴 High (deployment-dependent)

**Description**:
Web interface and API have no authentication. Anyone on the local network can control outputs, delete chasing groups, or reset configuration.

**Attack Scenarios**:
- Malicious user toggles outputs randomly
- Script floods API with requests (DoS)
- Configuration deleted via `/api/reset`

**Likelihood**: Low (trusted network deployment)

**Impact**: Unauthorized control, configuration loss

**Mitigation Strategies**:
1. **Network-Level**:
   - Deploy on isolated VLAN (recommended)
   - Firewall rules (block ports 80, 81 from untrusted networks)
   - VPN access only (no direct internet exposure)
   
2. **Application-Level** (requires code changes):
   - HTTP Basic Auth (adds ~2KB RAM overhead)
   - API key validation (header-based authentication)
   - Rate limiting (max 10 requests/second per IP)

**Example Fix (HTTP Basic Auth)**:
```cpp
bool checkAuth() {
    if (!server->authenticate("admin", "password")) {
        server->requestAuthentication();
        return false;
    }
    return true;
}

server->on("/api/control", HTTP_POST, []() {
    if (!checkAuth()) return;
    // ... control logic ...
});
```

**Note**: This requires storing plaintext password in flash (not ideal). Consider hashed credentials.

---

### TR-4: No HTTPS/TLS Encryption

**Severity**: 🔴 Critical (deployment-dependent)

**Description**:
All HTTP and WebSocket traffic is unencrypted. Network eavesdropping can reveal WiFi passwords (during captive portal setup), output states, and API commands.

**Attack Scenarios**:
- WiFi packet capture reveals API commands
- Man-in-the-middle (MITM) attack modifies commands in transit
- Captive portal password sniffing

**Likelihood**: Low (WPA2-encrypted WiFi provides partial protection)

**Impact**: Data confidentiality breach, command injection

**Mitigation Strategies**:
1. **Network-Level**:
   - Use WPA2/WPA3 WiFi encryption (mandatory)
   - Deploy on trusted networks only
   - Never expose to public internet (no port forwarding)
   
2. **Application-Level** (NOT feasible on ESP8266):
   - HTTPS/TLS requires ~30KB RAM (exceeds available memory)
   - TLS support not feasible on ESP8266 due to memory constraints

**Workaround**:
- Document security limitations clearly in README.md ✅ (already documented)
- Recommend network isolation in deployment guide

---

### TR-5: Single-File Architecture Maintainability

**Severity**: 🟡 Medium

**Description**:
All code in single `main.cpp` file (~1400 LOC). As features grow, code becomes harder to navigate and maintain.

**Threshold**: Code exceeds 2000 LOC (approaching limit)

**Likelihood**: Medium (likely with future feature additions)

**Impact**: Slower development, higher bug risk

**Mitigation Strategies**:
1. **Refactoring Trigger**: If code exceeds 2000 LOC, split into modules:
   - `web_server.cpp` - HTTP/WebSocket handlers
   - `pwm_controller.cpp` - Output control, chasing groups, blink logic
   - `storage.cpp` - EEPROM read/write
   - `config.h` - Configuration constants
   
2. **Interim Measures**:
   - Use `#pragma region` or comment blocks to mark logical sections
   - Keep related functions together (locality of reference)

---

## Known Bugs

### BUG-1: WebSocket Disconnect Not Detected

**Severity**: 🟡 Medium

**Description**:
If client closes browser tab without proper WebSocket close handshake, server may not detect disconnect for several minutes. This wastes RAM for "zombie" connections.

**Reproduction**:
1. Open web UI in browser
2. Force-close browser (kill process, not graceful close)
3. Observe WebSocket server still broadcasts to closed connection

**Impact**: Wasted RAM (~2KB per zombie connection), potential memory leak

**Workaround**:
- Implement ping/pong heartbeat (WebSocket standard)
- Close connections with no pong response for 30 seconds

**Example Fix**:
```cpp
void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_PONG) {
        clientLastPong[num] = millis();
    }
}

void loop() {
    // In main loop, check for stale connections
    for (uint8_t i = 0; i < WS_MAX_CLIENTS; i++) {
        if (millis() - clientLastPong[i] > 30000) {
            ws->disconnect(i);
        }
    }
}
```

---

### BUG-2: mDNS Name Collision Not Handled

**Severity**: 🟢 Low

**Description**:
If two RailHub8266 devices on same network both use hostname `railhub8266.local`, mDNS collision occurs. Only one device will be discoverable.

**Reproduction**:
1. Flash two devices with same firmware
2. Both use default device name "ESP8266-Controller-01"
3. mDNS hostname collision

**Impact**: One device not discoverable via mDNS (still accessible via IP)

**Workaround**:
- User can customize device name via WiFiManager portal
- Recommend unique names in documentation

**Example Fix**:
```cpp
// Append last 4 digits of MAC address to hostname
String macSuffix = WiFi.macAddress().substring(12);
macSuffix.replace(":", "");
String hostname = String(customDeviceName) + "-" + macSuffix;
MDNS.begin(hostname.c_str());
```

---

## Technical Debt

### TD-1: No Unit Tests

**Severity**: 🟡 Medium

**Description**:
`test/` directory is empty. No automated tests for core functions (PWM control, chasing groups, EEPROM persistence).

**Impact**:
- Higher risk of regressions during code changes
- Slower development (manual testing only)
- Harder to refactor (no safety net)

**Recommended Actions**:
1. **Phase 1**: Add Unity tests for pure functions
   - `findOutputIndexByPin()` - GPIO pin lookups
   - `createChasingGroup()` - Group validation logic
   - `serializeStatusToJson()` - JSON serialization
   
2. **Phase 2**: Add integration tests
   - Boot sequence (EEPROM load)
   - API endpoint validation
   - Chasing group step timing

**Example Test**:
```cpp
// test/test_pin_lookup.cpp
#include <unity.h>

void test_find_valid_pin() {
    int index = findOutputIndexByPin(4);
    TEST_ASSERT_EQUAL(0, index);
}

void test_find_invalid_pin() {
    int index = findOutputIndexByPin(99);
    TEST_ASSERT_EQUAL(-1, index);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_find_valid_pin);
    RUN_TEST(test_find_invalid_pin);
    UNITY_END();
}
```

---

### TD-2: Inline HTML/CSS/JS in C++ Strings

**Severity**: 🟡 Medium

**Description**:
Web UI (HTML/CSS/JavaScript) embedded as C++ string literals via `F()` macro. Difficult to maintain and no syntax highlighting.

**Impact**:
- Harder to modify web UI
- No IDE support for HTML/CSS/JS editing
- Larger firmware binary size

**Recommended Actions**:
1. **Option A**: Move to SPIFFS/LittleFS
   - Trade-off: +4KB RAM overhead
   - Benefit: Easier web UI development
   
2. **Option B**: Build-time HTML minification
   - Use Node.js script to minify HTML/CSS/JS
   - Generate C++ header file with minified content
   - Benefit: Smaller binary, easier source editing

**Example Build Script** (Node.js):
```javascript
const fs = require('fs');
const html = fs.readFileSync('web/index.html', 'utf8');
const minified = html.replace(/\s+/g, ' ').trim();
const cpp = `const char HTML[] PROGMEM = R"(${minified})";`;
fs.writeFileSync('src/web_ui.h', cpp);
```

---

### TD-3: No Version Reporting

**Severity**: 🟢 Low

**Description**:
Firmware version not reported in `/api/status` or web UI. Makes debugging difficult (user reports issue, but unclear which firmware version).

**Impact**:
- Harder to track down version-specific bugs
- Users cannot verify firmware version without serial console

**Recommended Actions**:
Add version constant to `config.h`:
```cpp
#define FIRMWARE_VERSION "2.0.0"
#define BUILD_DATE __DATE__ " " __TIME__
```

Include in `/api/status`:
```json
{
  "firmwareVersion": "2.0.0",
  "buildDate": "Nov 16 2024 14:23:45",
  // ...
}
```

Display in web UI footer:
```html
<footer>
  RailHub8266 v2.0.0 | Build: Nov 16 2024
</footer>
```

---

### TD-4: Magic Numbers in Code

**Severity**: 🟢 Low

**Description**:
Code contains magic numbers (e.g., `500`, `2048`, `3000`) without named constants.

**Examples**:
```cpp
const unsigned long BROADCAST_INTERVAL = 500;  // ms (good)
if (now - lastBroadcast >= 500) { ... }  // ❌ magic number

DynamicJsonDocument doc(2048);  // ❌ magic number
if (holdDuration > 3000) { ... }  // ❌ magic number
```

**Impact**:
- Harder to understand code intent
- Difficult to tune parameters (scattered across code)

**Recommended Actions**:
Define constants in `config.h`:
```cpp
#define BROADCAST_INTERVAL_MS 500
#define JSON_DOCUMENT_SIZE 2048
#define PORTAL_TRIGGER_HOLD_MS 3000
```

---

### TD-5: Inconsistent Error Handling

**Severity**: 🟢 Low

**Description**:
Some functions return error codes, others use Serial logging, others have no error handling.

**Examples**:
```cpp
int findOutputIndexByPin(int pin) {
    // ... Returns -1 on error (good)
}

void saveOutputState(int index) {
    // ... No return value, no error handling (inconsistent)
}
```

**Impact**:
- Harder to debug API failures
- No unified error reporting

**Recommended Actions**:
1. Define error codes:
   ```cpp
   enum ErrorCode {
       OK = 0,
       ERR_INVALID_PIN = 1,
       ERR_EEPROM_WRITE_FAILED = 2,
       ERR_OUT_OF_MEMORY = 3
   };
   ```
   
2. Return error codes from all functions that can fail

---

## Risk Mitigation Priority

| Risk/Issue | Severity | Effort | Priority |
|------------|----------|--------|----------|
| TR-1: RAM Exhaustion | 🔴 High | Medium | **P1** (implement connection limits) |
| TR-3: No Authentication | 🔴 High | High | **P2** (document network isolation) |
| TR-4: No HTTPS/TLS | 🔴 Critical | N/A | **N/A** (not feasible on ESP8266) |
| BUG-1: WebSocket Disconnect | 🟡 Medium | Low | **P3** (add ping/pong) |
| TD-1: No Unit Tests | 🟡 Medium | High | **P4** (add basic tests) |
| TD-2: Inline HTML | 🟡 Medium | Medium | **P5** (low priority) |
| TR-2: EEPROM Wear | 🟡 Medium | Low | **P6** (add write throttling) |
| TR-5: Single-File Architecture | 🟡 Medium | High | **P7** (refactor at 2000 LOC) |
| BUG-2: mDNS Collision | 🟢 Low | Low | **P8** (document workaround) |
| TD-3: No Version Reporting | 🟢 Low | Low | **P9** (nice to have) |

---

**Next**: [12. Glossary](12_glossary.md)
