#ifndef CONFIG_H
#define CONFIG_H

// WiFi Access Point Configuration
#define AP_SSID "RailHub8266-AP"         // Access Point name
#define AP_PASSWORD "RailHub8266Pass"    // Access Point password (min 8 characters)
#define AP_CHANNEL 6                     // WiFi channel (1-13)
#define AP_HIDDEN false                  // Set to true to hide SSID
#define AP_MAX_CONNECTIONS 4             // Maximum number of simultaneous connections

// Access Point IP Configuration
#define AP_LOCAL_IP "192.168.4.1"        // ESP8266 IP address
#define AP_GATEWAY "192.168.4.1"         // Gateway IP
#define AP_SUBNET "255.255.255.0"        // Subnet mask

// Device Configuration
#define DEVICE_NAME "ESP8266-Controller-01"
#define MAX_OUTPUTS 7                    // ESP8266 - using 7 outputs (GPIO 0 reserved for boot button)

// WiFiManager Configuration
#define WIFIMANAGER_AP_SSID "RailHub8266-Setup"  // Configuration portal AP name
#define WIFIMANAGER_AP_PASSWORD "12345678"       // AP password (min 8 characters)
#define WIFIMANAGER_TIMEOUT 180                   // Configuration portal timeout in seconds (3 min)
#define PORTAL_TRIGGER_PIN 0                      // GPIO pin to trigger config portal (boot button)
#define PORTAL_TRIGGER_DURATION 3000              // Hold duration in ms to trigger portal

// Pin Definitions for ESP8266
// ESP8266 has limited PWM pins - using GPIO: 4(D2), 5(D1), 12(D6), 13(D7), 14(D5), 16(D8), 2(D4)
// Note: GPIO 0 is RESERVED for boot button (portal trigger)
// Note: GPIO 2 has boot mode constraints but works for output
#define LED_PINS {4, 5, 12, 13, 14, 16, 2}

// Status LED
#define STATUS_LED_PIN 2  // D4 on NodeMCU (built-in LED, active LOW)

// EEPROM Configuration
#define EEPROM_SIZE 512   // Allocate 512 bytes for configuration storage

#endif
