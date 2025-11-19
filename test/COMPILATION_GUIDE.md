# Compilation Guide for Unit Tests

## Quick Start

### Option 1: Hardware Tests (Recommended - No Additional Setup)
```powershell
pio test --environment esp12e_test
```
✅ Works immediately - uses your ESP8266 board
✅ No additional compiler installation needed

### Option 2: Native Tests (Requires GCC/G++)

**Step 1: Install GCC/G++ Compiler**

#### Windows
Choose one of these options:

**A. MinGW-w64 (Recommended)**
1. Download from: https://winlibs.com/
2. Extract to `C:\mingw64`
3. Add to PATH: `C:\mingw64\bin`
4. Verify: `gcc --version`

**B. MSYS2**
1. Download from: https://www.msys2.org/
2. Install and run MSYS2
3. Run: `pacman -S mingw-w64-x86_64-gcc`
4. Add to PATH: `C:\msys64\mingw64\bin`

**C. Visual Studio Build Tools**
1. Install Visual Studio Build Tools
2. Includes Microsoft C++ compiler

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential
```

#### macOS
```bash
xcode-select --install
```

**Step 2: Run Native Tests**
```powershell
pio test --environment native
```

## What Gets Compiled

### Hardware Tests (`test_main.cpp`)
- Compiles for ESP8266 architecture
- Requires physical device
- Tests Arduino/ESP8266 integration
- Uploaded and executed on device

### Native Tests (`test_output_logic.cpp`)
- Compiles for your PC (x86/x64)
- Uses standard C++ compiler (GCC/Clang/MSVC)
- Tests pure logic without hardware
- Runs instantly on your machine
- Protected by `#ifdef NATIVE_BUILD` guard

## Compilation Flags

### Native Environment
```ini
[env:native]
platform = native
build_flags = 
    -std=c++11
    -DUNIT_TEST
    -DNATIVE_BUILD
```

### ESP8266 Test Environment
```ini
[env:esp12e_test]
platform = espressif8266
board = esp12e
framework = arduino
build_flags = 
    -DUNIT_TEST
    -DCORE_DEBUG_LEVEL=3
```

## Manual Compilation (Advanced)

If you want to compile tests manually:

### Native Tests (Linux/macOS)
```bash
g++ -std=c++11 -DNATIVE_BUILD -DUNIT_TEST \
    -I./test \
    -I./.pio/libdeps/native/Unity/src \
    test/test_output_logic.cpp \
    ./.pio/libdeps/native/Unity/src/unity.c \
    -o test_output_logic

./test_output_logic
```

### Native Tests (Windows with MinGW)
```powershell
g++ -std=c++11 -DNATIVE_BUILD -DUNIT_TEST `
    -I.\test `
    -I.\.pio\libdeps\native\Unity\src `
    test\test_output_logic.cpp `
    .\.pio\libdeps\native\Unity\src\unity.c `
    -o test_output_logic.exe

.\test_output_logic.exe
```

## Troubleshooting

### Error: "gcc is not recognized"
**Cause**: GCC not in system PATH

**Solution**:
1. Find where GCC is installed (e.g., `C:\mingw64\bin`)
2. Add to PATH:
   - Windows: Settings → System → About → Advanced System Settings → Environment Variables
   - Add `C:\mingw64\bin` to PATH variable
3. Restart terminal
4. Verify: `gcc --version`

### Error: "Multiple main functions"
**Cause**: Both test files have `main()` but not properly guarded

**Solution**: Ensure `#ifdef NATIVE_BUILD` guards in test_output_logic.cpp

### Error: Unity not found
**Cause**: Unity library not installed

**Solution**:
```powershell
pio lib install "throwtheswitch/Unity@^2.6.0"
```

### Tests compile but fail to upload
**Cause**: Wrong COM port or board not connected

**Solution**:
1. Check COM port in Device Manager
2. Update `upload_port` in platformio.ini
3. Press reset button on ESP8266

## Alternative: Skip Native Tests

If you don't want to set up GCC, you can:

1. **Only run hardware tests**:
   ```powershell
   pio test -e esp12e_test
   ```

2. **Remove native environment** from `platformio.ini`:
   ```ini
   # Comment out or remove:
   # [env:native]
   # platform = native
   # ...
   ```

3. **Guard native test file**:
   Native tests are already guarded with `#ifdef NATIVE_BUILD`, so they won't interfere with hardware builds.

## Best Practice: Use Both

1. **Develop logic** → Test with native (fast iterations)
2. **Integration** → Test with hardware (real-world validation)
3. **CI/CD** → Run both in automated pipeline

## CI/CD Example (GitHub Actions)

```yaml
name: Tests

on: [push, pull_request]

jobs:
  native-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
        with:
          python-version: '3.x'
      - name: Install PlatformIO
        run: pip install platformio
      - name: Run Native Tests
        run: pio test -e native
```

This allows running native tests on every commit without requiring ESP8266 hardware!
