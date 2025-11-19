# 9. Architecture Decisions

This section documents important architecture decisions in a lightweight ADR (Architecture Decision Record) format.

## ADR-001: Use ESP8266 Platform

**Status**: Accepted

**Context**:
The firmware requires a WiFi-enabled microcontroller platform with PWM capabilities. The ESP8266 provides an optimal balance of cost, availability, and features for model railway lighting control applications.

**Decision**:
Use ESP8266 (ESP-12E, NodeMCU, Wemos D1 Mini) as the target platform.

**Rationale**:
- **Cost**: ESP8266 modules cost just $2-5, highly accessible
- **Availability**: ESP8266 has large user base and extensive tutorials
- **Sufficient Performance**: 7 PWM outputs + web server fits within ESP8266 capabilities
- **WiFi Built-in**: No external network module required
- **Proven Platform**: Mature ecosystem with 10+ years of community support

**Consequences**:
- ✅ Lower hardware cost for end users
- ✅ Proven, mature Arduino ESP8266 core
- ✅ Low power consumption for always-on applications
- ❌ Limited to 7 outputs (expandable via multiple units)
- ❌ 80KB RAM requires careful memory management
- ❌ Single-core architecture requires efficient task scheduling

**Alternatives Considered**:
- **STM32**: Rejected due to lack of built-in WiFi
- **Arduino + WiFi Module**: Rejected due to complexity (multiple chips required)
- **Raspberry Pi Pico W**: Rejected due to higher power consumption

---

## ADR-002: EEPROM Over SPIFFS/LittleFS

**Status**: Accepted

**Context**:
Configuration persistence can be implemented using:
1. EEPROM emulation (512 bytes in flash)
2. SPIFFS/LittleFS filesystem (JSON files)

**Decision**:
Use EEPROM emulation for configuration storage.

**Rationale**:
- **Simplicity**: Single struct read/write, no filesystem overhead
- **Speed**: Faster access than file I/O (~10ms vs. ~50ms)
- **RAM Efficiency**: No filesystem cache required (~4KB saved)
- **Sufficient Size**: 512 bytes accommodates all config (device name, 7 outputs, 4 chasing groups)
- **Atomic Writes**: `EEPROM.commit()` ensures consistency

**Consequences**:
- ✅ Faster boot time (no filesystem mount)
- ✅ Lower RAM usage (~4KB saved)
- ✅ Simpler code (no file path management)
- ❌ Limited to 512 bytes total config
- ❌ EEPROM wear (~100K write cycles, acceptable for infrequent config changes)

**Alternatives Considered**:
- **SPIFFS**: Rejected due to RAM overhead (~4KB) and slower access
- **LittleFS**: Rejected for same reasons as SPIFFS
- **External I2C EEPROM**: Rejected due to added hardware cost and complexity

---

## ADR-003: Single-File Architecture

**Status**: Accepted (with conditions)

**Context**:
The firmware could be structured as:
1. Single `main.cpp` file (~1400 LOC)
2. Multiple files (e.g., `web_server.cpp`, `pwm_controller.cpp`, `storage.cpp`)

**Decision**:
Use single-file architecture for current codebase (<2000 LOC).

**Rationale**:
- **Simplicity**: No header file management, easier builds
- **Small Codebase**: ~1400 LOC is manageable in single file
- **Faster Compilation**: No multi-file dependency resolution
- **Easier Debugging**: All code visible in one file

**Consequences**:
- ✅ Simpler project structure for small codebase
- ✅ Faster incremental builds
- ❌ Harder to navigate as code grows
- ❌ No logical separation of concerns

**Threshold for Refactoring**:
If codebase exceeds 2000 LOC, split into modules:
- `web_server.cpp` - HTTP endpoints, WebSocket
- `pwm_controller.cpp` - PWM logic, chasing groups, blink intervals
- `storage.cpp` - EEPROM read/write

**Alternatives Considered**:
- **Multi-file from start**: Rejected due to small codebase (~1400 LOC)

---

## ADR-004: Chunked HTML Delivery

**Status**: Accepted

**Context**:
The web UI can be served via:
1. Single large string allocation (~40KB)
2. Chunked transfer encoding (small chunks)
3. SPIFFS-hosted HTML/CSS/JS files

**Decision**:
Use chunked HTTP transfer encoding to deliver HTML in small chunks.

**Rationale**:
- **Memory Efficiency**: Avoids 40KB heap allocation (critical on 80KB RAM)
- **Progressive Rendering**: User sees content before full page loads
- **No Filesystem**: No SPIFFS overhead (~4KB RAM saved)

**Consequences**:
- ✅ Significantly lower RAM usage (~35KB saved during page load)
- ✅ Faster time-to-first-byte
- ❌ Larger code size (HTML/CSS/JS inline in C++ strings)
- ❌ Less maintainable (web assets embedded in C++)

**Example**:
```cpp
server->setContentLength(CONTENT_LENGTH_UNKNOWN);
server->send(200, "text/html", "");
server->sendContent(F("<html><head>..."));  // Chunk 1
server->sendContent(F("<style>..."));       // Chunk 2
server->sendContent(F("<script>..."));      // Chunk 3
server->sendContent("");  // End chunked transfer
```

**Alternatives Considered**:
- **Single string**: Rejected due to RAM constraints
- **SPIFFS files**: Rejected due to RAM overhead and slower access

---

## ADR-005: WebSocket for Real-Time Updates

**Status**: Accepted

**Context**:
Real-time status updates can be implemented via:
1. HTTP polling (client requests `/api/status` every N seconds)
2. HTTP long polling (client waits for status change)
3. WebSocket (bidirectional persistent connection)

**Decision**:
Use WebSocket server on port 81 with 500ms broadcast interval.

**Rationale**:
- **Low Latency**: Sub-second updates (<100ms)
- **Efficient**: Single broadcast to all clients (vs. HTTP headers per request)
- **Bidirectional**: Enables future client→server commands
- **Standard**: Widely supported by browsers and home automation systems

**Consequences**:
- ✅ Responsive UI (500ms update rate)
- ✅ Lower network overhead (no HTTP headers per update)
- ✅ Scales to multiple clients (2-3 max due to RAM)
- ❌ Requires WebSocket library (~8KB code, ~2KB RAM per client)
- ❌ Limited client count (2-3 max to avoid RAM exhaustion)

**Alternatives Considered**:
- **HTTP Polling**: Rejected due to higher overhead (HTTP headers per request)
- **Server-Sent Events (SSE)**: Rejected due to unidirectional nature

---

## ADR-006: 500ms WebSocket Broadcast Interval

**Status**: Accepted

**Context**:
WebSocket broadcast interval can range from 100ms (10 Hz) to 2000ms (0.5 Hz).

**Decision**:
Set broadcast interval to 500ms (2 Hz).

**Rationale**:
- **Responsive UI**: 500ms is fast enough for human perception (threshold ~100-200ms)
- **Bandwidth**: ~2KB/s per client (acceptable for WiFi)
- **CPU Usage**: ~20% of CPU time for JSON serialization (measured)
- **Battery Life**: Lower frequency helps for mobile clients

**Consequences**:
- ✅ Good balance between responsiveness and resource usage
- ✅ Low WiFi bandwidth (~16 Kbps per client)
- ❌ Not suitable for sub-100ms control loops (not a requirement)

**Alternatives Considered**:
- **100ms**: Rejected due to higher CPU usage (~50%)
- **1000ms**: Rejected as too slow for manual control

---

## ADR-007: Global State Variables

**Status**: Accepted

**Context**:
Output states can be stored as:
1. Global variables (direct access)
2. Class members (encapsulated state)
3. Heap-allocated objects (dynamic allocation)

**Decision**:
Use global variables for output states, chasing groups, and configuration.

**Rationale**:
- **Memory Efficiency**: No heap allocations, predictable RAM usage
- **Fast Access**: Direct variable access (no pointer dereferencing)
- **Simple Lifecycle**: No allocation/deallocation logic
- **Arduino Convention**: Common pattern in Arduino projects

**Consequences**:
- ✅ Lower RAM usage (no heap fragmentation)
- ✅ Faster access (direct variable addressing)
- ❌ Harder to unit test (tightly coupled state)
- ❌ Risk of accidental mutation (no encapsulation)

**Best Practices**:
- Use `const` for read-only globals
- Use `static` for file-scoped variables
- Document global state clearly

**Alternatives Considered**:
- **Class Members**: Rejected due to negligible benefit for small codebase
- **Heap Allocation**: Rejected due to RAM constraints

---

## ADR-008: Chasing Groups Feature

**Status**: Accepted

**Context**:
Chasing light groups enable sequential lighting effects, a popular feature for model railway scenes, Christmas decorations, and ambient lighting displays.

**Decision**:
Implement chasing groups as a core feature of RailHub8266.

**Rationale**:
- **User Demand**: Sequential lighting is highly requested for model railways
- **Unique Value**: Differentiates from basic PWM controllers
- **Implementation Feasibility**: Fits within ESP8266 memory constraints
- **Practical Application**: Essential for realistic railway station and signal lighting

**Consequences**:
- ✅ Unique selling point for model railway enthusiasts
- ✅ Enables creative lighting effects without external controllers
- ✅ Minimal memory overhead (state machine approach)
- ❌ Increases code complexity slightly

**Notes**:
Chasing groups support up to 4 simultaneous sequences with configurable intervals.

---

## ADR-009: No User Authentication

**Status**: Accepted (with caveats)

**Context**:
The web interface could require:
1. No authentication (open access)
2. HTTP Basic Auth (username/password)
3. Custom login page with sessions

**Decision**:
No user authentication in default firmware.

**Rationale**:
- **Target Deployment**: Trusted local networks (home, hobby installations)
- **RAM Constraints**: Session management requires additional RAM
- **Simplicity**: Reduces complexity for non-technical users
- **Network Security**: WPA2 WiFi password provides first-layer protection

**Consequences**:
- ✅ Simpler setup (no credentials to manage)
- ✅ Lower RAM usage
- ❌ **HIGH RISK**: Open access on local network
- ❌ Not suitable for multi-user environments

**Recommendations**:
For deployments requiring authentication:
1. Network isolation (separate VLAN for IoT devices)
2. VPN access instead of direct exposure
3. Custom firmware with HTTP Basic Auth (requires code modification)

**Alternatives Considered**:
- **HTTP Basic Auth**: Rejected due to RAM overhead and poor UX (browser password prompts)
- **Custom Login Page**: Rejected due to significant development effort

---

## ADR-010: No HTTPS/TLS

**Status**: Accepted (with caveats)

**Context**:
The web interface could use:
1. HTTP (unencrypted)
2. HTTPS with TLS (encrypted)

**Decision**:
No HTTPS/TLS support.

**Rationale**:
- **RAM Constraints**: TLS handshake requires ~30KB RAM (unfeasible on 80KB total)
- **CPU Overhead**: TLS encryption/decryption on 80MHz CPU is slow
- **Certificate Management**: Self-signed certs cause browser warnings
- **Target Deployment**: Trusted local networks only

**Consequences**:
- ✅ Significantly lower RAM usage (~30KB saved)
- ✅ Faster page loads (no TLS overhead)
- ❌ **CRITICAL RISK**: Unencrypted WiFi traffic (credentials, commands visible)
- ❌ Not suitable for public internet exposure

**Mitigation**:
- Deploy on WPA2-secured WiFi only
- Use network isolation (separate VLAN)
- Never expose to public internet (no port forwarding)

**Alternatives Considered**:
- **HTTPS**: Rejected due to RAM constraints (requires ~30KB for TLS handshake)
- **VPN Tunnel**: User responsibility (router-level solution recommended)
- **Hardware Security Module**: Rejected as overkill for hobbyist application

---

## Summary Table

| ADR | Decision | Rationale | Trade-offs |
|-----|----------|-----------|------------|
| ADR-001 | ESP8266 platform | Low cost, sufficient performance | Limited GPIO, RAM constraints |
| ADR-002 | EEPROM persistence | Simple, fast, low RAM | 512-byte limit, wear cycles |
| ADR-003 | Single-file architecture | Simplicity for <2000 LOC | Harder to navigate as code grows |
| ADR-004 | Chunked HTML delivery | Avoids large heap allocations | Larger code size, less maintainable |
| ADR-005 | WebSocket updates | Low latency, efficient | RAM cost (~2KB/client) |
| ADR-006 | 500ms broadcast interval | Good balance responsiveness/resources | Not suitable for <100ms control |
| ADR-007 | Global state variables | Low RAM, fast access | Harder to test, no encapsulation |
| ADR-008 | Chasing groups feature | User demand, unique value | Increased code complexity |
| ADR-009 | No authentication | Simplicity, low RAM | **HIGH RISK** in untrusted networks |
| ADR-010 | No HTTPS/TLS | RAM constraints | **CRITICAL RISK** unencrypted traffic |

---

**Next**: [10. Quality Requirements](10_quality_requirements.md)
