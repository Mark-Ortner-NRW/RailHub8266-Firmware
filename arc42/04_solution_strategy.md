# 4. Solution Strategy

This section summarizes the fundamental decisions and solution approaches that shape the architecture of RailHub8266.

## Technology Decisions

### Core Platform

**Decision**: Use ESP8266 with Arduino framework via PlatformIO

**Rationale**:
- **Low Cost**: ESP8266 modules cost $2-5, making WiFi control accessible to hobbyists
- **WiFi Built-in**: No external network module required
- **Large Community**: Extensive Arduino ESP8266 library ecosystem
- **Sufficient Performance**: 80MHz CPU adequate for 7 PWM outputs + web server

**Trade-offs**:
- Limited RAM (80KB) requires careful memory management
- Single-core architecture requires efficient task scheduling
- Seven GPIO pins limit maximum output count (expandable via multiple units)

### Persistence Strategy

**Decision**: Use EEPROM for configuration storage

**Rationale**:
- **Simplicity**: Single 512-byte structure, no filesystem overhead
- **Speed**: Faster than SPIFFS/LittleFS for small data (<1KB)
- **Reliability**: No corruption from partial writes (atomic commit)

**Trade-offs**:
- Limited storage capacity (512 bytes total)
- EEPROM has ~100K write cycle limit (acceptable for infrequent config changes)

**Alternative Considered**: SPIFFS/LittleFS filesystem
- Rejected due to higher RAM overhead (~4KB) and slower access

### Web Interface Strategy

**Decision**: Server-side rendered HTML with chunked delivery

**Rationale**:
- **Memory Efficiency**: Sending HTML in chunks avoids large string allocations
- **No Framework Overhead**: Vanilla JavaScript, no React/Vue/Angular
- **Progressive Loading**: User sees content before full page loads

**Trade-offs**:
- Larger code size (inline HTML/CSS/JS in C++ strings)
- Less maintainable than separate web assets

**Alternative Considered**: SPIFFS-hosted HTML/CSS/JS files
- Rejected due to SPIFFS RAM overhead and slower file access

### Real-Time Updates Strategy

**Decision**: WebSocket broadcast every 500ms

**Rationale**:
- **Low Latency**: Sub-second updates for responsive UI
- **Efficient**: Single broadcast to all clients (vs. HTTP polling)
- **Bidirectional**: Allows future client→server commands

**Trade-offs**:
- Requires WebSocket library (~8KB code, ~2KB RAM per client)
- Limited to 2-3 concurrent clients due to RAM constraints

**Alternative Considered**: HTTP polling (long polling)
- Rejected due to higher overhead (HTTP headers per request)

## Decomposition Strategy

### Single-File Architecture

**Decision**: All code in `src/main.cpp` (~1400 lines)

**Rationale**:
- **Simplicity**: No header file management, easier builds
- **Small Codebase**: ~1400 LOC is manageable in single file
- **Faster Compilation**: No multi-file dependency resolution

**Trade-offs**:
- Harder to navigate as code grows beyond 2000 LOC
- No logical separation of concerns (web server, PWM, EEPROM)

**Threshold for Refactoring**: If codebase exceeds 2000 LOC, split into modules:
- `web_server.cpp` - HTTP endpoints, WebSocket
- `pwm_controller.cpp` - PWM logic, chasing groups, blink intervals
- `storage.cpp` - EEPROM read/write

### Global State Management

**Decision**: Use global variables for output states, chasing groups

**Rationale**:
- **Memory Efficiency**: No heap allocations, predictable RAM usage
- **Fast Access**: Direct variable access (no pointer dereferencing)
- **Simple Lifecycle**: No allocation/deallocation logic

**Trade-offs**:
- Harder to unit test (tightly coupled state)
- Risk of accidental mutation (no encapsulation)

**Best Practice**: Mark globals as `const` where possible, use `static` for file-scoped variables

## Quality Attribute Strategies

### Reliability

**Strategy**: Defensive programming + state persistence

**Measures**:
1. **EEPROM Write-on-Change**: Every config change immediately written to EEPROM
2. **Input Validation**: All API requests validated before processing (pin ranges, brightness 0-100)
3. **WiFi Auto-Reconnect**: WiFiManager handles reconnection after network loss
4. **WebSocket Auto-Reconnect**: Client-side reconnection logic (2s delay)
5. **Watchdog Timer**: ESP8266 built-in WDT resets device if main loop hangs

**Example**: API request validation
```cpp
if (brightnessPercent < 0 || brightnessPercent > 100) {
    Serial.println("[ERROR] Invalid brightness");
    brightnessPercent = constrain(brightnessPercent, 0, 100);
}
```

### Performance

**Strategy**: Minimize dynamic allocations + yield() calls

**Measures**:
1. **Static JSON Documents**: Use `DynamicJsonDocument doc(2048)` with fixed size
2. **Chunked HTML**: Send HTML in small chunks (`server->sendContent(F("..."))`
3. **Yield() in Loops**: Call `yield()` in main loop to prevent WDT resets
4. **Throttled Broadcasts**: WebSocket updates limited to 500ms interval

**Example**: Chunked HTML delivery
```cpp
server->setContentLength(CONTENT_LENGTH_UNKNOWN);
server->send(200, "text/html", "");
server->sendContent(F("<html>..."));  // Chunk 1
server->sendContent(F("<style>..."));  // Chunk 2
server->sendContent("");  // End chunked transfer
```

### Usability

**Strategy**: Zero-configuration first boot + mDNS discovery

**Measures**:
1. **WiFiManager Portal**: On first boot, creates `RailHub8266-Setup` AP → user configures WiFi
2. **mDNS Hostname**: Device accessible via `http://railhub8266.local` (no IP lookup)
3. **Persistent Names**: Custom output/group names saved to EEPROM
4. **Multilingual UI**: JavaScript i18n with 6 languages (English, German, French, Italian, Chinese, Hindi)
5. **Visual Feedback**: Status LED (GPIO 2) indicates WiFi state (blink = config mode, solid = connected)

## Architectural Patterns

### Main Loop Pattern

**Pattern**: Superloop with cooperative multitasking

**Implementation**:
```cpp
void loop() {
    checkConfigPortalTrigger();   // Priority 1: Config button
    server->handleClient();       // Priority 2: HTTP requests
    ws->loop();                   // Priority 3: WebSocket events
    MDNS.update();                // Priority 4: mDNS responder
    updateChasingLightGroups();   // Priority 5: Chasing effects
    updateBlinkingOutputs();      // Priority 6: Blink intervals
    yield();                      // Yield to WiFi stack
}
```

**Rationale**: No RTOS overhead, simple execution model

### Observer Pattern (WebSocket Broadcast)

**Pattern**: Broadcast state changes to all connected clients

**Implementation**:
```cpp
void executeOutputCommand(...) {
    // 1. Update local state
    outputStates[index] = active;
    
    // 2. Apply to hardware
    analogWrite(pin, brightness);
    
    // 3. Persist to EEPROM
    saveOutputState(index);
    
    // 4. Notify all observers (WebSocket clients)
    broadcastStatus();
}
```

**Rationale**: Decouples control logic from UI updates

### Repository Pattern (EEPROM)

**Pattern**: Encapsulate EEPROM access in save/load functions

**Implementation**:
```cpp
void saveOutputState(int index) {
    EEPROM.get(0, eepromData);            // Read current
    eepromData.outputStates[index] = ...;  // Update field
    EEPROM.put(0, eepromData);            // Write back
    EEPROM.commit();                      // Flush to flash
}
```

**Rationale**: Centralizes EEPROM logic, reduces code duplication

## Cross-Cutting Concepts

See [Section 8: Crosscutting Concepts](08_crosscutting_concepts.md) for:
- Error handling strategy
- Logging strategy
- Memory management
- Security approach

## Key Architecture Decisions

Detailed architecture decisions are documented in [Section 9: Architecture Decisions](09_architecture_decisions.md).

Summary of critical ADRs:

| ADR | Decision | Rationale |
|-----|----------|-----------|
| ADR-001 | Use ESP8266 platform | Low cost, WiFi built-in, sufficient for 7 outputs |
| ADR-002 | EEPROM over SPIFFS | Faster, simpler for small config |
| ADR-003 | Single-file architecture | Manageable for <2000 LOC |
| ADR-004 | Chunked HTML delivery | Avoids large memory allocations |
| ADR-005 | WebSocket for real-time updates | Lower overhead than HTTP polling |
| ADR-006 | 500ms WebSocket broadcast interval | Balance between responsiveness and RAM usage |
| ADR-007 | Global state variables | Reduces heap fragmentation, faster access |
| ADR-008 | Chasing groups feature | Enables sequential lighting effects for model railways |

---

**Next**: [5. Building Block View](05_building_block_view.md)
