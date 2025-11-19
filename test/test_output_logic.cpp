#ifdef NATIVE_BUILD
#include <unity.h>
#include <cstring>
#include <cstdint>

// Mock constants and types
#define MAX_OUTPUTS 7
#define MAX_CHASING_GROUPS 4
#define MAX_OUTPUTS_PER_CHASING_GROUP 8
#define MAX_NAME_LENGTH 20
#define MIN_CHASING_INTERVAL_MS 50

// Mock global state
static int outputPins[MAX_OUTPUTS] = {4, 5, 12, 13, 14, 16, 2};
static bool outputStates[MAX_OUTPUTS] = {false};
static int outputBrightness[MAX_OUTPUTS] = {255};
static unsigned int outputIntervals[MAX_OUTPUTS] = {0};
static int8_t outputChasingGroup[MAX_OUTPUTS] = {-1, -1, -1, -1, -1, -1, -1};

struct ChasingGroup {
    uint8_t groupId;
    bool active;
    char name[21];
    uint8_t outputIndices[8];
    uint8_t outputCount;
    uint16_t interval;
    uint8_t currentStep;
    unsigned long lastStepTime;
};

static ChasingGroup chasingGroups[MAX_CHASING_GROUPS];

// Helper function to find output index by GPIO pin
int findOutputIndexByPin(int pin) {
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        if (outputPins[i] == pin) {
            return i;
        }
    }
    return -1;
}

// Helper function to map brightness percentage to PWM value
int mapBrightnessToPWM(int brightnessPercent) {
    if (brightnessPercent < 0) return 0;
    if (brightnessPercent > 100) return 255;
    return (brightnessPercent * 255) / 100;
}

// Helper function to map PWM value to brightness percentage
int mapPWMToBrightness(int pwmValue) {
    if (pwmValue < 0) return 0;
    if (pwmValue > 255) return 100;
    return (pwmValue * 100) / 255;
}

// Validate output index
bool isValidOutputIndex(int index) {
    return index >= 0 && index < MAX_OUTPUTS;
}

// Validate chasing group parameters
bool validateChasingGroupParams(uint8_t groupId, uint8_t count, unsigned int intervalMs) {
    if (groupId == 0 || groupId > 255) return false;
    if (count == 0 || count > MAX_OUTPUTS_PER_CHASING_GROUP) return false;
    if (intervalMs < MIN_CHASING_INTERVAL_MS) return false;
    return true;
}

// Validate output indices for chasing group
bool validateOutputIndices(const uint8_t* outputIndices, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (outputIndices[i] >= MAX_OUTPUTS) {
            return false;
        }
        // Check for duplicates
        for (uint8_t j = i + 1; j < count; j++) {
            if (outputIndices[i] == outputIndices[j]) {
                return false;
            }
        }
    }
    return true;
}

// Find available group slot
int findGroupSlot(uint8_t groupId) {
    // First, try to find existing group with same ID
    for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
        if (chasingGroups[i].active && chasingGroups[i].groupId == groupId) {
            return i;
        }
    }
    
    // If not found, find first inactive slot
    for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
        if (!chasingGroups[i].active) {
            return i;
        }
    }
    
    return -1; // No slot available
}

// Check if output is part of any chasing group
bool isOutputInChasingGroup(int outputIndex) {
    return outputChasingGroup[outputIndex] >= 0;
}

// =============================================================================
// UNIT TESTS
// =============================================================================

void setUp(void) {
    // Reset state before each test
    memset(outputStates, 0, sizeof(outputStates));
    for (int i = 0; i < MAX_OUTPUTS; i++) {
        outputBrightness[i] = 255;
        outputIntervals[i] = 0;
        outputChasingGroup[i] = -1;
    }
    memset(chasingGroups, 0, sizeof(chasingGroups));
}

void tearDown(void) {
    // Clean up after each test
}

// =============================================================================
// Test: findOutputIndexByPin
// =============================================================================

void test_findOutputIndexByPin_validPins(void) {
    TEST_ASSERT_EQUAL(0, findOutputIndexByPin(4));
    TEST_ASSERT_EQUAL(1, findOutputIndexByPin(5));
    TEST_ASSERT_EQUAL(2, findOutputIndexByPin(12));
    TEST_ASSERT_EQUAL(3, findOutputIndexByPin(13));
    TEST_ASSERT_EQUAL(4, findOutputIndexByPin(14));
    TEST_ASSERT_EQUAL(5, findOutputIndexByPin(16));
    TEST_ASSERT_EQUAL(6, findOutputIndexByPin(2));
}

void test_findOutputIndexByPin_invalidPin(void) {
    TEST_ASSERT_EQUAL(-1, findOutputIndexByPin(99));
    TEST_ASSERT_EQUAL(-1, findOutputIndexByPin(0));
    TEST_ASSERT_EQUAL(-1, findOutputIndexByPin(-1));
    TEST_ASSERT_EQUAL(-1, findOutputIndexByPin(255));
}

// =============================================================================
// Test: Brightness Mapping
// =============================================================================

void test_mapBrightnessToPWM_normalRange(void) {
    TEST_ASSERT_EQUAL(0, mapBrightnessToPWM(0));
    TEST_ASSERT_EQUAL(25, mapBrightnessToPWM(10));
    TEST_ASSERT_EQUAL(127, mapBrightnessToPWM(50));
    TEST_ASSERT_EQUAL(255, mapBrightnessToPWM(100));
}

void test_mapBrightnessToPWM_edgeCases(void) {
    // Below minimum
    TEST_ASSERT_EQUAL(0, mapBrightnessToPWM(-1));
    TEST_ASSERT_EQUAL(0, mapBrightnessToPWM(-100));
    
    // Above maximum
    TEST_ASSERT_EQUAL(255, mapBrightnessToPWM(101));
    TEST_ASSERT_EQUAL(255, mapBrightnessToPWM(200));
}

void test_mapBrightnessToPWM_precision(void) {
    // Test specific percentages for correct rounding
    TEST_ASSERT_EQUAL(12, mapBrightnessToPWM(5));   // 5% = 12.75 -> 12
    TEST_ASSERT_EQUAL(63, mapBrightnessToPWM(25));  // 25% = 63.75 -> 63
    TEST_ASSERT_EQUAL(191, mapBrightnessToPWM(75)); // 75% = 191.25 -> 191
}

void test_mapPWMToBrightness_normalRange(void) {
    TEST_ASSERT_EQUAL(0, mapPWMToBrightness(0));
    TEST_ASSERT_EQUAL(50, mapPWMToBrightness(127));
    TEST_ASSERT_EQUAL(100, mapPWMToBrightness(255));
}

void test_mapPWMToBrightness_edgeCases(void) {
    // Below minimum
    TEST_ASSERT_EQUAL(0, mapPWMToBrightness(-1));
    TEST_ASSERT_EQUAL(0, mapPWMToBrightness(-255));
    
    // Above maximum
    TEST_ASSERT_EQUAL(100, mapPWMToBrightness(256));
    TEST_ASSERT_EQUAL(100, mapPWMToBrightness(1000));
}

// =============================================================================
// Test: Output Index Validation
// =============================================================================

void test_isValidOutputIndex_validIndices(void) {
    TEST_ASSERT_TRUE(isValidOutputIndex(0));
    TEST_ASSERT_TRUE(isValidOutputIndex(3));
    TEST_ASSERT_TRUE(isValidOutputIndex(6));
}

void test_isValidOutputIndex_invalidIndices(void) {
    TEST_ASSERT_FALSE(isValidOutputIndex(-1));
    TEST_ASSERT_FALSE(isValidOutputIndex(7));
    TEST_ASSERT_FALSE(isValidOutputIndex(100));
    TEST_ASSERT_FALSE(isValidOutputIndex(-100));
}

// =============================================================================
// Test: Chasing Group Validation
// =============================================================================

void test_validateChasingGroupParams_validParams(void) {
    TEST_ASSERT_TRUE(validateChasingGroupParams(1, 2, 50));
    TEST_ASSERT_TRUE(validateChasingGroupParams(1, 2, 100));
    TEST_ASSERT_TRUE(validateChasingGroupParams(255, 8, 1000));
}

void test_validateChasingGroupParams_invalidGroupId(void) {
    TEST_ASSERT_FALSE(validateChasingGroupParams(0, 2, 100));  // groupId = 0
    TEST_ASSERT_FALSE(validateChasingGroupParams(256, 2, 100)); // groupId > 255 (wraps to 0)
}

void test_validateChasingGroupParams_invalidOutputCount(void) {
    TEST_ASSERT_FALSE(validateChasingGroupParams(1, 0, 100));  // count = 0
    TEST_ASSERT_FALSE(validateChasingGroupParams(1, 9, 100));  // count > 8
}

void test_validateChasingGroupParams_invalidInterval(void) {
    TEST_ASSERT_FALSE(validateChasingGroupParams(1, 2, 0));    // interval = 0
    TEST_ASSERT_FALSE(validateChasingGroupParams(1, 2, 49));   // interval < 50ms
}

// =============================================================================
// Test: Output Indices Validation
// =============================================================================

void test_validateOutputIndices_validIndices(void) {
    uint8_t indices1[] = {0, 1, 2};
    TEST_ASSERT_TRUE(validateOutputIndices(indices1, 3));
    
    uint8_t indices2[] = {0, 6};
    TEST_ASSERT_TRUE(validateOutputIndices(indices2, 2));
    
    uint8_t indices3[] = {0, 1, 2, 3, 4, 5, 6};
    TEST_ASSERT_TRUE(validateOutputIndices(indices3, 7));
}

void test_validateOutputIndices_outOfBounds(void) {
    uint8_t indices1[] = {0, 1, 7};  // 7 >= MAX_OUTPUTS
    TEST_ASSERT_FALSE(validateOutputIndices(indices1, 3));
    
    uint8_t indices2[] = {0, 255};   // 255 >= MAX_OUTPUTS
    TEST_ASSERT_FALSE(validateOutputIndices(indices2, 2));
}

void test_validateOutputIndices_duplicates(void) {
    uint8_t indices1[] = {0, 1, 1};  // Duplicate 1
    TEST_ASSERT_FALSE(validateOutputIndices(indices1, 3));
    
    uint8_t indices2[] = {0, 0};     // Duplicate 0
    TEST_ASSERT_FALSE(validateOutputIndices(indices2, 2));
    
    uint8_t indices3[] = {2, 3, 2};  // Duplicate 2
    TEST_ASSERT_FALSE(validateOutputIndices(indices3, 3));
}

// =============================================================================
// Test: Group Slot Management
// =============================================================================

void test_findGroupSlot_emptySlots(void) {
    // All slots empty, should return slot 0
    TEST_ASSERT_EQUAL(0, findGroupSlot(1));
}

void test_findGroupSlot_existingGroup(void) {
    // Create a group in slot 0
    chasingGroups[0].active = true;
    chasingGroups[0].groupId = 5;
    
    // Should return slot 0 for same groupId
    TEST_ASSERT_EQUAL(0, findGroupSlot(5));
}

void test_findGroupSlot_nextAvailableSlot(void) {
    // Fill first two slots
    chasingGroups[0].active = true;
    chasingGroups[0].groupId = 1;
    chasingGroups[1].active = true;
    chasingGroups[1].groupId = 2;
    
    // New group should get slot 2
    TEST_ASSERT_EQUAL(2, findGroupSlot(3));
}

void test_findGroupSlot_noAvailableSlots(void) {
    // Fill all slots
    for (int i = 0; i < MAX_CHASING_GROUPS; i++) {
        chasingGroups[i].active = true;
        chasingGroups[i].groupId = i + 1;
    }
    
    // No slot available
    TEST_ASSERT_EQUAL(-1, findGroupSlot(10));
}

// =============================================================================
// Test: Output Chasing Group Membership
// =============================================================================

void test_isOutputInChasingGroup_notInGroup(void) {
    TEST_ASSERT_FALSE(isOutputInChasingGroup(0));
    TEST_ASSERT_FALSE(isOutputInChasingGroup(3));
}

void test_isOutputInChasingGroup_inGroup(void) {
    outputChasingGroup[0] = 1;
    outputChasingGroup[3] = 2;
    
    TEST_ASSERT_TRUE(isOutputInChasingGroup(0));
    TEST_ASSERT_TRUE(isOutputInChasingGroup(3));
    TEST_ASSERT_FALSE(isOutputInChasingGroup(1));
}

// =============================================================================
// Test: Boundary Conditions
// =============================================================================

void test_boundaryConditions_maxOutputs(void) {
    // Test accessing last valid output
    TEST_ASSERT_TRUE(isValidOutputIndex(MAX_OUTPUTS - 1));
    TEST_ASSERT_FALSE(isValidOutputIndex(MAX_OUTPUTS));
}

void test_boundaryConditions_chasingGroupSize(void) {
    // Test maximum chasing group size
    uint8_t maxIndices[MAX_OUTPUTS_PER_CHASING_GROUP];
    for (uint8_t i = 0; i < MAX_OUTPUTS_PER_CHASING_GROUP; i++) {
        maxIndices[i] = i % MAX_OUTPUTS; // Ensure valid indices
    }
    
    // Should fail if we have duplicates when wrapping
    TEST_ASSERT_FALSE(validateOutputIndices(maxIndices, MAX_OUTPUTS_PER_CHASING_GROUP));
    
    // Create valid max-sized group
    uint8_t validMaxIndices[7] = {0, 1, 2, 3, 4, 5, 6};
    TEST_ASSERT_TRUE(validateOutputIndices(validMaxIndices, 7));
}

void test_boundaryConditions_intervalLimits(void) {
    // Test minimum valid interval
    TEST_ASSERT_TRUE(validateChasingGroupParams(1, 2, MIN_CHASING_INTERVAL_MS));
    
    // Test just below minimum
    TEST_ASSERT_FALSE(validateChasingGroupParams(1, 2, MIN_CHASING_INTERVAL_MS - 1));
    
    // Test very large interval (should be valid)
    TEST_ASSERT_TRUE(validateChasingGroupParams(1, 2, 65535));
}

// =============================================================================
// Test: State Consistency
// =============================================================================

void test_stateConsistency_outputAssignment(void) {
    // Assign output 0 to group 1
    outputChasingGroup[0] = 1;
    
    // Create corresponding group
    chasingGroups[0].active = true;
    chasingGroups[0].groupId = 1;
    chasingGroups[0].outputIndices[0] = 0;
    chasingGroups[0].outputCount = 1;
    
    // Verify consistency
    TEST_ASSERT_EQUAL(1, outputChasingGroup[0]);
    TEST_ASSERT_TRUE(chasingGroups[0].active);
    TEST_ASSERT_EQUAL(0, chasingGroups[0].outputIndices[0]);
}

void test_stateConsistency_multipleOutputsInGroup(void) {
    // Create group with multiple outputs
    uint8_t indices[] = {0, 2, 4};
    
    for (int i = 0; i < 3; i++) {
        outputChasingGroup[indices[i]] = 5;
    }
    
    // Verify all assigned correctly
    TEST_ASSERT_EQUAL(5, outputChasingGroup[0]);
    TEST_ASSERT_EQUAL(-1, outputChasingGroup[1]); // Not in group
    TEST_ASSERT_EQUAL(5, outputChasingGroup[2]);
    TEST_ASSERT_EQUAL(-1, outputChasingGroup[3]); // Not in group
    TEST_ASSERT_EQUAL(5, outputChasingGroup[4]);
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Pin lookup tests
    RUN_TEST(test_findOutputIndexByPin_validPins);
    RUN_TEST(test_findOutputIndexByPin_invalidPin);
    
    // Brightness mapping tests
    RUN_TEST(test_mapBrightnessToPWM_normalRange);
    RUN_TEST(test_mapBrightnessToPWM_edgeCases);
    RUN_TEST(test_mapBrightnessToPWM_precision);
    RUN_TEST(test_mapPWMToBrightness_normalRange);
    RUN_TEST(test_mapPWMToBrightness_edgeCases);
    
    // Output validation tests
    RUN_TEST(test_isValidOutputIndex_validIndices);
    RUN_TEST(test_isValidOutputIndex_invalidIndices);
    
    // Chasing group validation tests
    RUN_TEST(test_validateChasingGroupParams_validParams);
    RUN_TEST(test_validateChasingGroupParams_invalidGroupId);
    RUN_TEST(test_validateChasingGroupParams_invalidOutputCount);
    RUN_TEST(test_validateChasingGroupParams_invalidInterval);
    
    // Output indices validation tests
    RUN_TEST(test_validateOutputIndices_validIndices);
    RUN_TEST(test_validateOutputIndices_outOfBounds);
    RUN_TEST(test_validateOutputIndices_duplicates);
    
    // Group slot management tests
    RUN_TEST(test_findGroupSlot_emptySlots);
    RUN_TEST(test_findGroupSlot_existingGroup);
    RUN_TEST(test_findGroupSlot_nextAvailableSlot);
    RUN_TEST(test_findGroupSlot_noAvailableSlots);
    
    // Output group membership tests
    RUN_TEST(test_isOutputInChasingGroup_notInGroup);
    RUN_TEST(test_isOutputInChasingGroup_inGroup);
    
    // Boundary condition tests
    RUN_TEST(test_boundaryConditions_maxOutputs);
    RUN_TEST(test_boundaryConditions_chasingGroupSize);
    RUN_TEST(test_boundaryConditions_intervalLimits);
    
    // State consistency tests
    RUN_TEST(test_stateConsistency_outputAssignment);
    RUN_TEST(test_stateConsistency_multipleOutputsInGroup);
    
    return UNITY_END();
}

#endif // NATIVE_BUILD
