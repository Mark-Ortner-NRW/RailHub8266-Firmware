# 6. Runtime View

This section describes the runtime behavior of the RailHub8266 system through important scenarios and use cases.

## Scenario 1: System Boot and Initialization

This scenario shows the complete boot sequence from power-on to ready state.

```mermaid
sequenceDiagram
    participant USER as User
    participant ESP as ESP8266
    participant EEPROM as EEPROM
    participant WIFI as WiFi Network
    participant ROUTER as Router/DHCP
    participant MDNS as mDNS

    Note over ESP: Power On / Reset
    
    ESP->>ESP: setup() - Serial.begin(115200)
    ESP->>EEPROM: EEPROM.begin(512)
    ESP->>ESP: initializeOutputs()
    Note over ESP: Configure 7 GPIO pins as PWM outputs<br/>Set frequency to 1kHz, range 0-255
    
    ESP->>EEPROM: loadCustomParameters()
    EEPROM-->>ESP: deviceName: "ESP8266-Controller-01"
    
    ESP->>EEPROM: loadOutputStates()
    EEPROM-->>ESP: outputStates[], outputBrightness[], outputNames[]
    Note over ESP: Restore last known output states
    
    ESP->>EEPROM: loadChasingGroups()
    EEPROM-->>ESP: chasingGroups[] (up to 4 groups)
    
    ESP->>ESP: initializeWiFiManager()
    ESP->>WIFI: WiFi.mode(WIFI_STA)
    ESP->>WIFI: wifiManager.autoConnect("RailHub8266-Setup")
    
    alt Saved WiFi credentials exist
        WIFI->>ROUTER: Connect to saved SSID
        ROUTER-->>WIFI: Assign IP (DHCP)
        WIFI-->>ESP: Connection successful
        ESP->>MDNS: MDNS.begin("esp8266-controller-01")
        MDNS->>MDNS: Register railhub8266.local
        ESP->>ESP: server = new ESP8266WebServer(80)
        ESP->>ESP: ws = new WebSocketsServer(81)
        ESP->>ESP: initializeWebServer()
        Note over ESP: Web server and WebSocket server started
    else No saved credentials
        ESP->>ESP: Create AP "RailHub8266-Setup"
        ESP->>ESP: Start captive portal (192.168.4.1)
        Note over USER: User connects to AP,<br/>configures WiFi via portal
        USER->>ESP: Submit WiFi credentials
        ESP->>EEPROM: Save credentials
        ESP->>ESP: ESP.restart()
        Note over ESP: Reboot to apply new settings
    end
    
    Note over ESP: Enter main loop()
```

### Steps

1. **Power-On Reset**: ESP8266 executes bootloader, loads firmware from flash
2. **Serial Initialization**: Opens serial console at 115200 baud for debug output
3. **EEPROM Initialization**: Allocates 512-byte EEPROM buffer in RAM
4. **GPIO Configuration**: Configures 7 output pins with PWM (1kHz, 8-bit)
5. **Load Persistent State**: Restores device name, output states, chasing groups from EEPROM
6. **WiFi Connection**: Attempts connection using saved credentials
7. **Fallback AP Mode**: If no credentials, creates "RailHub8266-Setup" AP with captive portal
8. **Network Services**: Starts HTTP server (port 80), WebSocket server (port 81), mDNS responder
9. **Ready State**: Enters main loop, begins WebSocket broadcasting (500ms interval)

### Performance

- **Typical Boot Time** (with saved WiFi): 3-5 seconds
- **Typical Boot Time** (first boot, AP mode): 2-3 seconds
- **WiFi Connection Time**: 2-8 seconds (depends on router DHCP)

---

## Scenario 2: Web UI Access and Real-Time Updates

This scenario shows a user opening the web interface and receiving real-time updates.

```mermaid
sequenceDiagram
    participant BROWSER as Web Browser
    participant HTTP as HTTP Server
    participant WS as WebSocket Server
    participant ESP as ESP8266 (main loop)

    BROWSER->>HTTP: GET / HTTP/1.1
    HTTP->>HTTP: initializeWebServer() handler
    HTTP->>BROWSER: HTTP 200 OK (chunked transfer)
    Note over HTTP,BROWSER: Send HTML in chunks:<br/>1. Header + CSS<br/>2. JavaScript<br/>3. i18n translations
    BROWSER->>BROWSER: Parse HTML, render UI
    BROWSER->>BROWSER: Execute JavaScript
    
    BROWSER->>HTTP: GET /api/status
    HTTP->>HTTP: serializeStatusToJson()
    HTTP->>BROWSER: JSON response (system + outputs + groups)
    Note over BROWSER: Display initial system status
    
    BROWSER->>WS: WebSocket handshake (ws://IP:81)
    WS->>WS: wsEvent() - WStype_CONNECTED
    WS-->>BROWSER: WebSocket connection established
    WS->>BROWSER: Immediate status broadcast (JSON)
    Note over BROWSER: Update UI with current state
    
    loop Every 500ms
        ESP->>ESP: loop() - check broadcast interval
        ESP->>WS: broadcastStatus()
        WS->>WS: serializeStatusToJson()
        WS->>BROWSER: WebSocket message (JSON)
        BROWSER->>BROWSER: Update UI elements (outputs, uptime, RAM)
    end
    
    Note over BROWSER: User changes output brightness
    BROWSER->>BROWSER: User drags brightness slider
    BROWSER->>HTTP: POST /api/control {pin, active, brightness}
    HTTP->>HTTP: API handler - executeOutputCommand()
    HTTP->>ESP: Update outputStates[], outputBrightness[]
    ESP->>ESP: analogWrite(pin, PWM value)
    ESP->>ESP: saveOutputState() to EEPROM
    HTTP->>WS: broadcastStatus() (triggered by change)
    HTTP->>BROWSER: HTTP 200 OK {status: "ok"}
    WS->>BROWSER: WebSocket message (updated state)
    BROWSER->>BROWSER: Update UI to reflect change
```

### Steps

1. **Initial Page Load**: Browser requests `/`, receives chunked HTML/CSS/JS
2. **Status Fetch**: JavaScript requests `/api/status`, displays initial state
3. **WebSocket Connection**: Browser opens WebSocket to port 81
4. **Real-Time Broadcast**: WebSocket server sends JSON status every 500ms
5. **User Interaction**: User changes output via UI (e.g., brightness slider)
6. **API Call**: Browser sends POST to `/api/control`
7. **State Update**: ESP updates output, saves to EEPROM, broadcasts to all clients
8. **UI Sync**: Browser receives WebSocket update, refreshes UI

### Performance

- **Initial Page Load**: <2 seconds (chunked delivery, ~40KB HTML)
- **WebSocket Latency**: <100ms round-trip
- **Broadcast Interval**: 500ms (configurable via `BROADCAST_INTERVAL`)

---

## Scenario 3: Creating a Chasing Light Group

This scenario demonstrates creating a sequential chasing effect across 4 outputs.

```mermaid
sequenceDiagram
    participant USER as User (Web UI)
    participant HTTP as HTTP Server
    participant API as API Handler
    participant CHASING as Chasing Groups Engine
    participant GPIO as GPIO Controller
    participant EEPROM as EEPROM
    participant WS as WebSocket Server

    USER->>USER: Select outputs 1, 2, 3, 4 (GPIO 4, 5, 12, 13)
    USER->>USER: Set interval: 300ms
    USER->>USER: Enter group name: "Runway Lights"
    USER->>HTTP: POST /api/chasing/create<br/>{groupId: 1, outputs: [4,5,12,13], interval: 300, name: "Runway Lights"}
    
    HTTP->>API: deserializeJsonRequest()
    API->>API: Validate groupId (1-255) ✓
    API->>API: Validate interval (≥50ms) ✓
    API->>API: Validate outputs count (≥2) ✓
    API->>API: Validate GPIO pins exist ✓
    
    API->>API: Convert pins to indices: [0,1,2,3]
    API->>CHASING: createChasingGroup(groupId=1, indices=[0,1,2,3], interval=300, name="Runway Lights")
    
    CHASING->>CHASING: findGroupSlot(groupId=1)
    Note over CHASING: Find available slot (0-3)
    
    CHASING->>CHASING: Setup ChasingGroup struct
    Note over CHASING: groupId=1, active=true<br/>name="Runway Lights"<br/>outputIndices=[0,1,2,3]<br/>outputCount=4<br/>interval=300ms<br/>currentStep=0
    
    CHASING->>CHASING: Mark outputs as owned by group
    Note over CHASING: outputChasingGroup[0..3] = 1
    
    CHASING->>GPIO: Turn ON output 0 (first in sequence)
    GPIO->>GPIO: analogWrite(4, outputBrightness[0])
    CHASING->>GPIO: Turn OFF outputs 1, 2, 3
    
    CHASING->>EEPROM: saveChasingGroups()
    EEPROM->>EEPROM: EEPROM.put(eepromData)
    EEPROM->>EEPROM: EEPROM.commit()
    
    CHASING-->>HTTP: Success
    HTTP->>WS: broadcastStatus()
    WS->>USER: WebSocket message (updated chasingGroups[])
    HTTP->>USER: HTTP 200 {success: true}
    
    USER->>USER: UI shows new "Runway Lights" group
    
    loop Every 300ms (in main loop)
        Note over CHASING: updateChasingLightGroups()
        CHASING->>CHASING: Check if interval elapsed (300ms)
        CHASING->>GPIO: Turn OFF current output
        CHASING->>CHASING: Advance currentStep: 0→1→2→3→0 (circular)
        CHASING->>GPIO: Turn ON next output
        CHASING->>CHASING: Update lastStepTime
    end
```

### Steps

1. **User Input**: User selects outputs, interval, group name via web UI
2. **API Request**: POST to `/api/chasing/create` with group parameters
3. **Validation**: Validate group ID (1-255), interval (≥50ms), outputs (≥2)
4. **Group Creation**: Create `ChasingGroup` struct, assign outputs
5. **Initial State**: Turn ON first output, turn OFF others
6. **Persistence**: Save group to EEPROM
7. **Broadcast**: WebSocket update notifies all clients
8. **Runtime Loop**: Main loop calls `updateChasingLightGroups()` every iteration
9. **Sequential Activation**: Every 300ms, advance to next output in sequence

### Performance

- **API Response Time**: <50ms (validation + group creation)
- **EEPROM Write Time**: ~10-20ms
- **Step Precision**: ±10ms (depends on main loop timing)

---

## Scenario 4: WiFi Connection Loss and Recovery

This scenario shows automatic reconnection after WiFi network disruption.

```mermaid
sequenceDiagram
    participant ESP as ESP8266
    participant WIFI as WiFi Stack
    participant ROUTER as WiFi Router
    participant WS as WebSocket Server
    participant CLIENTS as Web Clients

    Note over ESP: Normal operation - WiFi connected
    
    ROUTER->>ROUTER: WiFi network disruption<br/>(router reboot, interference)
    WIFI->>ESP: Connection lost event
    Note over ESP: WiFi.isConnected() = false
    
    ESP->>WS: Close all WebSocket connections
    WS->>CLIENTS: Connection closed (TCP FIN)
    CLIENTS->>CLIENTS: Detect WebSocket disconnect
    Note over CLIENTS: JavaScript auto-reconnect logic<br/>setTimeout(connectWS, 2000)
    
    loop Automatic reconnection (built-in WiFiManager)
        ESP->>WIFI: Attempt reconnection
        WIFI->>ROUTER: Probe for SSID
        alt Router back online
            ROUTER-->>WIFI: SSID beacon detected
            WIFI->>ROUTER: Association request
            ROUTER-->>WIFI: DHCP lease
            WIFI-->>ESP: Connection restored
            Note over ESP: WiFi.isConnected() = true
        else Router still offline
            Note over ESP: Retry after 5s
        end
    end
    
    ESP->>ESP: mDNS reannounce (MDNS.update())
    
    CLIENTS->>ESP: Retry WebSocket connection (ws://IP:81)
    ESP->>CLIENTS: WebSocket handshake accepted
    ESP->>WS: broadcastStatus()
    WS->>CLIENTS: JSON status update
    
    Note over ESP,CLIENTS: Normal operation resumed
```

### Steps

1. **Connection Loss**: WiFi network becomes unavailable (router reboot, interference)
2. **WebSocket Disconnect**: All WebSocket clients disconnected (TCP FIN)
3. **Client Retry Logic**: JavaScript `setTimeout()` retries connection every 2s
4. **ESP Reconnection**: WiFi stack automatically retries connection (built-in behavior)
5. **DHCP Renewal**: Router assigns IP (may be same or different)
6. **mDNS Reannounce**: mDNS responder re-registers `railhub8266.local`
7. **WebSocket Reconnect**: Clients successfully reconnect to port 81
8. **State Sync**: Broadcast resumes, clients receive current state

### Performance

- **Typical Reconnection Time**: 5-15 seconds
- **WebSocket Client Retry**: Every 2 seconds (JavaScript `setTimeout`)
- **No State Loss**: All output states persist in EEPROM, restored on reconnect

---

## Scenario 5: Manual WiFi Configuration via Portal

This scenario shows triggering the WiFiManager portal manually.

```mermaid
sequenceDiagram
    participant USER as User
    participant BUTTON as GPIO 0 Button
    participant ESP as ESP8266
    participant WIFI as WiFiManager
    participant BROWSER as Web Browser

    USER->>BUTTON: Press and hold GPIO 0 button
    Note over USER: Hold for 3 seconds
    
    loop Every loop() iteration
        ESP->>BUTTON: digitalRead(GPIO 0)
        alt Button LOW
            ESP->>ESP: portalButtonPressTime = millis()
            ESP->>ESP: Calculate holdDuration
            alt holdDuration > 2500ms
                ESP->>ESP: Serial: "Portal trigger in 0.5s..."
            end
            alt holdDuration > 3000ms
                ESP->>ESP: Serial: "Portal triggered!"
                ESP->>ESP: Blink status LED rapidly (20x)
                ESP->>WIFI: WiFi.disconnect(true) // Erase credentials
                ESP->>ESP: delay(1000)
                ESP->>ESP: ESP.restart()
            end
        else Button HIGH (released early)
            ESP->>ESP: Reset portalButtonPressTime
        end
    end
    
    Note over ESP: Reboot...
    
    ESP->>ESP: setup() - initializeWiFiManager()
    ESP->>WIFI: wifiManager.autoConnect("RailHub8266-Setup", "12345678")
    
    WIFI->>WIFI: No saved credentials found
    WIFI->>WIFI: Create AP "RailHub8266-Setup"
    WIFI->>WIFI: Start DNS server (captive portal)
    WIFI->>ESP: AP Mode active (192.168.4.1)
    
    USER->>USER: Connect device to "RailHub8266-Setup" AP
    BROWSER->>WIFI: HTTP request (any URL)
    WIFI->>BROWSER: Redirect to http://192.168.4.1
    BROWSER->>WIFI: GET http://192.168.4.1
    WIFI->>BROWSER: WiFiManager configuration page
    
    USER->>BROWSER: Select SSID, enter password
    BROWSER->>WIFI: POST /wifisave
    WIFI->>ESP: Save credentials to flash
    ESP->>ESP: ESP.restart()
    
    Note over ESP: Reboot with new credentials...
    
    ESP->>WIFI: wifiManager.autoConnect()
    WIFI->>WIFI: Connect to new SSID
    WIFI-->>ESP: Connection successful
    
    Note over ESP: Normal operation resumes
```

### Steps

1. **Button Press**: User holds GPIO 0 button for 3 seconds
2. **Portal Trigger**: ESP detects sustained LOW signal, triggers portal
3. **Credential Erase**: Calls `WiFi.disconnect(true)` to clear saved WiFi settings
4. **Reboot**: ESP restarts to apply changes
5. **AP Mode**: No saved credentials → creates "RailHub8266-Setup" AP
6. **Captive Portal**: User connects, redirected to 192.168.4.1
7. **WiFi Configuration**: User selects SSID, enters password, submits
8. **Save & Reboot**: Credentials saved to flash, ESP restarts
9. **Normal Connection**: ESP connects to new network, starts services

### Performance

- **Portal Trigger Time**: 3 seconds (hold duration)
- **Reboot Time**: ~3-5 seconds
- **AP Creation Time**: ~2 seconds
- **Portal Timeout**: 180 seconds (3 minutes, configurable)

---

**Next**: [7. Deployment View](07_deployment_view.md)
