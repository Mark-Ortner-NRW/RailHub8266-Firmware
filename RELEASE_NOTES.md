## v1.0.1 - PlatformIO Library Registry Support

### Added
- Added `library.json` for PlatformIO Library Registry publication
- Package now available for easy installation via PlatformIO

### Features
- ✅ **7 PWM Outputs** with individual brightness control (0-100%)
- ✅ **WiFi Control** via web interface and REST API
- ✅ **Chasing Light Groups** (up to 4 sequential lighting effects)
- ✅ **WebSocket Real-Time Updates** (500ms broadcast interval)
- ✅ **EEPROM Persistence** (all settings survive power loss)
- ✅ **Multilingual Web Interface** (6 languages: EN, DE, FR, IT, ZH, HI)
- ✅ **WiFiManager Integration** with captive portal
- ✅ **mDNS Support** (access via `railhub8266.local`)
- ✅ **Blink Intervals** (per-output configurable)
- ✅ **Master Brightness Control**

### 📱 Interface Preview

<p align="center">
  <img src="images/iPhone-13-PRO-192.168.137.8.png" width="250" alt="Output Control" />
  <img src="images/iPhone-13-PRO-192.168.137.8 (1).png" width="250" alt="Settings Panel" />
  <img src="images/iPhone-13-PRO-192.168.137.8 (2).png" width="250" alt="Status Dashboard" />
</p>

*Responsive web interface with dark theme and real-time updates*

### Installation via PlatformIO

Add to your `platformio.ini`:

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
lib_deps = 
    Mark-Ortner-NRW/RailHub8266@^1.0.1
```

Or install via CLI:
```bash
pio pkg install --library "Mark-Ortner-NRW/RailHub8266@^1.0.1"
```

### Hardware Requirements
- ESP8266 module (ESP-12E, NodeMCU, Wemos D1 Mini)
- 7 PWM-capable GPIO pins (2, 4, 5, 12, 13, 14, 16)

### Dependencies
- ArduinoJson ^7.0.4
- WiFiManager ^2.0.17
- WebSockets ^2.4.1

**Full Changelog**: https://github.com/Mark-Ortner-NRW/RailHub8266-Firmware/compare/v1.0.0...v1.0.1

---

📦 **PlatformIO Registry**: Coming soon!  
📖 **Documentation**: See [README.md](https://github.com/Mark-Ortner-NRW/RailHub8266-Firmware/blob/main/README.md)  
🐛 **Issues**: https://github.com/Mark-Ortner-NRW/RailHub8266-Firmware/issues
