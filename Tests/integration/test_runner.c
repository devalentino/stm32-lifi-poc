#include "test_suites.h"
#include "unity.h"

typedef void (*TestSuiteRunner)(void);

static const TestSuiteRunner TEST_SUITES[] = {
    Test_LiFi_Protocol_Run,
    // Test_LiFi_Modem_Run,
};

int main(void) {
  UNITY_BEGIN();

  for (unsigned int suite = 0; suite < sizeof(TEST_SUITES) / sizeof(TEST_SUITES[0]); ++suite) {
    TEST_SUITES[suite]();
  }

  return UNITY_END();
}
