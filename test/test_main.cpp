#include <Arduino.h>
#include <unity.h>

// Basic test to ensure the test framework works
void test_basic_addition(void) {
    TEST_ASSERT_EQUAL(4, 2 + 2);
}

void test_basic_subtraction(void) {
    TEST_ASSERT_EQUAL(0, 2 - 2);
}

void setUp(void) {
    // Set up code (runs before each test)
}

void tearDown(void) {
    // Tear down code (runs after each test)
}

void setup() {
    // Wait for serial connection
    delay(2000);
    
    UNITY_BEGIN();
    RUN_TEST(test_basic_addition);
    RUN_TEST(test_basic_subtraction);
    UNITY_END();
}

void loop() {
    // Tests run once in setup
}
