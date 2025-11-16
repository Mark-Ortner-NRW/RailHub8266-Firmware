# 12. Glossary

This section defines important domain and technical terms used throughout the RailHub8266 architecture documentation.

## Domain Terms

### Model Railway
A scale model of a railway system, often used as a hobby or educational tool. RailHub8266 is designed to control lighting and accessories in model railway layouts.

### Chasing Lights
A sequential lighting effect where outputs activate in sequence, creating a "chasing" or "running" visual pattern. Commonly used for runway lights, advertising signs, or decorative effects.

**Example**: Outputs 1 → 2 → 3 → 4 → 1 (repeat) with 300ms interval between steps.

### PWM (Pulse Width Modulation)
A technique for controlling the average power delivered to a load by varying the duty cycle of a digital signal. Used in RailHub8266 to control LED brightness (0% = off, 100% = full brightness).

**Characteristics**:
- Frequency: 1kHz (1000 cycles per second)
- Resolution: 8-bit (256 levels: 0-255)
- API: `analogWrite(pin, value)` on ESP8266

### Blink Interval
Time period between output toggles (ON ↔ OFF) for creating blinking effects. Set independently per output (0 = disabled, 1-65535ms).

**Example**: Interval of 500ms creates 1 Hz blink rate (ON 500ms, OFF 500ms, repeat).

---

## Hardware Terms

### ESP8266
Low-cost WiFi microcontroller (SoC) from Espressif Systems. Features 80MHz CPU, 80KB RAM, built-in 2.4GHz WiFi, and GPIO pins for interfacing with external devices.

**Variants**:
- **ESP-12E**: Bare module requiring USB-UART adapter
- **NodeMCU**: Development board with USB-UART built-in
- **Wemos D1 Mini**: Compact development board

### GPIO (General-Purpose Input/Output)
Configurable digital pins that can be set as inputs or outputs. RailHub8266 uses 7 GPIO pins as PWM outputs (4, 5, 12, 13, 14, 16, 2).

### EEPROM (Electrically Erasable Programmable Read-Only Memory)
Non-volatile memory that retains data when power is removed. ESP8266 emulates EEPROM in flash memory. Used for storing configuration (output states, chasing groups, device name).

**Characteristics**:
- Size: 512 bytes allocated
- Write endurance: ~100,000 cycles
- Access time: ~10-20ms per write

### UART (Universal Asynchronous Receiver-Transmitter)
Serial communication protocol used for debug logging and firmware uploads. RailHub8266 uses 115200 baud rate.

### MOSFET (Metal-Oxide-Semiconductor Field-Effect Transistor)
Electronic switch used to control high-current loads (>12mA) from low-current GPIO pins. Recommended for driving LED strips, motors, or relays.

**Example**: IRLZ44N logic-level N-channel MOSFET (commonly used with ESP8266).

---

## Software Terms

### Arduino Framework
C++ development framework for microcontrollers. Provides high-level APIs (`analogWrite()`, `Serial.println()`) abstracting hardware details. RailHub8266 uses Arduino core for ESP8266.

### PlatformIO
Cross-platform build system and IDE for embedded development. Manages libraries, compiles firmware, and uploads to devices. Alternative to Arduino IDE.

**Configuration File**: `platformio.ini`

### WiFiManager
Arduino library (2.0.17) providing captive portal for WiFi configuration. Automatically creates AP and web interface for entering WiFi credentials.

**Captive Portal SSID**: `RailHub8266-Setup`  
**Portal IP**: `192.168.4.1`

### WebSocket
Full-duplex communication protocol over TCP. Enables real-time bidirectional data transfer between server (ESP8266) and clients (browsers). Used for broadcasting status updates every 500ms.

**Protocol**: `ws://` (not encrypted, `wss://` not supported on ESP8266)

### JSON (JavaScript Object Notation)
Lightweight data interchange format. Used for API requests/responses and WebSocket messages.

**Library**: ArduinoJson 7.0.4

### mDNS (Multicast DNS)
Zero-configuration networking protocol for local hostname resolution. Allows accessing device via `railhub8266.local` instead of IP address.

**Also Known As**: Bonjour (Apple), Avahi (Linux)

### REST API (Representational State Transfer)
Architectural style for web services using HTTP methods (GET, POST). RailHub8266 provides REST endpoints for output control, configuration, and status.

**Base URL**: `http://<device-ip>/api/`

---

## Network Terms

### STA Mode (Station Mode)
WiFi mode where ESP8266 connects to existing WiFi network as a client (like a laptop or phone). Default operational mode after configuration.

### AP Mode (Access Point Mode)
WiFi mode where ESP8266 creates its own WiFi network. Used during initial setup or when saved credentials fail.

**AP SSID**: `RailHub8266-Setup`

### DHCP (Dynamic Host Configuration Protocol)
Network protocol for automatic IP address assignment. ESP8266 requests IP from router's DHCP server on connection.

### WPA2 (WiFi Protected Access 2)
WiFi security protocol using encryption. ESP8266 supports WPA2-PSK (pre-shared key). WPA3 not supported.

### SSID (Service Set Identifier)
WiFi network name. ESP8266 scans for available SSIDs during WiFiManager portal.

### VLAN (Virtual Local Area Network)
Network segmentation technique. Recommended for isolating IoT devices like RailHub8266 from main network.

---

## Architecture Terms

### Chunked Transfer Encoding
HTTP feature allowing server to send response in multiple chunks instead of single large buffer. Used to deliver HTML without allocating 40KB in RAM.

**Example**:
```http
Transfer-Encoding: chunked
```

### Watchdog Timer (WDT)
Hardware timer that resets device if main loop hangs. ESP8266 has built-in WDT (timeout ~8 seconds). Calling `yield()` resets the timer.

### Superloop
Programming pattern where main loop continuously executes tasks sequentially (no RTOS threads). Used in `loop()` function.

```cpp
void loop() {
    task1();
    task2();
    task3();
    yield();  // Reset WDT
}
```

### Flash String Macro (`F()`)
Arduino macro that stores string literals in flash memory instead of RAM. Critical for memory-constrained ESP8266.

**Example**:
```cpp
Serial.println(F("This string lives in flash, not RAM"));
```

---

## API Terms

### Endpoint
URL path for accessing specific API function. RailHub8266 provides 9 endpoints.

**Example**: `/api/control` (POST) - Control output state/brightness

### Request Body
Data sent with HTTP POST request, typically in JSON format.

**Example**:
```json
{
  "pin": 4,
  "active": true,
  "brightness": 75
}
```

### Response Code
HTTP status code indicating result of request.

**Common Codes**:
- `200 OK` - Success
- `400 Bad Request` - Invalid JSON or parameters
- `404 Not Found` - Invalid GPIO pin
- `500 Internal Server Error` - Unexpected error

---

## Configuration Terms

### Device Name
Custom hostname for the device. Used for mDNS (`<device-name>.local`) and web UI display.

**Default**: `ESP8266-Controller-01`  
**Customizable**: Via WiFiManager portal

### Output Name
User-defined label for an output (max 20 characters).

**Example**: "Platform Lights" instead of "GPIO 4"

### Chasing Group
Collection of outputs activated sequentially at fixed interval. Up to 4 groups supported.

**Properties**:
- Group ID (1-255)
- Name (max 20 characters)
- Outputs (2-8 GPIOs)
- Interval (≥50ms)

---

## Measurement Units

### Millisecond (ms)
1/1000th of a second. Used for timing intervals (blink intervals, chasing steps, WebSocket broadcast).

**Example**: 500ms = 0.5 seconds

### Kilobyte (KB)
1024 bytes. Used for memory measurements (RAM, flash).

**Example**: 80KB RAM = 81,920 bytes

### Megabyte (MB)
1024 kilobytes. Used for flash storage size.

**Example**: 4MB flash = 4,194,304 bytes

### Baud Rate
Symbols per second in serial communication. RailHub8266 uses 115200 baud for debug output.

### Hertz (Hz)
Cycles per second. Used for frequency measurements.

**Example**: PWM frequency = 1kHz = 1000 cycles/second

---

## Abbreviations

| Abbreviation | Full Term |
|--------------|-----------|
| **ADR** | Architecture Decision Record |
| **AP** | Access Point |
| **API** | Application Programming Interface |
| **CPU** | Central Processing Unit |
| **DHCP** | Dynamic Host Configuration Protocol |
| **DNS** | Domain Name System |
| **EEPROM** | Electrically Erasable Programmable Read-Only Memory |
| **GPIO** | General-Purpose Input/Output |
| **HTTP** | HyperText Transfer Protocol |
| **HTTPS** | HTTP Secure (with TLS) |
| **i18n** | Internationalization |
| **IDE** | Integrated Development Environment |
| **IoT** | Internet of Things |
| **IP** | Internet Protocol |
| **JSON** | JavaScript Object Notation |
| **LED** | Light-Emitting Diode |
| **LOC** | Lines of Code |
| **MAC** | Media Access Control (address) |
| **mDNS** | Multicast DNS |
| **MOSFET** | Metal-Oxide-Semiconductor Field-Effect Transistor |
| **OTA** | Over-The-Air (firmware update) |
| **PWM** | Pulse Width Modulation |
| **RAM** | Random Access Memory |
| **REST** | Representational State Transfer |
| **RTOS** | Real-Time Operating System |
| **SoC** | System on Chip |
| **SPIFFS** | Serial Peripheral Interface Flash File System |
| **SSID** | Service Set Identifier (WiFi network name) |
| **STA** | Station (WiFi client mode) |
| **TCP** | Transmission Control Protocol |
| **TLS** | Transport Layer Security |
| **UART** | Universal Asynchronous Receiver-Transmitter |
| **UI** | User Interface |
| **URL** | Uniform Resource Locator |
| **USB** | Universal Serial Bus |
| **VLAN** | Virtual Local Area Network |
| **WDT** | Watchdog Timer |
| **WiFi** | Wireless Fidelity (IEEE 802.11) |
| **WPA** | WiFi Protected Access |
| **WS** | WebSocket |

---

## Acronym Conventions

Throughout this documentation:
- **ESP8266**: Refers to the entire SoC family (ESP-12E, NodeMCU, Wemos D1 Mini)
- **RailHub8266**: The firmware/project name
- **Main loop**: The `loop()` function in Arduino framework
- **Chasing groups**: Sequential lighting effects (RailHub8266-specific feature)

---

## Related Terms

| Term | See Also |
|------|----------|
| **Brightness** | PWM, analogWrite() |
| **Captive Portal** | WiFiManager, AP Mode |
| **Flash Memory** | EEPROM, SPIFFS |
| **Heap** | RAM, Memory Management |
| **LED Strip** | MOSFET, PWM |
| **Network Isolation** | VLAN, Security |
| **Portal Trigger** | GPIO 0, Boot Button |
| **Real-Time Updates** | WebSocket, Broadcast |
| **Status LED** | GPIO 2, Built-in LED |
| **Web Interface** | HTTP Server, REST API |

---

**End of arc42 Documentation**

---

## Document Information

- **Project**: RailHub8266 ESP8266 Model Railway Controller
- **Documentation Template**: arc42 v8.0
- **Documentation Language**: English
- **Last Updated**: November 16, 2025
- **Version**: Based on firmware v2.0.0
- **Repository**: Mark-Ortner-NRW/RailHub8266-Firmware
- **Contact**: mark_ortner@hotmail.de
