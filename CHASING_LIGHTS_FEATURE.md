# Chasing Lights Feature (v2.0) - ESP8266 Exclusive

## Overview
The chasing lights feature enables sequential activation of multiple outputs in a loop, creating dynamic lighting effects for model railways, advertising signs, runway lights, and decorative displays. This feature is **exclusive to the ESP8266 (RailHub8266) platform** in v2.0.

## Key Features

- **Up to 4 Independent Groups**: Create multiple chasing sequences running simultaneously
- **Flexible Output Assignment**: Assign up to 8 outputs per group
- **Configurable Intervals**: Set step interval from 10ms to 65535ms per group
- **Custom Group Names**: Name each group for easy identification (up to 20 characters)
- **Persistent Storage**: All group configurations saved to EEPROM and restored on boot
- **Non-blocking Operation**: Chasing runs independently without affecting web server or other outputs
- **RESTful API**: Complete API for creating, managing, and deleting groups
- **Web UI Integration**: Dedicated Chasing tab in web interface

## Architecture

### Data Structures

```cpp
#define MAX_CHASING_GROUPS 4

// Chasing group structure
struct ChasingGroup {
    uint8_t groupId;              // Unique ID (0-3)
    bool active;                  // Group enabled/disabled
    char name[21];                // Group name (20 chars + null terminator)
    uint8_t outputIndices[8];     // Array of output indices (0-7)
    uint8_t outputCount;          // Number of outputs in sequence
    uint16_t interval;            // Step interval in milliseconds
    uint8_t currentStep;          // Current active output in sequence
    unsigned long lastStepTime;   // Last step change timestamp
};

// Global arrays
ChasingGroup chasingGroups[MAX_CHASING_GROUPS];
uint8_t chasingGroupCount = 0;
int8_t outputChasingGroup[MAX_OUTPUTS] = {-1, -1, -1, -1, -1, -1, -1, -1};
```

### EEPROM Persistence

The chasing groups are stored in the EEPROM structure:

```cpp
struct EEPROMData {
    // ... other fields ...
    
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
```

**EEPROM Usage**: ~280 bytes for chasing groups (4 groups × ~70 bytes each)

## API Reference

### Create Chasing Group

**Endpoint**: `POST /api/chasing/create`

**Request Body (JSON)**:
```json
{
  "groupId": 0,
  "outputs": [0, 1, 2, 3],
  "interval": 300,
  "name": "Runway Lights"
}
```

**Parameters**:
- `groupId` (uint8_t, required): Group ID (0-3)
- `outputs` (array, required): Array of output indices (0-7), up to 8 outputs
- `interval` (uint16_t, required): Step interval in milliseconds (10-65535)
- `name` (string, optional): Group name (max 20 characters, default: "Group X")

**Response**:
```json
{
  "success": true,
  "groupId": 0
}
```

**Error Response**:
```json
{
  "success": false,
  "error": "Group limit reached (max 4)"
}
```

**Example Usage**:
```bash
curl -X POST http://railhub8266.local/api/chasing/create \
  -H "Content-Type: application/json" \
  -d '{
    "groupId": 0,
    "outputs": [0, 1, 2, 3, 4],
    "interval": 250,
    "name": "Runway Approach"
  }'
```

### Delete Chasing Group

**Endpoint**: `POST /api/chasing/delete`

**Request Body (JSON)**:
```json
{
  "groupId": 0
}
```

**Response**:
```json
{
  "success": true
}
```

**Example Usage**:
```bash
curl -X POST http://railhub8266.local/api/chasing/delete \
  -H "Content-Type: application/json" \
  -d '{"groupId": 0}'
```

### Rename Chasing Group

**Endpoint**: `POST /api/chasing/name`

**Request Body (JSON)**:
```json
{
  "groupId": 0,
  "name": "Updated Name"
}
```

**Response**:
```json
{
  "success": true
}
```

**Example Usage**:
```bash
curl -X POST http://railhub8266.local/api/chasing/name \
  -H "Content-Type: application/json" \
  -d '{
    "groupId": 0,
    "name": "Taxiway Lights"
  }'
```

### Get Status (includes chasing groups)

**Endpoint**: `GET /api/status`

**Response** (excerpt):
```json
{
  "chasingGroups": [
    {
      "id": 0,
      "active": true,
      "name": "Runway Lights",
      "outputs": [0, 1, 2, 3],
      "interval": 300,
      "currentStep": 2
    }
  ],
  "outputs": [
    {
      "pin": 4,
      "active": true,
      "brightness": 100,
      "chasingGroup": 0,
      "name": "LED 1"
    }
  ]
}
```

## Operational Behavior

### Sequential Activation

Chasing groups activate outputs in sequence:

```
Step 0: Output[0] ON,  Output[1] OFF, Output[2] OFF, Output[3] OFF
        ↓ wait interval ms
Step 1: Output[0] OFF, Output[1] ON,  Output[2] OFF, Output[3] OFF
        ↓ wait interval ms
Step 2: Output[0] OFF, Output[1] OFF, Output[2] ON,  Output[3] OFF
        ↓ wait interval ms
Step 3: Output[0] OFF, Output[1] OFF, Output[2] OFF, Output[3] ON
        ↓ wait interval ms
Step 0: (loop back to beginning)
```

### Non-blocking Timing

- Uses `millis()` for timing (no delays)
- Each group tracks its own `lastStepTime`
- Independent step advancement per group
- Does not block web server, WebSocket, or other operations

### Conflict Resolution

**Manual Control vs. Chasing**:
- When an output is assigned to a chasing group, it is controlled exclusively by that group
- Manual control via web UI or API is disabled for outputs in active groups
- `outputChasingGroup[i]` tracks which group owns each output (-1 = manual control)
- Deleting a group releases outputs back to manual control

**Multiple Groups**:
- Groups can share outputs (not recommended)
- Last group to update an output "wins" (race condition)
- Best practice: Assign each output to only one group

## Web Interface

### Chasing Tab (ESP8266 only)

The web interface includes a dedicated "Chasing" tab with:

**Group Management**:
- List of all active chasing groups
- Group status (active/inactive)
- Current step indicator
- Interval display
- Output assignment display

**Group Controls**:
- Create new group button
- Delete group button
- Edit group name inline
- Enable/disable group toggle
- Visual step progression indicator

**Visual Indicators**:
- Active output highlighted in green
- Group border color-coded by ID
- Step counter shows current position
- Interval shown in milliseconds

### Creating a Group via UI

1. Click "Create Chasing Group"
2. Select outputs to include (checkboxes for outputs 0-7)
3. Set interval (milliseconds)
4. Enter group name (optional)
5. Click "Create"
6. Group starts immediately and saves to EEPROM

### Deleting a Group via UI

1. Find group in list
2. Click "Delete" button
3. Confirm deletion
4. Outputs released to manual control
5. Change saved to EEPROM

## Use Cases

### 1. Runway Approach Lights
```json
{
  "groupId": 0,
  "outputs": [0, 1, 2, 3, 4, 5, 6, 7],
  "interval": 200,
  "name": "Runway Approach"
}
```
Creates a sequential pattern simulating airplane approach lights.

### 2. Advertising Sign
```json
{
  "groupId": 1,
  "outputs": [0, 1, 2],
  "interval": 500,
  "name": "Neon Sign"
}
```
Cycles through sign segments for attention-grabbing effect.

### 3. Traffic Signal
```json
{
  "groupId": 2,
  "outputs": [4, 5, 6],
  "interval": 2000,
  "name": "Traffic Light"
}
```
Slow sequential activation for traffic light simulation (Red → Yellow → Green).

### 4. Decorative Chase
```json
{
  "groupId": 3,
  "outputs": [0, 2, 4, 6],
  "interval": 150,
  "name": "Christmas Lights"
}
```
Fast alternating pattern for decorative effects.

## Performance Considerations

### CPU Usage
- **Per Group Overhead**: ~5-10µs per loop iteration
- **4 Active Groups**: ~20-40µs total overhead
- **Impact**: Negligible (<0.1% CPU utilization)

### Memory Usage
- **RAM per Group**: ~30 bytes
- **Total RAM (4 groups)**: ~120 bytes
- **EEPROM**: 280 bytes (persistent storage)

### Timing Accuracy
- **Resolution**: ±5ms (depends on loop iteration time)
- **Minimum Practical Interval**: 50ms (recommended 100ms+)
- **Maximum Interval**: 65535ms (~65.5 seconds)
- **Jitter**: <10ms under normal load

## Limitations

1. **Platform Exclusive**: Only available on ESP8266 (RailHub8266)
2. **Maximum Groups**: 4 simultaneous chasing groups
3. **Outputs per Group**: Up to 8 outputs
4. **Total Outputs**: 8 (ESP8266 hardware limitation)
5. **Shared Outputs**: Not recommended (race conditions)
6. **No Direction Control**: Only forward sequence (no reverse)
7. **No Pause/Resume**: Groups are either active or deleted
8. **No Speed Ramping**: Interval is fixed per group

## Implementation Details

### Main Loop Integration

```cpp
void loop() {
    // ... other loop tasks ...
    
    updateChasingLightGroups();  // Update all chasing groups
    
    // ... WebSocket, WiFi, etc ...
}
```

### Update Function

```cpp
void updateChasingLightGroups() {
    unsigned long now = millis();
    
    for (uint8_t i = 0; i < chasingGroupCount; i++) {
        ChasingGroup& group = chasingGroups[i];
        
        if (!group.active || group.outputCount == 0) continue;
        
        // Check if interval has elapsed
        if (now - group.lastStepTime >= group.interval) {
            // Turn off current output
            uint8_t currentOutput = group.outputIndices[group.currentStep];
            analogWrite(outputPins[currentOutput], 0);
            
            // Advance to next step (with wrap-around)
            group.currentStep = (group.currentStep + 1) % group.outputCount;
            
            // Turn on next output
            uint8_t nextOutput = group.outputIndices[group.currentStep];
            analogWrite(outputPins[nextOutput], outputBrightness[nextOutput]);
            
            // Update timestamp
            group.lastStepTime = now;
        }
    }
}
```

## Troubleshooting

### Group Not Running
**Problem**: Created group but outputs not chasing  
**Solution**: 
- Check group is active (should be true by default)
- Verify outputs are not in use by another group
- Check interval is reasonable (>50ms)
- Monitor serial output for errors

### Outputs Flickering
**Problem**: Chasing outputs flicker or behave erratically  
**Solution**:
- Increase interval (>100ms recommended)
- Check power supply stability
- Reduce number of active groups
- Verify no manual control conflicts

### EEPROM Not Saving
**Problem**: Groups lost after reboot  
**Solution**:
- Check EEPROM initialization in serial monitor
- Verify checksum validation
- Reset EEPROM via `/api/reset` and recreate groups
- Ensure `EEPROM.commit()` is called after changes

### WebSocket Not Updating
**Problem**: Chasing status not reflected in web UI  
**Solution**:
- Refresh browser page
- Check WebSocket connection status (should be connected)
- Monitor WebSocket port 81 accessibility
- Check browser console for errors

## Future Enhancements

Potential improvements for future versions:

- [ ] **Bidirectional Chasing**: Forward and reverse sequences
- [ ] **Pause/Resume**: Temporary halt without deleting group
- [ ] **Speed Ramping**: Gradually increase/decrease interval
- [ ] **Pattern Modes**: Bounce, random, custom sequences
- [ ] **Brightness per Step**: Individual brightness for each output in sequence
- [ ] **Sync Groups**: Synchronize multiple groups
- [ ] **Group Templates**: Pre-configured patterns
- [ ] **Port to ESP32**: Implement on ESP32 platform (16 outputs)

## Compatibility

**Supported Platforms**:
- ✅ ESP8266 (RailHub8266 v2.0+)
- ❌ ESP32 (RailHub32) - Not available in v2.0

**Browser Compatibility**:
- ✅ Chrome/Edge (recommended)
- ✅ Firefox
- ✅ Safari
- ⚠️ Internet Explorer (limited WebSocket support)

**API Compatibility**:
- Fully compatible with existing `/api/control` and `/api/status` endpoints
- Adds three new endpoints: `/api/chasing/create`, `/api/chasing/delete`, `/api/chasing/name`
- Does not break existing API consumers

## Version History

**v2.0.0** - Initial chasing lights implementation
- Support for 4 independent groups
- EEPROM persistence
- RESTful API
- Web UI integration
- WebSocket real-time updates
- Non-blocking sequential control

---

**Platform**: ESP8266 (RailHub8266) Exclusive  
**Version**: 2.0  
**Last Updated**: November 14, 2025  
**Feature Status**: Production-ready
