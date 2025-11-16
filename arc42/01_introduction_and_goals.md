# 1. Introduction and Goals

## Overview

**RailHub8266** is an ESP8266-based firmware that transforms low-cost ESP8266 microcontrollers into WiFi-controlled PWM output controllers. The system is specifically designed for model railway automation and decorative lighting control, offering real-time control via WebSocket connections and a responsive web interface.

The firmware is a port of the RailHub32 ESP32 controller, optimized for the ESP8266's memory constraints while adding exclusive features like chasing light groups.

## Requirements Overview

### Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-1 | Control 7 PWM outputs individually (on/off, brightness 0-100%) | Critical |
| FR-2 | Provide real-time status updates via WebSocket (500ms interval) | High |
| FR-3 | Support WiFi connectivity (STA mode + AP fallback) | Critical |
| FR-4 | Persist all configuration and state to non-volatile storage | Critical |
| FR-5 | Offer chasing light groups (up to 4 groups, sequential activation) | High |
| FR-6 | Allow per-output blink intervals (0-65535ms) | Medium |
| FR-7 | Provide REST API for programmatic control | High |
| FR-8 | Serve responsive web interface with 6 language translations | Medium |
| FR-9 | Support WiFiManager captive portal for WiFi configuration | High |
| FR-10 | Enable mDNS service discovery (`railhub8266.local`) | Medium |
| FR-11 | Allow custom naming for outputs and chasing groups | Low |

### Quality Requirements

See [Section 10: Quality Requirements](10_quality_requirements.md) for detailed quality scenarios.

## Stakeholders

| Role | Expectations | Contact |
|------|--------------|---------|
| **Model Railway Enthusiasts** | Easy-to-use lighting control, reliable operation, support for sequential effects | End users |
| **Home Automation Integrators** | REST API access, predictable behavior, documentation | System integrators |
| **Firmware Developers** | Clear code structure, maintainability, extension points | Development team |
| **Product Owner** | Feature completeness, stability, low-cost hardware | mark_ortner@hotmail.de |
| **Hardware Engineers** | GPIO pin compatibility, power consumption constraints | Hardware team |

## Quality Goals

The top three quality goals for this architecture are:

### 1. **Reliability** (Priority: Critical)
- **Target**: 99.9% uptime over 30-day period
- **Rationale**: Model railway installations expect continuous operation
- **Measures**:
  - EEPROM-based state persistence (survive power loss)
  - Automatic WiFi reconnection logic
  - WebSocket auto-reconnect on client side
  - Watchdog timer protection (ESP8266 built-in)

### 2. **Resource Efficiency** (Priority: Critical)
- **Target**: Operate within ESP8266's 80KB RAM, <60% utilization
- **Rationale**: ESP8266 has severe memory constraints (vs. ESP32)
- **Measures**:
  - Chunked HTML delivery (avoid large memory allocations)
  - Limited WebSocket broadcast rate (500ms interval)
  - Minimal dynamic allocations in main loop
  - WiFiManager with reduced options to save RAM

### 3. **Usability** (Priority: High)
- **Target**: Non-technical users can configure and operate the system
- **Rationale**: Target audience includes hobbyists, not just engineers
- **Measures**:
  - WiFiManager captive portal (no serial console needed)
  - Visual web interface with clear status indicators
  - mDNS service (`railhub8266.local` - no IP address lookup)
  - Multilingual UI (6 languages)
  - Persistent custom naming for outputs

## Assumptions

The following assumptions underlie this architecture documentation:

1. **Hardware Platform**: The system targets ESP8266 modules (ESP-12E, NodeMCU, Wemos D1 Mini) with 4MB flash and 80KB RAM.

2. **WiFi Environment**: The device operates in environments with stable 2.4GHz WiFi coverage (ESP8266 does not support 5GHz).

3. **Load Profile**: Each GPIO pin drives low-current loads (<12mA direct, or via MOSFETs/transistors for higher currents).

4. **Network Security**: The system operates on trusted local networks (no TLS/HTTPS due to ESP8266 memory constraints).

5. **User Expertise**: End users have basic familiarity with WiFi networks and web browsers.

6. **Power Supply**: The ESP8266 is powered by stable 3.3V supply (>250mA capacity).

7. **Update Mechanism**: Firmware updates are performed via PlatformIO OTA or USB (not web-based OTA due to memory constraints).

## Success Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| RAM Utilization | <60% of 80KB (~48KB) | `ESP.getFreeHeap()` monitoring |
| Flash Utilization | <50% of 1MB partition | Build output, `/api/status` |
| WebSocket Latency | <100ms round-trip | Client-side measurement |
| State Persistence | 100% config survival across reboots | EEPROM integrity checks |
| WiFi Reconnect Time | <10 seconds after WiFi loss | Serial log analysis |
| Web UI Load Time | <2 seconds initial page load | Browser developer tools |

---

**Next**: [2. Architecture Constraints](02_architecture_constraints.md)
