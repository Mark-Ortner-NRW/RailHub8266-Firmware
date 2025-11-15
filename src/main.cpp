#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>
#include "config.h"

// Forward declarations
void initializeOutputs();
void initializeWiFi();
void initializeWiFiManager();
void checkConfigPortalTrigger();
void initializeWebServer();
void executeOutputCommand(int pin, bool active, int brightnessPercent);
void updateBlinkingOutputs();
void updateChasingLightGroups();
void setOutputInterval(int index, unsigned int intervalMs);
void createChasingGroup(uint8_t groupId, uint8_t* outputIndices, uint8_t count, unsigned int intervalMs);
void deleteChasingGroup(uint8_t groupId);
void saveChasingGroups();
void loadChasingGroups();
void saveOutputState(int index);
void loadOutputStates();
void saveAllOutputStates();
void saveCustomParameters();
void loadCustomParameters();

// Global variables
// Web Server
ESP8266WebServer* server = nullptr;
WebSocketsServer* ws = nullptr;
WiFiManager wifiManager;

// WebSocket broadcast timer
unsigned long lastBroadcast = 0;
const unsigned long BROADCAST_INTERVAL = 500; // Broadcast every 500ms

#define MAX_CHASING_GROUPS 4

// Chasing group structure
struct ChasingGroup {
    uint8_t groupId;
    bool active;
    char name[21]; // 20 chars + null terminator
    uint8_t outputIndices[8]; // Max 8 outputs per group
    uint8_t outputCount;
    uint16_t interval; // Step interval in ms
    uint8_t currentStep; // Current active output in sequence
    unsigned long lastStepTime;
};

// EEPROM structure for ESP8266
struct EEPROMData {
    char deviceName[40];
    bool outputStates[8];
    uint8_t outputBrightness[8];
    char outputNames[8][21]; // 20 chars + null terminator
    uint16_t outputIntervals[8]; // Blink interval in milliseconds (0 = no blink)
    // Chasing groups data
    uint8_t chasingGroupCount;
    struct {
        uint8_t groupId;
        bool active;
        char name[21];
        uint8_t outputIndices[8];
        uint8_t outputCount;
        uint16_t interval;
    } chasingGroups[MAX_CHASING_GROUPS];
    uint8_t checksum;
};
EEPROMData eepromData;

String macAddress;
char customDeviceName[40] = DEVICE_NAME; // Custom device name from WiFiManager
bool portalRunning = false;
unsigned long portalButtonPressTime = 0;
bool wifiConnected = false;

// Output pin configuration
int outputPins[MAX_OUTPUTS] = LED_PINS;
bool outputStates[MAX_OUTPUTS] = {false};
int outputBrightness[MAX_OUTPUTS] = {255}; // 0-255 for PWM
String outputNames[MAX_OUTPUTS]; // Custom names for outputs
unsigned int outputIntervals[MAX_OUTPUTS] = {0}; // Blink interval in ms (0 = no blink)
unsigned long lastBlinkTime[MAX_OUTPUTS] = {0}; // Last blink toggle time
bool blinkState[MAX_OUTPUTS] = {false}; // Current blink state (for internal tracking)
int8_t outputChasingGroup[MAX_OUTPUTS] = {-1, -1, -1, -1, -1, -1, -1}; // Which chasing group owns this output (-1 = none)

// Chasing light groups
ChasingGroup chasingGroups[MAX_CHASING_GROUPS];
uint8_t chasingGroupCount = 0;

// Timing variables

void broadcastStatus(); // Forward declaration

void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WS] Client #%u disconnected\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = ws->remoteIP(num);
                Serial.printf("[WS] Client #%u connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
                broadcastStatus(); // Send current status to new client
            }
            break;
        case WStype_TEXT:
            Serial.printf("[WS] Received from #%u: %s\n", num, payload);
            break;
    }
}

void broadcastStatus() {
    if (!ws) return;
    
    DynamicJsonDocument doc(2048);
    doc["macAddress"] = macAddress;
    doc["name"] = customDeviceName;
    doc["wifiMode"] = WiFi.getMode() == WIFI_AP ? "AP" : "STA";
    doc["ip"] = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    doc["ssid"] = WiFi.getMode() == WIFI_AP ? String(AP_SSID) : WiFi.SSID();
    doc["apClients"] = WiFi.softAPgetStationNum();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis();
    doc["buildDate"] = String(__DATE__) + " " + String(__TIME__);
    doc["flashUsed"] = ESP.getSketchSize();
    doc["flashFree"] = ESP.getFreeSketchSpace();
    doc["flashPartition"] = 1044464; // Program partition size (from platformio build output)
    
    JsonArray outputs = doc.createNestedArray("outputs");
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        JsonObject output = outputs.createNestedObject();
        output["pin"] = outputPins[i];
        output["active"] = outputStates[i];
        output["brightness"] = map(outputBrightness[i], 0, 255, 0, 100);
        output["name"] = outputNames[i];
        output["interval"] = outputIntervals[i];
        output["chasingGroup"] = outputChasingGroup[i];
    }
    
    JsonArray groups = doc.createNestedArray("chasingGroups");
    for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
        if (chasingGroups[i].active) {
            JsonObject group = groups.createNestedObject();
            group["groupId"] = chasingGroups[i].groupId;
            group["name"] = chasingGroups[i].name;
            group["interval"] = chasingGroups[i].interval;
            group["outputCount"] = chasingGroups[i].outputCount;
            JsonArray groupOutputs = group.createNestedArray("outputs");
            for (int j = 0; j < chasingGroups[i].outputCount; j++) {
                groupOutputs.add(outputPins[chasingGroups[i].outputIndices[j]]);
            }
        }
    }
    
    String response;
    serializeJson(doc, response);
    ws->broadcastTXT(response);
}

void setup() {
    Serial.begin(115200);
    delay(100);
    
    // Initialize EEPROM for ESP8266
    EEPROM.begin(EEPROM_SIZE);
    
    Serial.println("\n\n========================================");
    Serial.println("  RailHub8266 ESP8266 Controller v1.0");
    Serial.println("========================================");
    Serial.println("[BOOT] Chip ID: " + String(ESP.getChipId(), HEX));
    Serial.println("[BOOT] CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
    Serial.println("[BOOT] Flash Size: " + String(ESP.getFlashChipSize() / 1024) + " KB");
    Serial.println("[BOOT] Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    
    // Get MAC address for unique identification
    macAddress = WiFi.macAddress();
    Serial.println("[INIT] MAC Address: " + macAddress);
    
    // Initialize portal trigger pin
    Serial.println("[INIT] Configuring portal trigger pin (GPIO " + String(PORTAL_TRIGGER_PIN) + ")");
    pinMode(PORTAL_TRIGGER_PIN, INPUT_PULLUP);
    
    // Initialize output pins
    Serial.println("[INIT] Initializing " + String(MAX_OUTPUTS) + " output pins...");
    initializeOutputs();
    
    // Load custom parameters from preferences
    Serial.println("[INIT] Loading custom parameters from NVRAM...");
    loadCustomParameters();
    
    // Load saved output states from NVRAM
    Serial.println("[INIT] Loading saved output states...");
    loadOutputStates();
    
    // Load chasing groups
    Serial.println("[INIT] Loading chasing groups...");
    loadChasingGroups();
    
    // Initialize WiFi with WiFiManager
    Serial.println("[INIT] Initializing WiFi Manager...");
    initializeWiFiManager();
    
    // Initialize web server after WiFi is connected
    if (wifiConnected) {
        Serial.println("[INIT] Starting web server on port 80...");
        server = new ESP8266WebServer(80);
        initializeWebServer();
        Serial.println("[WEB] Web server initialized successfully");
        
        Serial.println("[INIT] Starting WebSocket server on port 81...");
        ws = new WebSocketsServer(81);
        ws->begin();
        ws->onEvent(wsEvent);
        Serial.println("[WS] WebSocket server started on port 81");
    } else {
        Serial.println("[WARN] WiFi not connected - web server not started");
    }
    
    Serial.println("\n========================================");
    Serial.println("  Setup Complete!");
    Serial.println("========================================");
    Serial.println("[INFO] Device Name: " + String(customDeviceName));
    Serial.println("[INFO] Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("[INFO] System ready for operation\n");
}

void loop() {
    // Check for config portal trigger button
    checkConfigPortalTrigger();
    
    // Handle web server requests
    if (server) {
        server->handleClient();
    }
    
    // Handle WebSocket events
    if (ws) {
        ws->loop();
        
        // Broadcast status periodically for real-time updates
        unsigned long now = millis();
        if (now - lastBroadcast >= BROADCAST_INTERVAL) {
            broadcastStatus();
            lastBroadcast = now;
        }
    }
    
    // Update mDNS responder
    MDNS.update();
    
    // Update chasing light groups (has priority)
    updateChasingLightGroups();
    
    // Update blinking outputs (only for non-chasing outputs)
    updateBlinkingOutputs();
    
    // Handle any other tasks
    yield();
}

// Periodic status logging (called every 60 seconds via timer)
void logSystemStatus() {
    static unsigned long lastStatusLog = 0;
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastStatusLog >= 60000) {
        lastStatusLog = currentMillis;
        Serial.println("\n[STATUS] === System Status Report ===");
        Serial.println("[STATUS] Uptime: " + String(currentMillis / 1000) + " seconds");
        Serial.println("[STATUS] Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
        Serial.println("[STATUS] WiFi Status: " + String(WiFi.isConnected() ? "Connected" : "Disconnected"));
        if (WiFi.isConnected()) {
            Serial.println("[STATUS] IP Address: " + WiFi.localIP().toString());
            Serial.println("[STATUS] RSSI: " + String(WiFi.RSSI()) + " dBm");
        }
        
        // Count active outputs
        int activeCount = 0;
        for (int i = 0; i < MAX_OUTPUTS; i++) {
            if (outputStates[i]) activeCount++;
        }
        Serial.println("[STATUS] Active Outputs: " + String(activeCount) + "/" + String(MAX_OUTPUTS));
        Serial.println("[STATUS] ========================\n");
    }
}

void initializeOutputs() {
    Serial.println("[OUTPUT] Initializing outputs...");
    
    // Set PWM range for ESP8266 (0-1023 by default, we'll use 0-255 range)
    analogWriteRange(255);
    analogWriteFreq(1000); // 1kHz PWM frequency
    
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        Serial.print("[OUTPUT] Configuring Output " + String(i) + " on GPIO " + String(outputPins[i]));
        pinMode(outputPins[i], OUTPUT);
        analogWrite(outputPins[i], 0);
        Serial.println(" - OK (PWM 1kHz, 8-bit)");
    }
    
    // Status LED (active LOW on ESP8266)
    Serial.println("[OUTPUT] Initializing status LED on GPIO " + String(STATUS_LED_PIN));
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW); // Turn on status LED (active LOW)
    Serial.println("[OUTPUT] All outputs initialized successfully");
}

void initializeWiFi() {
    Serial.println("Configuring Access Point...");
    
    // Disconnect from any existing WiFi connection
    WiFi.disconnect();
    delay(100);
    
    // Configure Access Point IP address
    IPAddress local_IP;
    IPAddress gateway;
    IPAddress subnet;
    
    local_IP.fromString(AP_LOCAL_IP);
    gateway.fromString(AP_GATEWAY);
    subnet.fromString(AP_SUBNET);
    
    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
        Serial.println("AP Config Failed!");
    }
    
    // Start Access Point
    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_HIDDEN, AP_MAX_CONNECTIONS);
    
    if (apStarted) {
        Serial.println();
        Serial.println("Access Point started successfully!");
        Serial.print("AP SSID: ");
        Serial.println(AP_SSID);
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
        Serial.print("AP MAC address: ");
        Serial.println(WiFi.softAPmacAddress());
        Serial.print("Max connections: ");
        Serial.println(AP_MAX_CONNECTIONS);
        
        // Blink status LED to indicate AP started (active LOW)
        for (int i = 0; i < 5; i++) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(150);
            digitalWrite(STATUS_LED_PIN, LOW);
            delay(150);
        }
    } else {
        Serial.println();
        Serial.println("Access Point failed to start!");
    }
}

void initializeWiFiManager() {
    Serial.println("[WIFI] Initializing WiFiManager...");
    Serial.println("[WIFI] Configuration Portal SSID: " + String(WIFIMANAGER_AP_SSID));
    Serial.println("[WIFI] Portal Trigger Pin: GPIO " + String(PORTAL_TRIGGER_PIN));
    
    // Ensure WiFi is in correct mode
    WiFi.mode(WIFI_STA);
    delay(100);
    
    // WiFiManager already initialized globally
    
    // Set custom parameters
    WiFiManagerParameter custom_device_name("device_name", "Device Name", customDeviceName, 40);
    
    // Add parameters to WiFiManager
    wifiManager.addParameter(&custom_device_name);
    
    // Minimal configuration for ESP8266 to save RAM
    wifiManager.setMinimumSignalQuality(20);  // Higher = fewer networks shown = less RAM
    wifiManager.setRemoveDuplicateAPs(true);
    
    // Disable features to save memory
    wifiManager.setShowInfoUpdate(false);  // Don't show info in update mode
    wifiManager.setShowInfoErase(false);   // Don't show erase button
    
    // Set save config callback
    wifiManager.setSaveConfigCallback([]() {
        Serial.println("[WIFI] Configuration saved!");
        Serial.print("[WIFI] Device Name: ");
        Serial.println(customDeviceName);
        Serial.println("[WIFI] WiFi credentials will be used on next boot");
        Serial.println("[WIFI] Restarting ESP8266 to apply new configuration...");
        delay(2000);
        ESP.restart();
    });
    
    // Set short timeout to free memory after config
    wifiManager.setConfigPortalTimeout(300); // 5 minutes timeout
    
    // Disable debug output to save RAM
    wifiManager.setDebugOutput(false);
    

    
    // Set AP callback
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
        Serial.println("\n========================================");
        Serial.println("     CONFIGURATION MODE ACTIVE");
        Serial.println("========================================");
        Serial.println("[WIFI] AP Mode Started");
        Serial.println("[WIFI] AP SSID: " + String(WIFIMANAGER_AP_SSID));
        Serial.println("[WIFI] AP Password: " + String(WIFIMANAGER_AP_PASSWORD));
        Serial.println("[WIFI] AP IP Address: " + WiFi.softAPIP().toString());
        Serial.println("[WIFI] Configuration Portal: http://192.168.4.1");
        Serial.println("[INFO] Connect your device to the AP above");
        Serial.println("[INFO] Portal running on port 80");
        Serial.println("========================================\n");
        
        // Blink LED to indicate config mode (active LOW)
        for (int i = 0; i < 10; i++) {
            digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
            delay(100);
        }
        digitalWrite(STATUS_LED_PIN, LOW); // On (active LOW)
    });
    
    // Configure static IP for portal
    IPAddress portal_ip(192, 168, 4, 1);
    IPAddress portal_gateway(192, 168, 4, 1);
    IPAddress portal_subnet(255, 255, 255, 0);
    wifiManager.setAPStaticIPConfig(portal_ip, portal_gateway, portal_subnet);
    
    // Try to connect with saved credentials or start portal
    Serial.println("[WIFI] Attempting to connect to WiFi...");
    Serial.print("[WIFI] Config AP SSID: ");
    Serial.println(WIFIMANAGER_AP_SSID);
    
    // Use NULL for open AP if password is empty, otherwise use the password
    const char* apPassword = (strlen(WIFIMANAGER_AP_PASSWORD) == 0) ? NULL : WIFIMANAGER_AP_PASSWORD;
    
    unsigned long connectStart = millis();
    if (wifiManager.autoConnect(WIFIMANAGER_AP_SSID, apPassword)) {
        // Connected to WiFi successfully
        unsigned long connectDuration = millis() - connectStart;
        wifiConnected = true;
        
        Serial.println("\n========================================");
        Serial.println("     WIFI CONNECTION SUCCESSFUL");
        Serial.println("========================================");
        Serial.print("[WIFI] IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WIFI] SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("[WIFI] Signal Strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        Serial.print("[WIFI] MAC Address: ");
        Serial.println(WiFi.macAddress());
        Serial.print("[WIFI] Connection Time: ");
        Serial.print(connectDuration);
        Serial.println("ms");
        Serial.println("========================================\n");
        
        // Get custom parameters
        strncpy(customDeviceName, custom_device_name.getValue(), 40);
        saveCustomParameters();
        
        // Start mDNS service
        String hostname = String(customDeviceName);
        hostname.toLowerCase();
        hostname.replace(" ", "-");
        if (MDNS.begin(hostname.c_str())) {
            Serial.print("[MDNS] mDNS responder started: ");
            Serial.print(hostname);
            Serial.println(".local");
            MDNS.addService("http", "tcp", 80);
            Serial.println("[MDNS] HTTP service added");
        } else {
            Serial.println("[ERROR] mDNS failed to start");
        }
        
        // Solid LED to indicate connected (active LOW)
        digitalWrite(STATUS_LED_PIN, LOW);
    } else {
        // Failed to connect - fallback to AP mode
        Serial.println("[ERROR] Failed to connect - starting fallback AP mode");
        wifiConnected = false;
        initializeWiFi();
    }
}

void checkConfigPortalTrigger() {
    static bool warningShown = false;
    
    // Check if portal trigger button is pressed
    if (digitalRead(PORTAL_TRIGGER_PIN) == LOW) {
        if (portalButtonPressTime == 0) {
            portalButtonPressTime = millis();
            warningShown = false;
            Serial.println("[PORTAL] Config button pressed (hold for 3s to trigger)");
        } else {
            unsigned long holdDuration = millis() - portalButtonPressTime;
            
            // Warning at 2.5 seconds - only show once
            if (holdDuration > 2500 && !warningShown && !portalRunning) {
                Serial.println("[PORTAL] Warning: Portal trigger in 0.5s...");
                warningShown = true;
            }
            
            if (holdDuration > PORTAL_TRIGGER_DURATION && !portalRunning) {
                Serial.println("[PORTAL] Portal trigger detected! Resetting WiFi and restarting...");
                Serial.print("[PORTAL] Free heap before reset: ");
                Serial.print(ESP.getFreeHeap());
                Serial.println(" bytes");
                portalRunning = true;
                
                // Blink LED rapidly (active LOW)
                Serial.println("[PORTAL] Blinking status LED (confirmation)");
                for (int i = 0; i < 20; i++) {
                    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
                    delay(50);
                }
                
                // Clear WiFi settings (ESP8266 stores WiFi creds in flash)
                Serial.println("[PORTAL] Disconnecting WiFi and clearing saved networks...");
                WiFi.disconnect(true); // true = also erase stored credentials
                delay(1000);
                
                // Restart to trigger portal
                Serial.println("[PORTAL] Restarting ESP8266 in 1s...");
                Serial.flush();
                delay(1000);
                ESP.restart();
            }
        }
    } else {
        if (portalButtonPressTime > 0) {
            unsigned long pressDuration = millis() - portalButtonPressTime;
            Serial.print("[PORTAL] Config button released after ");
            Serial.print(pressDuration);
            Serial.println("ms (trigger requires 3000ms)");
        }
        portalButtonPressTime = 0;
        portalRunning = false;
        warningShown = false;
    }
}

void saveCustomParameters() {
    Serial.println("[EEPROM] Saving custom parameters...");
    
    // Read current EEPROM data
    EEPROM.get(0, eepromData);
    
    // Update device name
    strncpy(eepromData.deviceName, customDeviceName, 39);
    eepromData.deviceName[39] = '\0';
    
    // Write back to EEPROM
    EEPROM.put(0, eepromData);
    EEPROM.commit();
    
    Serial.print("[EEPROM] Custom parameters saved: Device Name = '");
    Serial.print(customDeviceName);
    Serial.println("'");
}

void saveChasingGroups() {
    Serial.println("[EEPROM] Saving chasing groups...");
    
    // Read current EEPROM data
    EEPROM.get(0, eepromData);
    
    // Update chasing groups
    eepromData.chasingGroupCount = 0;
    for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
        if (chasingGroups[i].active) {
            eepromData.chasingGroups[i].groupId = chasingGroups[i].groupId;
            eepromData.chasingGroups[i].active = true;
            strncpy(eepromData.chasingGroups[i].name, chasingGroups[i].name, 20);
            eepromData.chasingGroups[i].name[20] = '\0';
            eepromData.chasingGroups[i].outputCount = chasingGroups[i].outputCount;
            eepromData.chasingGroups[i].interval = chasingGroups[i].interval;
            for (int j = 0; j < chasingGroups[i].outputCount; j++) {
                eepromData.chasingGroups[i].outputIndices[j] = chasingGroups[i].outputIndices[j];
            }
            eepromData.chasingGroupCount++;
        } else {
            eepromData.chasingGroups[i].active = false;
        }
    }
    
    // Write back to EEPROM
    EEPROM.put(0, eepromData);
    EEPROM.commit();
    
    Serial.print("[EEPROM] Saved ");
    Serial.print(eepromData.chasingGroupCount);
    Serial.println(" chasing groups");
}

void loadChasingGroups() {
    Serial.println("[EEPROM] Loading chasing groups...");
    
    // Read EEPROM data
    EEPROM.get(0, eepromData);
    
    int loadedGroups = 0;
    
    for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
        if (eepromData.chasingGroups[i].active && eepromData.chasingGroups[i].outputCount > 0) {
            chasingGroups[i].groupId = eepromData.chasingGroups[i].groupId;
            chasingGroups[i].active = true;
            strncpy(chasingGroups[i].name, eepromData.chasingGroups[i].name, 20);
            chasingGroups[i].name[20] = '\0';
            chasingGroups[i].outputCount = eepromData.chasingGroups[i].outputCount;
            chasingGroups[i].interval = eepromData.chasingGroups[i].interval;
            chasingGroups[i].currentStep = 0;
            chasingGroups[i].lastStepTime = millis();
            
            for (int j = 0; j < chasingGroups[i].outputCount; j++) {
                uint8_t idx = eepromData.chasingGroups[i].outputIndices[j];
                chasingGroups[i].outputIndices[j] = idx;
                if (idx < MAX_OUTPUTS) {
                    outputChasingGroup[idx] = chasingGroups[i].groupId;
                }
            }
            
            loadedGroups++;
            
            Serial.print("[CHASING] Loaded group ");
            Serial.print(chasingGroups[i].groupId);
            Serial.print(" '");
            Serial.print(chasingGroups[i].name);
            Serial.print("' with ");
            Serial.print(chasingGroups[i].outputCount);
            Serial.print(" outputs, interval: ");
            Serial.print(chasingGroups[i].interval);
            Serial.println("ms");
        } else {
            chasingGroups[i].active = false;
            chasingGroups[i].outputCount = 0;
        }
    }
    
    Serial.print("[EEPROM] Loaded ");
    Serial.print(loadedGroups);
    Serial.println(" chasing groups");
}

void loadCustomParameters() {
    Serial.println("[EEPROM] Loading custom parameters...");
    
    // Read EEPROM data
    EEPROM.get(0, eepromData);
    
    // Check if data is valid (simple check - not empty)
    if (eepromData.deviceName[0] != '\0' && eepromData.deviceName[0] != 0xFF) {
        strncpy(customDeviceName, eepromData.deviceName, 39);
        customDeviceName[39] = '\0';
        Serial.print("[EEPROM] Loaded custom device name: '");
        Serial.print(customDeviceName);
        Serial.println("'");
    } else {
        strncpy(customDeviceName, DEVICE_NAME, 39);
        customDeviceName[39] = '\0';
        Serial.print("[EEPROM] No custom device name found, using default: '");
        Serial.print(customDeviceName);
        Serial.println("'");
    }
}

void executeOutputCommand(int pin, bool active, int brightnessPercent) {
    unsigned long startTime = millis();
    
    // Find the output index for the given pin
    int outputIndex = -1;
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        if (outputPins[i] == pin) {
            outputIndex = i;
            break;
        }
    }
    
    if (outputIndex == -1) {
        Serial.println("[ERROR] Invalid GPIO pin: " + String(pin));
        return;
    }
    
    // Validate brightness range
    if (brightnessPercent < 0 || brightnessPercent > 100) {
        Serial.println("[ERROR] Invalid brightness: " + String(brightnessPercent) + "% (must be 0-100)");
        brightnessPercent = constrain(brightnessPercent, 0, 100);
    }
    
    // Update state
    outputStates[outputIndex] = active;
    outputBrightness[outputIndex] = map(brightnessPercent, 0, 100, 0, 255);
    
    // Apply the command
    if (active) {
        analogWrite(outputPins[outputIndex], outputBrightness[outputIndex]);
    } else {
        analogWrite(outputPins[outputIndex], 0);
    }
    
    // Save the state to persistent storage
    saveOutputState(outputIndex);
    
    // Broadcast update to all WebSocket clients
    broadcastStatus();
    
    unsigned long duration = millis() - startTime;
    String nameStr = outputNames[outputIndex].length() > 0 ? " [" + outputNames[outputIndex] + "]" : "";
    Serial.println("[CMD] Output " + String(outputIndex) + " (GPIO " + String(pin) + ")" + nameStr + ": " + 
                   (active ? "ON" : "OFF") + " @ " + String(brightnessPercent) + "% (" + String(duration) + "ms)");
}

void saveOutputState(int index) {
    if (index < 0 || index >= MAX_OUTPUTS) {
        Serial.print("[ERROR] Invalid output index for state save: ");
        Serial.println(index);
        return;
    }
    
    // Read current EEPROM data
    EEPROM.get(0, eepromData);
    
    // Update specific output
    eepromData.outputStates[index] = outputStates[index];
    eepromData.outputBrightness[index] = outputBrightness[index];
    eepromData.outputIntervals[index] = outputIntervals[index];
    
    // Write back to EEPROM
    EEPROM.put(0, eepromData);
    EEPROM.commit();
    
    Serial.print("[EEPROM] Saved state for Output ");
    Serial.print(index);
    Serial.print(" (GPIO ");
    Serial.print(outputPins[index]);
    Serial.print("): ");
    Serial.print(outputStates[index] ? "ON" : "OFF");
    Serial.print(" @ ");
    Serial.print(outputBrightness[index]);
    Serial.print(" PWM, Interval: ");
    Serial.print(outputIntervals[index]);
    Serial.println("ms");
}

void saveOutputName(int index, String name) {
    if (index < 0 || index >= MAX_OUTPUTS) {
        Serial.println("[ERROR] Invalid output index for name save: " + String(index));
        return;
    }
    
    // Read current EEPROM data
    EEPROM.get(0, eepromData);
    
    // If name is empty or whitespace-only, clear the name
    name.trim(); // Trim modifies in place
    if (name.length() == 0) {
        eepromData.outputNames[index][0] = '\0';
        outputNames[index] = "";
        EEPROM.put(0, eepromData);
        EEPROM.commit();
        Serial.println("[EEPROM] Removed custom name for Output " + String(index) + " (GPIO " + String(outputPins[index]) + ") - using default");
        return;
    }
    
    // Copy name to EEPROM structure (max 20 chars + null)
    strncpy(eepromData.outputNames[index], name.c_str(), 20);
    eepromData.outputNames[index][20] = '\0';
    
    // Write back to EEPROM
    EEPROM.put(0, eepromData);
    EEPROM.commit();
    
    outputNames[index] = name;
    Serial.println("[EEPROM] Saved name for Output " + String(index) + " (GPIO " + String(outputPins[index]) + "): '" + name + "'");
}

void loadOutputStates() {
    Serial.println("[EEPROM] Loading saved output states...");
    
    // Read EEPROM data
    EEPROM.get(0, eepromData);
    
    // Check if data is valid (check for uninitialized EEPROM)
    bool validData = true;
    
    // Check device name for 0xFF pattern (uninitialized)
    if (eepromData.deviceName[0] == 0xFF) {
        validData = false;
    }
    
    // Check brightness values
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        if (eepromData.outputBrightness[i] > 255) {
            validData = false;
            break;
        }
    }
    
    // Initialize with defaults if invalid
    if (!validData) {
        Serial.println("[EEPROM] No valid data found, initializing defaults");
        
        // Clear entire structure
        memset(&eepromData, 0, sizeof(eepromData));
        
        // Set defaults
        for (int i = 0; i < MAX_OUTPUTS; i++) {
            eepromData.outputStates[i] = false;
            eepromData.outputBrightness[i] = 255;
            eepromData.outputNames[i][0] = '\0';
            eepromData.outputIntervals[i] = 0;
        }
        
        // Initialize chasing groups
        eepromData.chasingGroupCount = 0;
        for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
            eepromData.chasingGroups[i].active = false;
            eepromData.chasingGroups[i].outputCount = 0;
        }
        
        strncpy(eepromData.deviceName, DEVICE_NAME, 39);
        eepromData.deviceName[39] = '\0';
        
        EEPROM.put(0, eepromData);
        EEPROM.commit();
        Serial.println("[EEPROM] Defaults saved to EEPROM");
    }
    
    int loadedCount = 0;
    int namedCount = 0;
    int blinkingCount = 0;
    
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        // Load state and brightness from EEPROM
        outputStates[i] = eepromData.outputStates[i];
        outputBrightness[i] = eepromData.outputBrightness[i];
        outputIntervals[i] = eepromData.outputIntervals[i];
        
        // Load custom name - validate it's printable ASCII
        if (eepromData.outputNames[i][0] != '\0' && 
            eepromData.outputNames[i][0] != 0xFF &&
            eepromData.outputNames[i][0] >= 32 && 
            eepromData.outputNames[i][0] <= 126) {
            // Ensure null termination
            eepromData.outputNames[i][20] = '\0';
            outputNames[i] = String(eepromData.outputNames[i]);
            namedCount++;
        } else {
            outputNames[i] = "";
        }
        
        // Apply the loaded state to the output
        if (outputStates[i]) {
            // If blinking is enabled, start in ON state
            if (outputIntervals[i] > 0) {
                analogWrite(outputPins[i], outputBrightness[i]);
                blinkState[i] = true;
                lastBlinkTime[i] = millis();
                blinkingCount++;
            } else {
                analogWrite(outputPins[i], outputBrightness[i]);
            }
            int brightPercent = map(outputBrightness[i], 0, 255, 0, 100);
            Serial.print("[EEPROM] Output " + String(i) + " (GPIO " + String(outputPins[i]) + "): ON @ " + String(brightPercent) + "%");
            if (outputIntervals[i] > 0) {
                Serial.print(" [Blink: " + String(outputIntervals[i]) + "ms]");
            }
            if (outputNames[i].length() > 0) {
                Serial.println(" [Name: " + outputNames[i] + "]");
            } else {
                Serial.println("");
            }
            loadedCount++;
        } else {
            analogWrite(outputPins[i], 0);
            blinkState[i] = false;
        }
    }
    
    Serial.println("[EEPROM] Loaded " + String(loadedCount) + " active outputs, " + String(namedCount) + " custom names, " + String(blinkingCount) + " blinking");
}

void saveAllOutputStates() {
    unsigned long startTime = millis();
    Serial.println("[EEPROM] Saving all output states (batch operation)...");
    
    // Read current EEPROM data
    EEPROM.get(0, eepromData);
    
    // Update all output states and brightness
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        eepromData.outputStates[i] = outputStates[i];
        eepromData.outputBrightness[i] = outputBrightness[i];
        eepromData.outputIntervals[i] = outputIntervals[i];
    }
    
    // Write back to EEPROM
    EEPROM.put(0, eepromData);
    EEPROM.commit();
    
    unsigned long duration = millis() - startTime;
    Serial.print("[EEPROM] Batch save complete: ");
    Serial.print(MAX_OUTPUTS);
    Serial.print(" outputs saved (");
    Serial.print(duration);
    Serial.println("ms)");
}

void updateChasingLightGroups() {
    unsigned long currentMillis = millis();
    
    for (int g = 0; g < MAX_CHASING_GROUPS; g++) {
        ChasingGroup* group = &chasingGroups[g];
        
        if (!group->active || group->outputCount == 0) continue;
        
        // Check if it's time to step to next output
        if (currentMillis - group->lastStepTime >= group->interval) {
            // Turn off current output
            uint8_t currentIdx = group->outputIndices[group->currentStep];
            if (currentIdx < MAX_OUTPUTS) {
                analogWrite(outputPins[currentIdx], 0);
                Serial.print("[CHASING] Group ");
                Serial.print(group->groupId);
                Serial.print(" OFF: idx=");
                Serial.print(currentIdx);
                Serial.print(" GPIO=");
                Serial.println(outputPins[currentIdx]);
            }
            
            // Move to next step
            group->currentStep = (group->currentStep + 1) % group->outputCount;
            group->lastStepTime = currentMillis;
            
            // Turn on next output (always, regardless of state)
            uint8_t nextIdx = group->outputIndices[group->currentStep];
            if (nextIdx < MAX_OUTPUTS) {
                analogWrite(outputPins[nextIdx], outputBrightness[nextIdx]);
                Serial.print("[CHASING] Group ");
                Serial.print(group->groupId);
                Serial.print(" ON: idx=");
                Serial.print(nextIdx);
                Serial.print(" GPIO=");
                Serial.println(outputPins[nextIdx]);
            }
        }
    }
}

void updateBlinkingOutputs() {
    unsigned long currentMillis = millis();
    
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        // Skip if output is part of a chasing group
        if (outputChasingGroup[i] >= 0) continue;
        
        // Only process if output is active and has a blink interval set
        if (outputStates[i] && outputIntervals[i] > 0) {
            // Check if it's time to toggle
            if (currentMillis - lastBlinkTime[i] >= outputIntervals[i]) {
                lastBlinkTime[i] = currentMillis;
                blinkState[i] = !blinkState[i];
                
                // Toggle the output
                if (blinkState[i]) {
                    analogWrite(outputPins[i], outputBrightness[i]);
                } else {
                    analogWrite(outputPins[i], 0);
                }
            }
        } else if (outputStates[i] && outputIntervals[i] == 0) {
            // No blinking - ensure output is solid on
            if (!blinkState[i]) {
                analogWrite(outputPins[i], outputBrightness[i]);
                blinkState[i] = true;
            }
        }
    }
}

void createChasingGroup(uint8_t groupId, uint8_t* outputIndices, uint8_t count, unsigned int intervalMs, const char* groupName = nullptr) {
    if (groupId >= MAX_CHASING_GROUPS || count == 0 || count > 8) {
        Serial.println("[ERROR] Invalid chasing group parameters");
        return;
    }
    
    // Find or create group slot
    int groupSlot = -1;
    for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
        if (chasingGroups[i].groupId == groupId || !chasingGroups[i].active) {
            groupSlot = i;
            break;
        }
    }
    
    if (groupSlot == -1) {
        Serial.println("[ERROR] No available chasing group slots");
        return;
    }
    
    // Clear old group memberships for these outputs
    for (int i = 0; i < count; i++) {
        uint8_t idx = outputIndices[i];
        if (idx < MAX_OUTPUTS) {
            outputChasingGroup[idx] = -1;
        }
    }
    
    // Setup new group
    ChasingGroup* group = &chasingGroups[groupSlot];
    group->groupId = groupId;
    group->active = true;
    
    // Set group name (default: "Group X" if not provided)
    if (groupName != nullptr && strlen(groupName) > 0) {
        strncpy(group->name, groupName, 20);
        group->name[20] = '\0';
    } else {
        snprintf(group->name, 21, "Group %d", groupId);
    }
    
    group->outputCount = count;
    group->interval = intervalMs;
    group->currentStep = 0;
    group->lastStepTime = millis();
    
    for (int i = 0; i < count; i++) {
        group->outputIndices[i] = outputIndices[i];
        if (outputIndices[i] < MAX_OUTPUTS) {
            outputChasingGroup[outputIndices[i]] = groupId;
        }
    }
    
    // Turn on all outputs in group
    for (int i = 0; i < count; i++) {
        uint8_t idx = outputIndices[i];
        if (idx < MAX_OUTPUTS) {
            outputStates[idx] = true;
        }
    }
    
    // Initialize first output on, rest off
    for (int i = 0; i < count; i++) {
        uint8_t idx = outputIndices[i];
        if (idx < MAX_OUTPUTS) {
            if (i == 0) {
                analogWrite(outputPins[idx], outputBrightness[idx]);
            } else {
                analogWrite(outputPins[idx], 0);
            }
        }
    }
    
    saveChasingGroups();
    
    Serial.print("[CHASING] Group ");
    Serial.print(groupId);
    Serial.print(" created with ");
    Serial.print(count);
    Serial.print(" outputs, interval: ");
    Serial.print(intervalMs);
    Serial.println("ms");
}

void deleteChasingGroup(uint8_t groupId) {
    for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
        if (chasingGroups[i].groupId == groupId && chasingGroups[i].active) {
            // Free outputs from group
            for (int j = 0; j < chasingGroups[i].outputCount; j++) {
                uint8_t idx = chasingGroups[i].outputIndices[j];
                if (idx < MAX_OUTPUTS) {
                    outputChasingGroup[idx] = -1;
                    // Turn off output
                    analogWrite(outputPins[idx], 0);
                    outputStates[idx] = false;
                }
            }
            
            // Clear group
            chasingGroups[i].active = false;
            chasingGroups[i].outputCount = 0;
            
            saveChasingGroups();
            
            Serial.print("[CHASING] Group ");
            Serial.print(groupId);
            Serial.println(" deleted");
            return;
        }
    }
    
    Serial.print("[ERROR] Chasing group ");
    Serial.print(groupId);
    Serial.println(" not found");
}

void setOutputInterval(int index, unsigned int intervalMs) {
    if (index < 0 || index >= MAX_OUTPUTS) {
        Serial.println("[ERROR] Invalid output index for interval: " + String(index));
        return;
    }
    
    outputIntervals[index] = intervalMs;
    
    // Reset blink timing
    lastBlinkTime[index] = millis();
    blinkState[index] = true;
    
    // If output is active and interval is set, start with ON state
    if (outputStates[index]) {
        if (intervalMs > 0) {
            analogWrite(outputPins[index], outputBrightness[index]);
            Serial.println("[INTERVAL] Output " + String(index) + " (GPIO " + String(outputPins[index]) + ") set to blink every " + String(intervalMs) + "ms");
        } else {
            analogWrite(outputPins[index], outputBrightness[index]);
            Serial.println("[INTERVAL] Output " + String(index) + " (GPIO " + String(outputPins[index]) + ") blinking disabled (solid)");
        }
    }
    
    // Save to EEPROM
    saveOutputState(index);
}

void initializeWebServer() {
    if (!server) return;
    
    // Serve minimal HTML page optimized for ESP8266 low RAM - send in chunks
    server->on("/", HTTP_GET, []() {
        server->setContentLength(CONTENT_LENGTH_UNKNOWN);
        server->send(200, "text/html", "");
        
        // Header chunk
        server->sendContent(F("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
        "<title>RailHub8266</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Arial,sans-serif;background:#1a1a1a;color:#e0e0e0;padding:15px;max-width:1200px;margin:0 auto}"
        ".card{background:#2a2a2a;border:1px solid #3a3a3a;padding:15px;margin-bottom:15px;border-radius:8px}h1{font-size:1.5rem;margin-bottom:10px}h2{font-size:1.2rem;margin-bottom:10px}"
        ".status{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px;margin-bottom:20px}.stat{background:#333;padding:12px;text-align:center;border-radius:6px}"
        ".value{font-size:1.5rem;color:#6c9bcf}.label{font-size:0.8rem;color:#999;margin-top:5px}"
        ".outputs{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:10px}.output{background:#333;padding:12px;display:flex;flex-direction:column;gap:10px;border-radius:6px}"
        ".output-header{display:flex;justify-content:space-between;align-items:center;gap:10px}"
        ".output-controls{display:flex;flex-direction:column;gap:8px;width:100%}"
        ".output.on{border-left:4px solid #4a9b6f}.output.blinking{border-left:4px solid #f39c12}.toggle{width:60px;height:32px;background:#555;cursor:pointer;position:relative;border-radius:16px;flex-shrink:0}"
        ".toggle.on{background:#4a9b6f}.toggle::after{content:'';position:absolute;width:28px;height:28px;background:#fff;top:2px;left:2px;transition:0.2s;border-radius:50%}"
        ".toggle.on::after{left:30px}.brightness{display:flex;align-items:center;gap:10px}"
        ".brightness input{flex:1;height:8px;border-radius:4px;background:#555;outline:none;-webkit-appearance:none;min-width:0}"
        ".brightness input::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#6c9bcf;cursor:pointer;box-shadow:0 2px 4px rgba(0,0,0,0.3)}"
        ".brightness input::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:#6c9bcf;cursor:pointer;border:none;box-shadow:0 2px 4px rgba(0,0,0,0.3)}"
        ".brightness span{min-width:45px;text-align:right;font-size:0.9rem;color:#999}"
        ".interval{display:flex;align-items:center;gap:8px;flex-wrap:wrap}"
        ".interval input{width:80px;padding:6px 8px;background:#555;border:1px solid #666;color:#fff;border-radius:4px;font-size:0.9rem}"
        ".interval span{font-size:0.85rem;color:#999}"
        ".output.chasing{border-left:4px solid #9b59b6}"
        ".chasing-group{background:#3a2a4a;padding:12px;margin-bottom:10px;border-left:4px solid #9b59b6;border-radius:6px}"
        ".chasing-group h3{font-size:1rem;margin-bottom:8px;color:#bb79d6;cursor:pointer;word-break:break-word;display:flex;align-items:center;gap:8px}"
        ".chasing-group h3:hover{color:#d699f0}"
        ".chasing-group h3::before{content:'⚡';font-size:1.1rem}"
        ".group-info{font-size:0.85rem;color:#b8b8b8;word-break:break-word;margin-bottom:10px;line-height:1.4}"
        ".group-controls{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}"
        ".output-name{cursor:pointer;font-weight:bold;color:#6c9bcf;word-break:break-word;flex:1}"
        ".output-name:hover{color:#8bb5e0;text-decoration:underline}"
        "button{background:#6c9bcf;color:#fff;border:none;padding:10px 20px;cursor:pointer;margin:5px 5px 5px 0;border-radius:6px;font-size:0.95rem;touch-action:manipulation;transition:background 0.3s,transform 0.1s}"
        "button:hover{background:#5a8bc0}button:active{transform:scale(0.98)}button.processing{background:#4caf50!important;cursor:wait;transform:scale(1)!important}button.processing::before{content:'✓ ';font-size:1.3rem;font-weight:bold}"
        "button.state-match{background:#4caf50!important;color:#fff;box-shadow:0 0 0 2px rgba(255,255,255,0.15) inset}"
        "button:disabled{opacity:0.8;cursor:wait}button.delete{background:#e74c3c}button.delete:hover{background:#c0392b}"
        ".info{font-size:0.9rem;color:#999}"
        ".tabs{display:flex;gap:5px;margin-bottom:15px;overflow-x:auto;-webkit-overflow-scrolling:touch}.tab{background:#333;padding:12px 20px;cursor:pointer;border:none;color:#999;white-space:nowrap;border-radius:6px 6px 0 0;touch-action:manipulation}"
        ".tab.active{background:#6c9bcf;color:#fff}.tab-content{display:none}.tab-content.active{display:block}"
        ".storage-bar{background:#333;height:24px;border-radius:4px;overflow:hidden;margin-top:5px;position:relative}"
        ".storage-fill{background:linear-gradient(90deg,#4a9b6f,#f39c12);height:100%;transition:width 0.3s}"
        ".storage-text{position:absolute;top:4px;left:0;right:0;text-align:center;font-size:0.75rem;color:#fff;text-shadow:1px 1px 2px rgba(0,0,0,0.8)}"
        ".modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.8);z-index:1000;align-items:center;justify-content:center;padding:20px}"
        ".modal.show{display:flex}"
        ".modal-content{background:#2a2a3a;padding:20px;border-radius:12px;width:100%;max-width:400px;box-shadow:0 4px 20px rgba(0,0,0,0.5)}"
        ".modal-header{font-size:1.2rem;font-weight:bold;margin-bottom:15px;color:#6c9bcf}"
        ".modal-input{width:100%;padding:12px;background:#555;border:1px solid #666;color:#fff;border-radius:6px;font-size:1rem;margin-bottom:15px}"
        ".modal-input:focus{outline:none;border-color:#6c9bcf}"
        ".modal-buttons{display:flex;gap:10px;justify-content:flex-end;flex-wrap:wrap}"
        ".modal-buttons button{min-width:80px;flex:1}"
        ".modal-buttons .cancel{background:#666}"
        ".modal-buttons .cancel:hover{background:#555}"
        ".control-buttons{display:flex;flex-wrap:wrap;gap:5px}"
        ".form-group{margin-bottom:15px}"
        ".form-group label{display:block;margin-bottom:5px;color:#999;font-size:0.9rem}"
        ".form-group input[type=number],.form-group input[type=text]{width:100%;max-width:200px;padding:8px;background:#555;border:1px solid #666;color:#fff;border-radius:4px;font-size:0.95rem}"
        ".checkbox-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:8px;padding:8px;background:#333;border-radius:4px}"
        ".checkbox-label{display:flex;align-items:center;gap:10px;padding:10px 12px;background:#444;border-radius:4px;cursor:pointer;transition:background 0.2s}"
        ".checkbox-label:hover:not(.disabled){background:#505050}"
        ".checkbox-label input[type=checkbox]{appearance:none;-webkit-appearance:none;cursor:pointer;width:20px;height:20px;margin:0;flex-shrink:0;background:#555;border:2px solid #666;border-radius:4px;transition:all 0.2s;display:flex;align-items:center;justify-content:center}"
        ".checkbox-label input[type=checkbox]:checked{background:#6c9bcf;border-color:#6c9bcf}"
        ".checkbox-label input[type=checkbox]:checked::before{content:'✓';color:#fff;font-size:14px;font-weight:bold;line-height:1}"
        ".checkbox-label input[type=checkbox]:disabled{opacity:0.4;cursor:not-allowed}"
        ".checkbox-label.disabled{opacity:0.5;cursor:not-allowed}"
        ".checkbox-label span{line-height:1.3;word-break:break-word;font-size:0.9rem}"
        ".no-groups{text-align:center;padding:20px;color:#666;font-style:italic}"
        "@media(min-width:768px){.output{flex-direction:row}.output-header{flex:0 0 auto}.output-controls{width:auto;flex:1}}"
        "@media(max-width:480px){body{padding:10px}.card{padding:12px}h1{font-size:1.3rem}h2{font-size:1.1rem}button{padding:8px 16px;font-size:0.9rem}.toggle{width:50px;height:28px}.toggle::after{width:24px;height:24px}.toggle.on::after{left:24px}.stat{padding:10px}.value{font-size:1.3rem}}"
        "</style></head><body>"));
        
        // Modal dialog for name editing
        server->sendContent(F("<div id='nameModal' class='modal'><div class='modal-content'>"
        "<div class='modal-header' id='modalTitle'>Edit Name</div>"
        "<input type='text' id='modalInput' class='modal-input' maxlength='20' placeholder='Enter name...'>"
        "<div class='modal-buttons'>"
        "<button class='cancel' onclick='closeModal()'>Cancel</button>"
        "<button onclick='saveModalName()'>Save</button>"
        "</div></div></div>"));
        
        // Confirmation modal
        server->sendContent(F("<div id='confirmModal' class='modal'><div class='modal-content'>"
        "<div class='modal-header' id='confirmTitle'>Confirm</div>"
        "<div id='confirmMessage' style='margin-bottom:20px;color:#ccc'></div>"
        "<div class='modal-buttons'>"
        "<button class='cancel' onclick='closeConfirm()'>Cancel</button>"
        "<button class='delete' onclick='confirmYes()'>Delete</button>"
        "</div></div></div>"));
        
        // Alert modal
        server->sendContent(F("<div id='alertModal' class='modal'><div class='modal-content'>"
        "<div class='modal-header' id='alertTitle'>Alert</div>"
        "<div id='alertMessage' style='margin-bottom:20px;color:#ccc'></div>"
        "<div class='modal-buttons'>"
        "<button onclick='closeAlert()'>OK</button>"
        "</div></div></div>"));
        
        // Body start
        server->sendContent(F("<div class='card'><h1>🚂 RailHub8266</h1><p class='info'>"));
        server->sendContent(String(customDeviceName));
        server->sendContent(F("</p></div><div class='card'><div class='tabs'>"
        "<button class='tab active' onclick='showTab(0)'>Status</button>"
        "<button class='tab' onclick='showTab(1)'>Settings</button>"
        "</div><div class='tab-content active' id='tab0'><h2>Status</h2><div class='status'>"
        "<div class='stat'><div class='value' id='uptime'>-</div><div class='label'>Uptime</div></div>"
        "<div class='stat'><div class='value' id='buildDate'>-</div><div class='label'>Build Date</div></div>"
        "</div><div style='margin-top:15px'><div class='label'>RAM (80 KB)</div>"
        "<div class='storage-bar'><div class='storage-fill' id='ramFill' style='width:0%'></div>"
        "<div class='storage-text' id='ramText'>-</div></div></div>"
        "<div style='margin-top:15px'><div class='label'>Program Flash (1 MB)</div>"
        "<div class='storage-bar'><div class='storage-fill' id='storageFill' style='width:0%'></div>"
        "<div class='storage-text' id='storageText'>-</div></div></div>"
        "<div style='margin-top:20px'><h2>Controls</h2>"
        "<div class='control-buttons'><button id='btnAllOn' onclick='allOn()'>All ON</button><button id='btnAllOff' onclick='allOff()'>All OFF</button></div>"
        "<div class='brightness' style='margin-top:15px'><label style='display:block;margin-bottom:5px;color:#999;font-size:0.9rem'>Master Brightness:</label>"
        "<input type='range' min='0' max='100' value='100' id='masterBrightness' oninput='this.nextElementSibling.textContent=this.value+\"%\"' onchange='setMasterBrightness(this.value)'>"
        "<span style='color:#6c9bcf;font-weight:bold'>100%</span></div>"
        "</div></div><div class='tab-content' id='tab1'><h2>Chasing Light Groups</h2>"
        "<div style='background:#333;padding:15px;border-radius:6px;margin-bottom:15px'>"
        "<div class='form-group'><label>Group ID:</label>"
        "<input type='number' id='newGroupId' min='1' max='255' value='1'></div>"
        "<div class='form-group'><label>Interval (ms):</label>"
        "<input type='text' id='newGroupInterval' value='500'></div>"
        "<div class='form-group'><label>Select Outputs (min. 2):</label>"
        "<div id='outputSelector' class='checkbox-grid'></div></div>"
        "<button onclick='createGroup()'>Create Group</button>"
        "</div><div id='chasingGroups'></div>"
        "<h2 style='margin-top:20px'>Outputs</h2><div class='outputs' id='outputs'></div></div></div>"));
        
        // JavaScript chunk
        server->sendContent(F("<script>function showTab(n){localStorage.setItem('activeTab',n);document.querySelectorAll('.tab').forEach((t,i)=>t.classList.toggle('active',i===n));"
        "document.querySelectorAll('.tab-content').forEach((c,i)=>c.classList.toggle('active',i===n));}"
        "let wsData=null;let bulkState=null;async function load(){let d;if(wsData){d=wsData;wsData=null;}else{try{const r=await fetch('/api/status');d=await r.json();}catch(err){console.error('[LOAD] Error:',err);return;}}if(!d)return;try{const activeEl=document.activeElement;const isFocused=activeEl&&activeEl.tagName==='INPUT'&&activeEl.type==='text'&&activeEl.closest('.interval');"
        "const focusedPin=isFocused?activeEl.closest('.output')?.querySelector('.output-name')?.getAttribute('onclick')?.match(/\\d+/)?.[0]:null;"
        "const cursorPos=isFocused?activeEl.selectionStart:null;const focusedVal=isFocused?activeEl.value:null;"
        "const usedRam=80-(d.freeHeap/1024);const ramPct=Math.round((usedRam/80)*100);"
        "document.getElementById('ramFill').style.width=ramPct+'%';"
        "document.getElementById('ramText').textContent=usedRam.toFixed(1)+'KB / 80KB ('+ramPct+'%)';"
        "const s=Math.floor(d.uptime/1000);document.getElementById('uptime').textContent=s+'s';"
        "if(d.buildDate)document.getElementById('buildDate').textContent=d.buildDate;"
        "if(d.flashUsed&&d.flashPartition){const pct=Math.round((d.flashUsed/d.flashPartition)*100);"
        "document.getElementById('storageFill').style.width=pct+'%';"
        "document.getElementById('storageText').textContent=(d.flashUsed/1024).toFixed(0)+'KB / '+(d.flashPartition/1024).toFixed(0)+'KB ('+pct+'%)';}"
        "const sel=document.getElementById('outputSelector');"
        "const checked=[];document.querySelectorAll('#outputSelector input:checked').forEach(cb=>checked.push(cb.value));"
        "sel.innerHTML='';"
        "d.outputs.forEach(out=>{"
        "const lbl=document.createElement('label');lbl.className='checkbox-label';"
        "if(out.chasingGroup>=0)lbl.classList.add('disabled');"
        "const cb=document.createElement('input');cb.type='checkbox';cb.value=out.pin;cb.id='out_'+out.pin;"
        "cb.disabled=out.chasingGroup>=0;"
        "if(out.chasingGroup<0&&checked.includes(out.pin.toString()))cb.checked=true;"
        "lbl.appendChild(cb);"
        "const outName=out.name||'GPIO '+out.pin;"
        "const span=document.createElement('span');span.textContent=outName;span.style.fontSize='0.85rem';"
        "lbl.appendChild(span);"
        "sel.appendChild(lbl);});"
        "const cg=document.getElementById('chasingGroups');cg.innerHTML='';"
        "if(d.chasingGroups&&d.chasingGroups.length>0){"
        "d.chasingGroups.forEach(g=>{"
        "const div=document.createElement('div');div.className='chasing-group';"
        "const outNames=g.outputs.map(pin=>{const o=d.outputs.find(x=>x.pin===pin);return o?(o.name||'GPIO '+pin):'GPIO '+pin;}).join(', ');"
        "div.innerHTML=`<h3 onclick='editGName(${g.groupId},\"${g.name}\")'>${g.name}</h3>"
        "<div class='group-info'><strong>Outputs:</strong> ${outNames}<br><strong>Interval:</strong> ${g.interval}ms</div>"
        "<div class='group-controls'><button class='delete' onclick='deleteGroup(${g.groupId})'>Delete Group</button></div>`;"
        "cg.appendChild(div);});}else{cg.innerHTML='<div class=\"no-groups\">No active groups</div>';}"
        "const o=document.getElementById('outputs');o.innerHTML='';"
        "d.outputs.forEach((out,i)=>{"
        "const div=document.createElement('div');"
        "let cls='output'+(out.active?' on':'')+(out.interval>0?' blinking':'')+(out.chasingGroup>=0?' chasing':'');"
        "div.className=cls;"
        "let groupTag='';"
        "if(out.chasingGroup>=0){const grp=d.chasingGroups.find(g=>g.groupId===out.chasingGroup);groupTag=grp?' ['+grp.name+']':' [G'+out.chasingGroup+']';}"
        "div.innerHTML=`<div class='output-header'><span class='output-name' onclick='editOName(${out.pin},\"${out.name}\")'>${out.name || 'GPIO '+out.pin}${groupTag}</span>"
        "<div class='toggle ${out.active?'on':''}' onclick='tog(${out.pin})'></div></div>"
        "<div class='output-controls'><div class='brightness'><input type='range' min='0' max='100' value='${out.brightness}' "
        "oninput='this.nextElementSibling.textContent=this.value+\"%\"' onchange='setBright(${out.pin},this.value)'>"
        "<span>${out.brightness}%</span></div>"
        "<div class='interval'><span>Interval:</span><input type='text' value='${out.interval}' "
        "onchange='setInt(${out.pin},this.value)' ${out.chasingGroup>=0?'disabled':''}><span>ms</span></div></div>`;"
        "o.appendChild(div);});"
        "if(focusedPin){const inputs=document.querySelectorAll('.interval input[type=text]');"
        "inputs.forEach(inp=>{const pin=inp.closest('.output')?.querySelector('.output-name')?.getAttribute('onclick')?.match(/\\d+/)?.[0];"
        "if(pin===focusedPin){inp.focus();if(cursorPos!==null){inp.setSelectionRange(cursorPos,cursorPos);inp.value=focusedVal||inp.value;}}});}"
        "const btnOn=document.getElementById('btnAllOn');const btnOff=document.getElementById('btnAllOff');"
        "const everyOn=d.outputs.length>0&&d.outputs.every(out=>out.active);"
        "const everyOff=d.outputs.length>0&&d.outputs.every(out=>!out.active);"
        "bulkState=everyOn?'on':everyOff?'off':null;"
        "if(btnOn)btnOn.classList.toggle('state-match',bulkState==='on');"
        "if(btnOff)btnOff.classList.toggle('state-match',bulkState==='off');"
        "}catch(e){console.error(e);}}"));
        
        server->sendContent(F("async function tog(pin){try{const r=await fetch('/api/status');const d=await r.json();"
        "const out=d.outputs.find(o=>o.pin===pin);await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({pin:pin,active:!out.active,brightness:out.brightness})});load();}catch(e){console.error(e);}}"));
        
        server->sendContent(F("async function setBright(pin,val){try{const r=await fetch('/api/status');const d=await r.json();"
        "const out=d.outputs.find(o=>o.pin===pin);await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({pin:pin,active:out.active,brightness:parseInt(val)})});}catch(e){console.error(e);}}"));
        
        server->sendContent(F("async function setInt(pin,val){try{await fetch('/api/interval',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({pin:pin,interval:parseInt(val)||0})});}catch(e){console.error(e);}}"));
        
        server->sendContent(F("let confirmCallback=null;function openConfirm(title,message,callback){"
        "document.getElementById('confirmTitle').textContent=title;"
        "document.getElementById('confirmMessage').textContent=message;"
        "confirmCallback=callback;"
        "document.getElementById('confirmModal').classList.add('show');}"
        "function closeConfirm(){document.getElementById('confirmModal').classList.remove('show');confirmCallback=null;}"
        "function confirmYes(){if(confirmCallback){confirmCallback();}closeConfirm();}"
        "document.getElementById('confirmModal').addEventListener('click',e=>{"
        "if(e.target.id==='confirmModal'){closeConfirm();}});"));
        
        server->sendContent(F("function showAlert(title,message){"
        "document.getElementById('alertTitle').textContent=title;"
        "document.getElementById('alertMessage').textContent=message;"
        "document.getElementById('alertModal').classList.add('show');}"
        "function closeAlert(){document.getElementById('alertModal').classList.remove('show');}"
        "document.getElementById('alertModal').addEventListener('click',e=>{"
        "if(e.target.id==='alertModal'){closeAlert();}});"));
        
        server->sendContent(F("async function deleteGroup(gid){"
        "openConfirm('Delete Group','Are you sure you want to delete this chasing group?',async()=>{"
        "try{await fetch('/api/chasing/delete',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({groupId:gid})});load();}catch(e){console.error(e);}});}"));
        
        server->sendContent(F("let modalCallback=null;function openModal(title,currentVal,callback){"
        "document.getElementById('modalTitle').textContent=title;"
        "const input=document.getElementById('modalInput');"
        "input.value=currentVal||'';"
        "modalCallback=callback;"
        "document.getElementById('nameModal').classList.add('show');"
        "setTimeout(()=>input.focus(),100);}"
        "function closeModal(){document.getElementById('nameModal').classList.remove('show');modalCallback=null;}"
        "function saveModalName(){const val=document.getElementById('modalInput').value.trim();"
        "if(modalCallback){modalCallback(val);}closeModal();}"
        "document.getElementById('modalInput').addEventListener('keydown',e=>{"
        "if(e.key==='Enter'){saveModalName();}else if(e.key==='Escape'){closeModal();}});"
        "document.getElementById('nameModal').addEventListener('click',e=>{"
        "if(e.target.id==='nameModal'){closeModal();}});"));
        
        server->sendContent(F("async function editGName(gid,oldName){"
        "openModal('Edit Group Name',oldName,async(name)=>{"
        "if(name===oldName)return;"
        "const finalName=name.trim()||'Group '+gid;"
        "try{await fetch('/api/chasing/name',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({groupId:gid,name:finalName})});load();}catch(e){showAlert('Error',e.toString());console.error(e);}});}"));
        
        server->sendContent(F("async function editOName(pin,oldName){"
        "openModal('Edit Output Name',oldName||'GPIO '+pin,async(name)=>{"
        "const finalName=name.trim();"
        "if(finalName===(oldName||'GPIO '+pin))return;"
        "try{await fetch('/api/name',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({pin:pin,name:finalName})});load();}catch(e){showAlert('Error',e.toString());console.error(e);}});}"));
        
        server->sendContent(F("async function createGroup(){try{"
        "const gid=parseInt(document.getElementById('newGroupId').value);"
        "const interval=parseInt(document.getElementById('newGroupInterval').value);"
        "const outputs=[];"
        "document.querySelectorAll('#outputSelector input[type=checkbox]:checked').forEach(cb=>outputs.push(parseInt(cb.value)));"
        "if(outputs.length<2){showAlert('Validation Error','Please select at least 2 outputs');return;}"
        "if(gid<1||gid>255){showAlert('Validation Error','Group ID must be 1-255');return;}"
        "if(interval<50){showAlert('Validation Error','Interval must be at least 50ms');return;}"
        "await fetch('/api/chasing/create',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({groupId:gid,interval:interval,outputs:outputs})});"
        "document.getElementById('newGroupId').value=parseInt(gid)+1;load();}catch(e){showAlert('Error',e.toString());console.error(e);}}"));;
        
        server->sendContent(F("let isProcessing=false;async function allOn(){const btn=document.getElementById('btnAllOn');if(isProcessing)return;isProcessing=true;"
        "bulkState='on';btn.classList.add('processing');btn.disabled=true;try{const r=await fetch('/api/status');const d=await r.json();"
        "for(const o of d.outputs){await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({pin:o.pin,active:true,brightness:100})});}}catch(e){console.error(e);}finally{await new Promise(r=>setTimeout(r,2000));"
        "btn.classList.remove('processing');btn.disabled=false;isProcessing=false;}}"));
        
        server->sendContent(F("async function allOff(){const btn=document.getElementById('btnAllOff');if(isProcessing)return;isProcessing=true;"
        "bulkState='off';btn.classList.add('processing');btn.disabled=true;try{const r=await fetch('/api/status');const d=await r.json();"
        "for(const o of d.outputs){await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({pin:o.pin,active:false,brightness:0})});}}catch(e){console.error(e);}finally{await new Promise(r=>setTimeout(r,2000));"
        "btn.classList.remove('processing');btn.disabled=false;isProcessing=false;}}"));
        
        server->sendContent(F("async function setMasterBrightness(val){try{const r=await fetch('/api/status');const d=await r.json();"
        "for(const o of d.outputs){if(o.active){await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({pin:o.pin,active:true,brightness:parseInt(val)})});}}}catch(e){console.error(e);}}"));
        
        server->sendContent(F("let ws;function connectWS(){const wsUrl='ws://'+window.location.hostname+':81';"
        "ws=new WebSocket(wsUrl);ws.onopen=()=>{console.log('[WS] Connected');};"
        "ws.onmessage=(e)=>{try{wsData=JSON.parse(e.data);if(!isProcessing){load();}}catch(err){console.error('[WS] Parse error:',err);}};"
        "ws.onerror=(e)=>{console.error('[WS] Error:',e);};"
        "ws.onclose=()=>{console.log('[WS] Disconnected, reconnecting...');setTimeout(connectWS,2000);}};"
        "const savedTab=localStorage.getItem('activeTab');if(savedTab!==null){showTab(parseInt(savedTab));}load().then(()=>connectWS());</script>"
        "<footer style='text-align:center;padding:20px;margin-top:40px;border-top:1px solid #333;color:#666;font-size:0.9em;'>Made with ❤️ by innoMO</footer>"
        "</body></html>"));
        server->sendContent("");  // End chunked transfer
    });
    
    // API endpoint for status
    server->on("/api/status", HTTP_GET, []() {
        unsigned long startTime = millis();
        IPAddress clientIP = server->client().remoteIP();
        Serial.print("[WEB] GET /api/status from ");
        Serial.println(clientIP.toString());
        
        DynamicJsonDocument doc(2048);
        doc["macAddress"] = macAddress;
        doc["name"] = customDeviceName;
        doc["wifiMode"] = WiFi.getMode() == WIFI_AP ? "AP" : "STA";
        doc["ip"] = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        doc["ssid"] = WiFi.getMode() == WIFI_AP ? String(AP_SSID) : WiFi.SSID();
        doc["apClients"] = WiFi.softAPgetStationNum();
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["uptime"] = millis();
        doc["flashTotal"] = ESP.getFlashChipSize();
        doc["flashUsed"] = ESP.getSketchSize();
        doc["flashFree"] = ESP.getFreeSketchSpace();
        
        JsonArray outputs = doc.createNestedArray("outputs");
        for (int i = 0; i < MAX_OUTPUTS; i++) {
            JsonObject output = outputs.createNestedObject();
            output["pin"] = outputPins[i];
            output["active"] = outputStates[i];
            output["brightness"] = map(outputBrightness[i], 0, 255, 0, 100);
            output["name"] = outputNames[i];
            output["interval"] = outputIntervals[i];
            output["chasingGroup"] = outputChasingGroup[i];
        }
        
        // Add chasing groups info
        JsonArray groups = doc.createNestedArray("chasingGroups");
        for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
            if (chasingGroups[i].active) {
                JsonObject group = groups.createNestedObject();
                group["groupId"] = chasingGroups[i].groupId;
                group["name"] = chasingGroups[i].name;
                group["interval"] = chasingGroups[i].interval;
                group["outputCount"] = chasingGroups[i].outputCount;
                JsonArray groupOutputs = group.createNestedArray("outputs");
                for (int j = 0; j < chasingGroups[i].outputCount; j++) {
                    groupOutputs.add(outputPins[chasingGroups[i].outputIndices[j]]);
                }
            }
        }
        
        String response;
        serializeJson(doc, response);
        
        unsigned long duration = millis() - startTime;
        Serial.print("[WEB] Status response: ");
        Serial.print(response.length());
        Serial.print(" bytes, ");
        Serial.print(duration);
        Serial.println("ms");
        
        server->send(200, "application/json", response);
    });
    
    // API endpoint for updating output name
    server->on("/api/name", HTTP_POST, []() {
        unsigned long startTime = millis();
        IPAddress clientIP = server->client().remoteIP();
        String body = server->arg("plain");
        Serial.print("[WEB] POST /api/name from ");
        Serial.print(clientIP.toString());
        Serial.print(" (");
        Serial.print(body.length());
        Serial.println(" bytes)");
        
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            Serial.print("[ERROR] JSON deserialization failed: ");
            Serial.println(error.c_str());
            server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        int pin = doc["pin"];
        String name = doc["name"].as<String>();
        
        Serial.print("[WEB] Name update request: GPIO ");
        Serial.print(pin);
        Serial.print(" -> '");
        Serial.print(name);
        Serial.println("'");
        
        // Find output index by pin
        int outputIndex = -1;
        for (int i = 0; i < MAX_OUTPUTS; i++) {
            if (outputPins[i] == pin) {
                outputIndex = i;
                break;
            }
        }
        
        if (outputIndex >= 0) {
            saveOutputName(outputIndex, name);
            unsigned long duration = millis() - startTime;
            Serial.print("[WEB] Name update complete (");
            Serial.print(duration);
            Serial.println("ms)");
            broadcastStatus();
            server->send(200, "application/json", "{\"success\":true}");
        } else {
            Serial.print("[ERROR] GPIO pin not found: ");
            Serial.println(pin);
            server->send(404, "application/json", "{\"error\":\"Output not found\"}");
        }
    });
    
    // API endpoint for updating output blink interval
    server->on("/api/interval", HTTP_POST, []() {
        unsigned long startTime = millis();
        IPAddress clientIP = server->client().remoteIP();
        String body = server->arg("plain");
        Serial.print("[WEB] POST /api/interval from ");
        Serial.print(clientIP.toString());
        Serial.print(" (");
        Serial.print(body.length());
        Serial.println(" bytes)");
        
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            Serial.print("[ERROR] JSON deserialization failed: ");
            Serial.println(error.c_str());
            server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        int pin = doc["pin"];
        unsigned int interval = doc["interval"];
        
        Serial.print("[WEB] Interval update request: GPIO ");
        Serial.print(pin);
        Serial.print(" -> ");
        Serial.print(interval);
        Serial.println("ms");
        
        // Find output index by pin
        int outputIndex = -1;
        for (int i = 0; i < MAX_OUTPUTS; i++) {
            if (outputPins[i] == pin) {
                outputIndex = i;
                break;
            }
        }
        
        if (outputIndex >= 0) {
            setOutputInterval(outputIndex, interval);
            unsigned long duration = millis() - startTime;
            Serial.print("[WEB] Interval update complete (");
            Serial.print(duration);
            Serial.println("ms)");
            broadcastStatus();
            server->send(200, "application/json", "{\"success\":true}");
        } else {
            Serial.print("[ERROR] GPIO pin not found: ");
            Serial.println(pin);
            server->send(404, "application/json", "{\"error\":\"Output not found\"}");
        }
    });
    
    // API endpoint for control
    server->on("/api/control", HTTP_POST, []() {
        unsigned long startTime = millis();
        IPAddress clientIP = server->client().remoteIP();
        String body = server->arg("plain");
        Serial.print("[WEB] POST /api/control from ");
        Serial.print(clientIP.toString());
        Serial.print(" (");
        Serial.print(body.length());
        Serial.println(" bytes)");
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            Serial.print("[ERROR] JSON deserialization failed: ");
            Serial.println(error.c_str());
            server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        int pin = doc["pin"];
        bool active = doc["active"];
        int brightness = doc["brightness"] | 100;
        
        Serial.print("[WEB] Control request: GPIO ");
        Serial.print(pin);
        Serial.print(" -> ");
        Serial.print(active ? "ON" : "OFF");
        Serial.print(" @ ");
        Serial.print(brightness);
        Serial.println("%");
        
        executeOutputCommand(pin, active, brightness);
        
        unsigned long duration = millis() - startTime;
        Serial.print("[WEB] Control complete (");
        Serial.print(duration);
        Serial.println("ms)");
        
        server->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API endpoint for creating chasing group
    server->on("/api/chasing/create", HTTP_POST, []() {
        unsigned long startTime = millis();
        IPAddress clientIP = server->client().remoteIP();
        String body = server->arg("plain");
        Serial.print("[WEB] POST /api/chasing/create from ");
        Serial.print(clientIP.toString());
        Serial.print(" (");
        Serial.print(body.length());
        Serial.println(" bytes)");
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            Serial.print("[ERROR] JSON deserialization failed: ");
            Serial.println(error.c_str());
            server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        uint8_t groupId = doc["groupId"];
        unsigned int interval = doc["interval"];
        JsonArray outputs = doc["outputs"];
        const char* groupName = doc.containsKey("name") ? doc["name"].as<const char*>() : nullptr;
        
        if (outputs.size() == 0 || outputs.size() > 8) {
            server->send(400, "application/json", "{\"error\":\"Invalid output count (1-8)\"}");
            return;
        }
        
        // Convert output pins to indices
        uint8_t outputIndices[8];
        uint8_t count = 0;
        for (JsonVariant v : outputs) {
            int pin = v.as<int>();
            // Find output index by pin
            for (int i = 0; i < MAX_OUTPUTS; i++) {
                if (outputPins[i] == pin) {
                    outputIndices[count++] = i;
                    break;
                }
            }
        }
        
        if (count != outputs.size()) {
            server->send(400, "application/json", "{\"error\":\"Invalid GPIO pin(s)\"}");
            return;
        }
        
        createChasingGroup(groupId, outputIndices, count, interval, groupName);
        
        unsigned long duration = millis() - startTime;
        Serial.print("[WEB] Chasing group created (");
        Serial.print(duration);
        Serial.println("ms)");
        
        server->send(200, "application/json", "{\"success\":true}");
    });
    
    // API endpoint for deleting chasing group
    server->on("/api/chasing/delete", HTTP_POST, []() {
        IPAddress clientIP = server->client().remoteIP();
        String body = server->arg("plain");
        Serial.print("[WEB] POST /api/chasing/delete from ");
        Serial.println(clientIP.toString());
        
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        uint8_t groupId = doc["groupId"];
        deleteChasingGroup(groupId);
        
        server->send(200, "application/json", "{\"success\":true}");
    });
    
    // API endpoint for updating chasing group name
    server->on("/api/chasing/name", HTTP_POST, []() {
        IPAddress clientIP = server->client().remoteIP();
        String body = server->arg("plain");
        Serial.print("[WEB] POST /api/chasing/name from ");
        Serial.println(clientIP.toString());
        
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        uint8_t groupId = doc["groupId"];
        const char* newName = doc["name"];
        
        // If name is empty or null, use default "Group X"
        char finalName[21];
        if (newName == nullptr || strlen(newName) == 0) {
            snprintf(finalName, sizeof(finalName), "Group %d", groupId);
        } else {
            strncpy(finalName, newName, 20);
            finalName[20] = '\0';
        }
        
        // Find and update group
        bool found = false;
        for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
            if (chasingGroups[i].active && chasingGroups[i].groupId == groupId) {
                strncpy(chasingGroups[i].name, finalName, 20);
                chasingGroups[i].name[20] = '\0';
                saveChasingGroups();
                found = true;
                Serial.print("[CHASING] Updated group ");
                Serial.print(groupId);
                Serial.print(" name to '");
                Serial.print(finalName);
                Serial.println("'");
                break;
            }
        }
        
        if (found) {
            broadcastStatus();
            server->send(200, "application/json", "{\"success\":true}");
        } else {
            server->send(404, "application/json", "{\"error\":\"Group not found\"}");
        }
    });
    
    // API endpoint to reset saved states
    server->on("/api/reset", HTTP_POST, []() {
        IPAddress clientIP = server->client().remoteIP();
        Serial.print("[WEB] POST /api/reset from ");
        Serial.println(clientIP.toString());
        Serial.println("[EEPROM] Resetting all saved states...");
        Serial.print("[EEPROM] Free heap before reset: ");
        Serial.print(ESP.getFreeHeap());
        Serial.println(" bytes");
        
        // Clear EEPROM data
        for (int i = 0; i < EEPROM_SIZE; i++) {
            EEPROM.write(i, 0xFF);
        }
        EEPROM.commit();
        
        Serial.println("[EEPROM] All saved states cleared!");
        Serial.print("[EEPROM] Free heap after reset: ");
        Serial.print(ESP.getFreeHeap());
        Serial.println(" bytes");
        
        server->send(200, "application/json", "{\"status\":\"reset_complete\"}");
    });
    
    server->begin();
    Serial.println("[WEB] Web server started on port 80");
    Serial.println("[WEB] Available endpoints:");
    Serial.println("[WEB]   GET  /                   - Main control interface");
    Serial.println("[WEB]   GET  /api/status         - System and output status");
    Serial.println("[WEB]   POST /api/control        - Control output state/brightness");
    Serial.println("[WEB]   POST /api/name           - Update output name");
    Serial.println("[WEB]   POST /api/interval       - Set output blink interval");
    Serial.println("[WEB]   POST /api/chasing/create - Create chasing light group");
    Serial.println("[WEB]   POST /api/chasing/delete - Delete chasing light group");
    Serial.println("[WEB]   POST /api/reset          - Reset all saved preferences");
}