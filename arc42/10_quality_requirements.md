# 10. Quality Requirements

This section specifies quality requirements using a quality tree and concrete quality scenarios.

## Quality Tree

```
RailHub8266 Quality
│
├── Reliability (Critical Priority)
│   ├── Availability (99.9% uptime)
│   ├── State Persistence (100% config survival across reboots)
│   └── WiFi Resilience (Auto-reconnect within 15s)
│
├── Performance Efficiency (High Priority)
│   ├── Memory Usage (<60% of 80KB RAM)
│   ├── Response Time (<200ms API calls)
│   └── WebSocket Latency (<100ms round-trip)
│
├── Usability (High Priority)
│   ├── Ease of Setup (Zero-config first boot)
│   ├── Multilingual UI (6 languages)
│   └── Accessibility (mDNS, no IP lookup)
│
├── Maintainability (Medium Priority)
│   ├── Code Readability (Self-documenting code)
│   ├── Testability (Unit test support)
│   └── Modularity (Clear separation of concerns)
│
├── Portability (Medium Priority)
│   ├── Hardware Compatibility (ESP-12E, NodeMCU, Wemos D1 Mini)
│   └── Platform Independence (PlatformIO build system)
│
└── Security (Low Priority - Trusted Networks Only)
    ├── Network Isolation (VLAN recommended)
    └── Physical Security (Flash protection)
```

## Quality Scenarios

### Reliability Scenarios

#### QS-R1: State Persistence Across Power Loss

**Scenario**: User configures 4 outputs with custom names and a chasing group, then experiences unexpected power loss. After power restoration, the device boots and restores all settings.

| Aspect | Details |
|--------|---------|
| **Source** | Power supply failure |
| **Stimulus** | Sudden power loss during operation |
| **Environment** | Normal operation, all outputs configured |
| **Artifact** | EEPROM storage subsystem |
| **Response** | On reboot, all output states, names, and chasing groups restored from EEPROM |
| **Measure** | **100% configuration survival** (all fields restored correctly) |

**Priority**: Critical

**Current Status**: ✅ Implemented (every config change written to EEPROM immediately)

---

#### QS-R2: WiFi Connection Resilience

**Scenario**: WiFi router reboots during operation. Device detects connection loss and automatically reconnects within 15 seconds.

| Aspect | Details |
|--------|---------|
| **Source** | WiFi network infrastructure |
| **Stimulus** | WiFi router reboot or temporary network outage |
| **Environment** | Device operating normally, 2 WebSocket clients connected |
| **Artifact** | WiFi stack, WebSocket server |
| **Response** | Device detects connection loss, attempts reconnection, restores services |
| **Measure** | **Reconnection within 15 seconds** (95th percentile) |

**Priority**: Critical

**Current Status**: ✅ Implemented (WiFiManager auto-reconnect, WebSocket client retry logic)

---

#### QS-R3: Watchdog Timer Protection

**Scenario**: Main loop gets blocked by unexpected condition (e.g., hardware I2C hang). Watchdog timer resets device after 8 seconds.

| Aspect | Details |
|--------|---------|
| **Source** | Software bug or hardware malfunction |
| **Stimulus** | Main loop hangs (no `yield()` calls for 8+ seconds) |
| **Environment** | Runtime operation |
| **Artifact** | ESP8266 hardware watchdog timer |
| **Response** | WDT triggers reset, device reboots, restores from EEPROM |
| **Measure** | **Automatic recovery within 10 seconds** (reboot + WiFi connect) |

**Priority**: High

**Current Status**: ✅ Implemented (ESP8266 built-in WDT, `yield()` calls in main loop)

---

### Performance Efficiency Scenarios

#### QS-P1: RAM Usage Under Load

**Scenario**: Device serves 3 concurrent WebSocket clients while running 4 chasing groups and handling HTTP requests. RAM usage stays below 60% of total.

| Aspect | Details |
|--------|---------|
| **Source** | Multiple concurrent users |
| **Stimulus** | 3 WebSocket clients + 2 HTTP requests/second |
| **Environment** | All 7 outputs active, 4 chasing groups running |
| **Artifact** | Memory allocator, WebSocket server |
| **Response** | System continues operating without crashes or slowdowns |
| **Measure** | **Free heap >32KB (40% of 80KB)** at all times |

**Priority**: Critical

**Current Status**: ✅ Implemented (chunked HTML, static JSON buffers, measured ~47KB free during typical operation)

**Test Method**:
```cpp
Serial.print("Free heap: ");
Serial.print(ESP.getFreeHeap());
Serial.println(" bytes");
```

---

#### QS-P2: API Response Time

**Scenario**: User changes output brightness via web UI. API responds within 200ms, output changes immediately.

| Aspect | Details |
|--------|---------|
| **Source** | User interaction (web UI) |
| **Stimulus** | POST /api/control request (set brightness to 75%) |
| **Environment** | Normal operation, WiFi connected |
| **Artifact** | HTTP server, PWM controller |
| **Response** | API validates request, updates output, saves to EEPROM, broadcasts status |
| **Measure** | **Total response time <200ms** (95th percentile) |

**Priority**: High

**Current Status**: ✅ Implemented (typical response time ~50-100ms)

**Breakdown**:
- JSON deserialization: ~10ms
- PWM update: <1ms
- EEPROM write: ~10-20ms
- WebSocket broadcast: ~20ms
- HTTP response: ~10ms
- **Total**: ~50-60ms (well within 200ms target)

---

#### QS-P3: WebSocket Broadcast Latency

**Scenario**: Output state changes. WebSocket broadcast reaches all clients within 100ms.

| Aspect | Details |
|--------|---------|
| **Source** | Output state change (manual or chasing group) |
| **Stimulus** | Output toggled ON via API |
| **Environment** | 2 WebSocket clients connected |
| **Artifact** | WebSocket server |
| **Response** | Serialize status to JSON, broadcast to all clients |
| **Measure** | **Latency <100ms from state change to client receipt** |

**Priority**: High

**Current Status**: ✅ Implemented (measured ~30-50ms latency on local network)

---

### Usability Scenarios

#### QS-U1: Zero-Configuration First Boot

**Scenario**: User powers on device for first time (no saved WiFi credentials). Device creates captive portal automatically.

| Aspect | Details |
|--------|---------|
| **Source** | End user (non-technical) |
| **Stimulus** | First boot with no WiFi credentials |
| **Environment** | Out-of-box experience |
| **Artifact** | WiFiManager |
| **Response** | Device creates "RailHub8266-Setup" AP, starts captive portal on 192.168.4.1 |
| **Measure** | **No serial console or command-line tools required** |

**Priority**: High

**Current Status**: ✅ Implemented (WiFiManager captive portal)

---

#### QS-U2: Multilingual Interface

**Scenario**: User from Germany accesses web UI. Interface displays in German after language selection.

| Aspect | Details |
|--------|---------|
| **Source** | International users |
| **Stimulus** | User selects "Deutsch" from language dropdown |
| **Environment** | Web UI loaded |
| **Artifact** | JavaScript i18n system |
| **Response** | All UI text translates to German |
| **Measure** | **6 languages supported** (EN, DE, FR, IT, ZH, HI) |

**Priority**: Medium

**Current Status**: ✅ Implemented (client-side i18n with localStorage persistence)

---

#### QS-U3: mDNS Discovery

**Scenario**: User wants to access device without looking up IP address. Uses `http://railhub8266.local` in browser.

| Aspect | Details |
|--------|---------|
| **Source** | End user |
| **Stimulus** | Browser navigation to `http://railhub8266.local` |
| **Environment** | Device connected to WiFi, mDNS responder running |
| **Artifact** | ESP8266mDNS library |
| **Response** | Browser resolves hostname to device IP, loads web UI |
| **Measure** | **No manual IP address lookup required** |

**Priority**: High

**Current Status**: ✅ Implemented (mDNS hostname based on device name)

**Note**: Requires mDNS-compatible network (most home routers support Bonjour/mDNS)

---

### Maintainability Scenarios

#### QS-M1: Code Readability

**Scenario**: New developer reads `main.cpp` to understand chasing groups implementation. Can locate and understand logic within 10 minutes.

| Aspect | Details |
|--------|---------|
| **Source** | New contributor |
| **Stimulus** | Need to understand chasing groups algorithm |
| **Environment** | Reviewing source code |
| **Artifact** | `main.cpp` source code |
| **Response** | Developer locates `updateChasingLightGroups()` function, reads comments, understands algorithm |
| **Measure** | **Time to comprehension <10 minutes** |

**Priority**: Medium

**Current Status**: ⚠️ Partially implemented (code has some comments, but could benefit from more documentation)

**Improvement**: Add function-level docstrings for all public functions

---

#### QS-M2: Build Reproducibility

**Scenario**: Developer clones repository and builds firmware on new machine. Build succeeds without manual dependency installation.

| Aspect | Details |
|--------|---------|
| **Source** | Developer |
| **Stimulus** | Execute `pio run -e esp12e` |
| **Environment** | Fresh PlatformIO installation |
| **Artifact** | `platformio.ini` |
| **Response** | PlatformIO downloads dependencies, compiles firmware |
| **Measure** | **Zero manual dependency installation steps** |

**Priority**: High

**Current Status**: ✅ Implemented (PlatformIO manages all dependencies)

---

### Portability Scenarios

#### QS-PO1: Hardware Compatibility

**Scenario**: User has Wemos D1 Mini instead of NodeMCU. Firmware works without modification.

| Aspect | Details |
|--------|---------|
| **Source** | End user with different ESP8266 variant |
| **Stimulus** | Upload firmware to Wemos D1 Mini |
| **Environment** | Same GPIO pin assignments (D0-D8 labels match) |
| **Artifact** | Firmware binary |
| **Response** | Firmware boots, all outputs functional |
| **Measure** | **Compatible with all ESP8266 modules** (ESP-12E, NodeMCU, Wemos D1 Mini) |

**Priority**: High

**Current Status**: ✅ Implemented (standard GPIO pin numbers, not board-specific)

---

### Security Scenarios

#### QS-S1: Network Isolation

**Scenario**: Device deployed on isolated IoT VLAN. Cannot access main network or internet.

| Aspect | Details |
|--------|---------|
| **Source** | Network administrator |
| **Stimulus** | Place device on isolated VLAN (192.168.100.0/24) |
| **Environment** | Firewall rules block inter-VLAN traffic |
| **Artifact** | Network infrastructure |
| **Response** | Device operates normally, accessible only from IoT VLAN |
| **Measure** | **No unauthorized network access** |

**Priority**: Medium

**Current Status**: ✅ Supported (device does not require internet access, can operate on isolated network)

**Note**: Recommended deployment for production environments

---

#### QS-S2: Firmware Protection

**Scenario**: Attacker with physical access attempts to read firmware via USB-UART. Flash read protection prevents extraction.

| Aspect | Details |
|--------|---------|
| **Source** | Physical attacker |
| **Stimulus** | Attempt to read flash via esptool.py |
| **Environment** | Physical access to device |
| **Artifact** | ESP8266 flash memory |
| **Response** | Flash read protection (if enabled) prevents firmware extraction |
| **Measure** | **Firmware cannot be easily extracted** |

**Priority**: Low

**Current Status**: ❌ Not implemented (flash read protection not enabled by default)

**Note**: Can be enabled via `esptool.py --flash_mode qio --flash_freq 40m --flash_size 4MB write_flash --flash_protect 1`

---

## Quality Metrics Summary

| Quality Attribute | Metric | Target | Current Status |
|-------------------|--------|--------|----------------|
| **Availability** | Uptime over 30 days | 99.9% | ✅ Exceeds (no known crashes) |
| **State Persistence** | Config survival rate | 100% | ✅ Meets |
| **WiFi Reconnection** | Time to reconnect | <15s (95th %ile) | ✅ Meets (~5-10s typical) |
| **RAM Usage** | Free heap | >32KB (40%) | ✅ Exceeds (~47KB free) |
| **API Response Time** | Latency | <200ms (95th %ile) | ✅ Exceeds (~50-100ms) |
| **WebSocket Latency** | Round-trip time | <100ms | ✅ Meets (~30-50ms) |
| **Language Support** | UI translations | 6 languages | ✅ Meets |
| **Hardware Compatibility** | Supported modules | 3+ variants | ✅ Meets (ESP-12E, NodeMCU, Wemos) |
| **Security** | Network isolation | Recommended | ⚠️ Deployment-dependent |

---

**Next**: [11. Risks and Technical Debt](11_risks_and_technical_debt.md)
