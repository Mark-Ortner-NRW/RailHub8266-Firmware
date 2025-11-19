# 2. Architecture Constraints

This section describes the technical and organizational constraints that limit the freedom of architectural decisions.

## Technical Constraints

### Hardware Constraints

| Constraint | Description | Impact |
|------------|-------------|--------|
| **TC-1: ESP8266 RAM** | 80KB total RAM (user ~50KB available after system overhead) | Severe memory pressure. Requires chunked HTML delivery, minimal dynamic allocations, limited WebSocket clients (2-3 max). |
| **TC-2: Flash Partition** | 1MB program partition (out of 4MB total flash) | Limits code size. Web UI must be minimal. No HTTPS/TLS (requires ~30KB RAM). |
| **TC-3: GPIO Limitations** | Only 7 usable GPIO pins (GPIO 0 reserved for boot button, GPIO 15 not usable for PWM) | Limits available outputs to 7 PWM-capable pins. |
| **TC-4: Single Core** | ESP8266 is single-threaded | All tasks run sequentially on one core. Requires yield() calls in main loop to prevent watchdog timeouts. |
| **TC-5: PWM API** | ESP8266 uses `analogWrite()` with 10-bit range (0-1023) | PWM setup uses simple analogWrite() API. Implementation uses 8-bit range (0-255) to save memory. |
| **TC-6: 3.3V Logic** | ESP8266 GPIO pins are 3.3V (NOT 5V tolerant) | Requires level shifters or transistors for 5V loads. |
| **TC-7: Current Limit** | Each GPIO can source/sink max 12mA | Direct LED driving only. Requires MOSFETs for higher currents. |

### Software Constraints

| Constraint | Description | Impact |
|------------|-------------|--------|
| **SC-1: Arduino Framework** | Platform uses Arduino core for ESP8266 | Limits access to low-level RTOS features (no FreeRTOS tasks). |
| **SC-2: EEPROM API** | ESP8266 uses `EEPROM.h` for flash emulation | Requires manual memory management. Limited to 512-byte structure. |
| **SC-3: No Native TLS** | TLS/HTTPS not feasible due to RAM constraints | API endpoints are unencrypted HTTP. Must operate on trusted networks only. |
| **SC-4: WiFiManager Dependency** | WiFiManager 2.0.17 required for captive portal | Adds ~15KB code, consumes ~5KB RAM. Cannot use lighter alternatives due to feature requirements. |
| **SC-5: ArduinoJson Dependency** | ArduinoJson 7.x required for JSON parsing | Uses statically allocated documents (2KB max) to avoid heap fragmentation. |
| **SC-6: WebSockets Library** | WebSockets 2.4.1 (links2004) required for real-time updates | Adds ~8KB code. Limited to 3 concurrent clients to avoid RAM exhaustion. |

### Organizational Constraints

| Constraint | Description | Impact |
|------------|-------------|--------|
| **OC-1: PlatformIO Build System** | Project uses PlatformIO, not Arduino IDE | Developers must use PlatformIO CLI or VSCode extension. |
| **OC-2: Single Developer** | Project maintained by single developer (mark_ortner@hotmail.de) | Limited development bandwidth. Prioritize simplicity over feature richness. |
| **OC-3: Open Source License** | MIT License (assumed, not explicitly stated in repo) | Code must remain open and freely usable. |
| **OC-4: GitHub Repository** | Code hosted on GitHub (Mark-Ortner-NRW/RailHub8266-Firmware) | Version control, issue tracking via GitHub. |

## Conventions

### Coding Conventions

| Convention | Description | Rationale |
|------------|-------------|-----------|
| **C++ Style** | Arduino C++ dialect (C++11 compatible) | Required by ESP8266 Arduino core. |
| **Naming** | CamelCase for functions, snake_case for variables (not enforced) | Inconsistent in current codebase (legacy). |
| **Indentation** | 4 spaces (appears to be used in main.cpp) | Standard PlatformIO default. |
| **Comments** | English language, descriptive block comments | Improves maintainability for international contributors. |

### Architectural Conventions

| Convention | Description | Rationale |
|------------|-------------|-----------|
| **Single File Architecture** | All code in `src/main.cpp` (~1400 lines) | Simplifies build, acceptable for ~1500 LOC project. |
| **Global State** | Output states, chasing groups stored as global variables | Reduces stack pressure, simplifies state access. |
| **Synchronous API** | No callbacks, no RTOS tasks | ESP8266 single-core limitation, simplifies logic. |
| **EEPROM-First Persistence** | All state written to EEPROM immediately on change | Ensures config survives unexpected power loss. |

## Assumptions on Development Environment

1. **Build Environment**: PlatformIO Core 6.x or later, with `espressif8266` platform installed.
2. **Serial Monitor**: 115200 baud rate for debug output.
3. **Upload Protocol**: UART (USB-serial) or OTA (if custom implemented).
4. **Testing**: Unity test framework available (environment `esp12e_test`), but `test/` directory is empty (no tests implemented).

## External Interfaces and Constraints

### Network Constraints

| Constraint | Description | Impact |
|------------|-------------|--------|
| **WiFi 2.4GHz Only** | ESP8266 does not support 5GHz WiFi | Deployment limited to 2.4GHz networks. |
| **No WPA3** | ESP8266 WiFi stack supports WPA2 only | Cannot connect to WPA3-only networks. |
| **mDNS Responder** | Uses ESP8266mDNS for local discovery | Requires mDNS/Bonjour-compatible network (most home routers support). |
| **HTTP Port 80** | Web server listens on port 80 (hardcoded) | Requires port 80 available (conflicts with other web servers on same IP). |
| **WebSocket Port 81** | WebSocket server on port 81 (hardcoded) | Firewall rules must allow port 81. |

### Hardware Interface Constraints

| Constraint | Description | Impact |
|------------|-------------|--------|
| **GPIO 0 Boot Pin** | GPIO 0 must be HIGH on boot (or device enters flash mode) | Cannot use GPIO 0 for active-LOW outputs during boot. Reserved for button. |
| **GPIO 2 Boot Pin** | GPIO 2 affects boot mode (must be HIGH or floating) | Can use for output, but avoid pull-down resistors. |
| **No ADC for Outputs** | ESP8266 has one ADC pin (A0), not used in this design | No analog input capability for feedback/sensors. |

## Technology Decisions

The following technology choices are fixed:

| Technology | Decision | Alternative Considered | Rationale |
|------------|----------|------------------------|-----------|
| **Microcontroller** | ESP8266 (ESP-12E) | STM32, Arduino + WiFi Module | Low cost (~$2), WiFi built-in, large community, mature ecosystem. |
| **Language** | C++ (Arduino dialect) | MicroPython, Rust | Best library support, lowest RAM overhead. |
| **Build System** | PlatformIO | Arduino IDE, ESP-IDF | Professional tooling, dependency management, multi-environment support. |
| **Web Server** | ESP8266WebServer (built-in) | AsyncWebServer | AsyncWebServer uses more RAM, synchronous is acceptable for <5 clients. |
| **WebSocket Library** | links2004/WebSockets | AsyncWebSocket | Synchronous approach saves RAM, acceptable latency. |
| **Persistence** | EEPROM (512 bytes) | SPIFFS, LittleFS | EEPROM is simplest, fastest for small config (<500 bytes). |
| **WiFi Config** | WiFiManager 2.0.17 | Custom portal, WPS | Mature library, good UX, captive portal. |

---

**Next**: [3. System Scope and Context](03_system_scope_and_context.md)
