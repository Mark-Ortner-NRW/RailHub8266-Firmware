# RailHub8266 Unit Tests

## Overview

This project includes two types of tests:

1. **Hardware Tests** (`test_main.cpp`) - Basic tests that run on the ESP8266 hardware
2. **Logic Tests** (`test_output_logic.cpp`) - Comprehensive unit tests for business logic that run on native C++ (off-device)

## Test Files

### test_main.cpp
- **Purpose**: Basic smoke tests to verify the test framework works on hardware
- **Environment**: `esp12e_test` (requires ESP8266 device)
- **Tests**: Basic arithmetic tests

### test_output_logic.cpp
- **Purpose**: Comprehensive unit tests for core application logic
- **Environment**: `native` (runs on development machine)
- **Coverage**:
  - GPIO pin lookup and validation
  - Brightness to PWM value mapping (0-100% → 0-255)
  - Output index boundary checks
  - Chasing group parameter validation
  - Output indices validation (duplicates, out-of-bounds)
  - Group slot management
  - State consistency checks

## Running Tests

### Run ALL Tests (Hardware + Native)
```powershell
pio test
```

### Run Hardware Tests Only (on ESP8266)
```powershell
pio test --environment esp12e_test
```

**Note**: Requires ESP8266 board connected via USB on COM6 (configured in platformio.ini)

### Run Native Tests Only (off-device)
```powershell
pio test --environment native
```

**Runs on your PC** - no hardware required, fast execution

**Prerequisites**: Requires GCC/G++ compiler installed:
- **Windows**: Install MinGW-w64 or MSYS2
- **Linux**: `sudo apt install build-essential`
- **macOS**: Install Xcode Command Line Tools

## Test Output

Successful test run shows:
```
Environment    Test              Status    Duration
-------------  ----------------  --------  ----------
native         test_output_logic PASSED    00:00:01.234
esp12e_test    test_main         PASSED    00:00:45.678
```

## Test Coverage

### Edge Cases Tested
- ✅ Invalid GPIO pins (negative, out of range)
- ✅ Brightness bounds (< 0, > 100)
- ✅ PWM value bounds (< 0, > 255)
- ✅ Output index validation
- ✅ Chasing group validation (groupId 0, count 0, interval < 50ms)
- ✅ Duplicate output indices
- ✅ Array boundary conditions
- ✅ Maximum group size (8 outputs)

### Logic Tested
- ✅ Brightness percentage to PWM mapping precision
- ✅ Pin number to array index lookup
- ✅ Group slot allocation (existing vs new groups)
- ✅ Output assignment to chasing groups
- ✅ State consistency across operations

## Compilation Details

The native tests are compiled with:
- **Standard**: C++11
- **Flags**: `-DUNIT_TEST -DNATIVE_BUILD`
- **Framework**: Unity test framework
- **Dependencies**: ArduinoJson, WebSockets (mocked)

The `NATIVE_BUILD` flag ensures hardware-specific code is excluded during native compilation.

## Adding New Tests

To add a new test:

1. Create test function with signature `void test_yourTestName(void)`
2. Use Unity assertions:
   - `TEST_ASSERT_EQUAL(expected, actual)`
   - `TEST_ASSERT_TRUE(condition)`
   - `TEST_ASSERT_FALSE(condition)`
   - `TEST_ASSERT_EQUAL_INT(expected, actual)`
3. Add to test runner in `main()`:
   ```cpp
   RUN_TEST(test_yourTestName);
   ```

## Troubleshooting

### Test fails to compile on native
- Ensure `#ifdef NATIVE_BUILD` guards are in place
- Check that hardware-specific functions are mocked

### Test fails on hardware
- Verify COM port in `platformio.ini` matches your device
- Press reset button if upload hangs
- Check serial monitor for detailed output

### Tests pass individually but fail together
- Check `setUp()` and `tearDown()` functions properly reset state
- Look for shared state between tests

## Best Practices

1. **Test one thing per test** - Keep tests focused and simple
2. **Use descriptive names** - `test_functionName_scenario`
3. **Test boundaries** - Min, max, and edge values
4. **Reset state** - Use `setUp()` to ensure clean state
5. **Independent tests** - Each test should run standalone
