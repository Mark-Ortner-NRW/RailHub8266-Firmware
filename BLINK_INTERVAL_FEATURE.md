# Blink Interval Feature (v2.0)

## Overview
The blink interval feature allows each output to blink at configurable intervals, enabling signal lights, warning indicators, or decorative effects to operate automatically without external control. This feature is available on both ESP32 (RailHub32) and ESP8266 (RailHub8266) platforms.

## Features Added

### 1. **Data Structures**
- `outputIntervals[MAX_OUTPUTS]` - Stores blink interval in milliseconds (0 = no blink, solid output)
- `lastBlinkTime[MAX_OUTPUTS]` - Tracks last toggle time for each output
- `blinkState[MAX_OUTPUTS]` - Current internal blink state (on/off)

### 2. **Persistent Storage**

**ESP8266 (EEPROM):**
- Updated `EEPROMData` structure to include `outputIntervals[8]` (uint16_t array)
- Intervals are automatically saved and restored on power cycle
- Works seamlessly with existing save/load functionality
- 16 bytes additional EEPROM usage

**ESP32 (Preferences/NVRAM):**
- Individual keys for each output interval: `outInterval0` through `outInterval15`
- Automatic persistence on interval changes
- Integrated with existing Preferences namespace

### 3. **Blinking Logic**
- `updateBlinkingOutputs()` - Called in main loop to handle all blinking
- Non-blocking implementation using `millis()`
- Independent timing for each output
- Automatic state management

### 4. **API Endpoints**

#### **POST /api/interval**
Set the blink interval for an output.

**Request:**
```json
{
  "pin": 4,
  "interval": 500
}
```

**Response:**
```json
{
  "success": true
}
```

**Parameters:**
- `pin` (int) - GPIO pin number
- `interval` (unsigned int) - Blink interval in milliseconds
  - `0` = Disable blinking (solid output)
  - `10-65535` = Blink interval in ms (recommended minimum: 100ms)

**Behavior:**
- Interval is saved to EEPROM/NVRAM automatically
- Persists across reboots
- Independent of output brightness setting
- Works alongside manual on/off control

#### **GET /api/status** (Updated)
Now includes interval information for each output, plus WebSocket support.

**Response:**
```json
{
  "outputs": [
    {
      "pin": 4,
      "active": true,
      "brightness": 100,
      "name": "Signal Light",
      "interval": 500
    },
    {
      "pin": 5,
      "active": false,
      "brightness": 0,
      "name": "Platform Light",
      "interval": 0
    }
  ]
}
```

**Note**: Status updates are also broadcast via WebSocket every 500ms on port 81.

### 5. **Web Interface**
- Added interval input field for each output (0-65535ms range)
- Visual indicator: outputs with blinking enabled show **orange border**
- Real-time interval adjustment via slider or input box
- Status display shows "Blinking" or "Solid" based on interval value
- Persistent across page reloads (saved to EEPROM/NVRAM)
- Live updates via WebSocket when interval changes
- Intervals display in milliseconds with validation

**UI Elements:**
- Interval input: Number field with min=0, max=65535
- Visual feedback: Border color changes when blinking is active
- Integration with brightness controls
- Tooltips for guidance

## Usage Examples

### Example 1: Basic Blinking Signal Light
```
Set interval to 500ms -> Light blinks every 500ms (ON for 500ms, OFF for 500ms)
```

### Example 2: Slow Warning Light
```
Set interval to 2000ms -> Light blinks slowly every 2 seconds
```

### Example 3: Disable Blinking
```
Set interval to 0 -> Light stays solid (no blinking)
```

## Technical Details

### Timing Precision
- Uses `millis()` for timing - non-blocking
- Minimum practical interval: ~10ms
- Maximum interval: 65535ms (stored as uint16_t in EEPROM)

### Memory Usage
- **Additional RAM**: 
  - ESP32: ~48 bytes (3 arrays × 16 outputs × 1 byte average)
  - ESP8266: ~42 bytes (3 arrays × 8 outputs × 1.75 bytes average)
- **Additional EEPROM (ESP8266)**: 16 bytes (8 outputs × 2 bytes each)
- **Additional NVRAM (ESP32)**: ~32 bytes (16 key-value pairs)

### Performance
- Blinking update overhead: ~2-3µs per output per loop iteration
- No impact on web server responsiveness
- All outputs blink independently
- Non-blocking operation allows WebSocket and web server to run smoothly

## Web Interface Controls

Each output now displays:
1. **GPIO Pin Number**
2. **Brightness Slider** (0-100%)
3. **Interval Input** (0-10000ms)
   - 0 = Solid output
   - >0 = Blinking enabled
4. **Status Indicator** (Blink/Solid)
5. **Toggle Button** (ON/OFF)

### Visual Indicators
- **Green border**: Output ON, solid
- **Orange border**: Output ON, blinking enabled
- **No border**: Output OFF

## Configuration Notes

- Intervals are saved to EEPROM automatically
- Persist across power cycles and reboots
- Can be configured per-output independently
- Works with existing brightness control
- Compatible with WiFi configuration portal

## API Integration

### Controlling via HTTP
```bash
# Enable blinking at 1000ms interval
curl -X POST http://192.168.4.1/api/interval \
  -H "Content-Type: application/json" \
  -d '{"pin":4,"interval":1000}'

# Disable blinking (solid)
curl -X POST http://192.168.4.1/api/interval \
  -H "Content-Type: application/json" \
  -d '{"pin":4,"interval":0}'
```

## Compatibility
- Works with all existing features
- Compatible with brightness control
- No breaking changes to existing API
- Backwards compatible with existing EEPROM data
