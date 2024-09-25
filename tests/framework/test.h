#ifndef XV6_TEST_H
#define XV6_TEST_H

#ifdef HOST_TESTS
#include <stdio.h>
#include <string.h>  // NOLINT(build/include_what_you_use)
#else
#include "user/lib/user.h"
#endif

extern int failed;
extern void (*_test_init_func)(void);

#define REMOVE_2_ADDITIONAL_CHARS 2

enum exit_status {
  EXIT_STATUS_SUCCESS = 0,
  EXIT_STATUS_FAILURE = 1,
  EXIT_STATUS_EXPECTED_PANIC = 2
};

// NOTE: the "failed" variable cannot be 'static' as it might be used
//       by several source files.
#define INIT_TESTS_PLATFORM() \
  int failed = 0;             \
  void (*_test_init_func)(void) = (void (*)(void))0

#define SET_TEST_INITIALIZER(init_func)            \
  do {                                             \
    _test_init_func = (void (*)(void))(init_func); \
  } while (0)

#define TEST(test_name) void test_name(const char* name)

#define CURRENT_TESTS_RESULT() ((const int)failed)
#define PRINT_TESTS_RESULT(suite_name)              \
  do {                                              \
    if (failed) {                                   \
      PRINT("[    %s FAILED    ]\n", (suite_name)); \
      exit(EXIT_STATUS_FAILURE);                    \
    } else {                                        \
      PRINT("[    %s PASSED    ]\n", (suite_name)); \
      exit(EXIT_STATUS_SUCCESS);                    \
    }                                               \
  } while (0)

#ifdef HOST_TESTS
#define PRINT(...) printf(__VA_ARGS__)
#else
#define PRINT(...) printf(stdout, __VA_ARGS__)
#endif

#define print_error(name, x, y, file, line)                                   \
  do {                                                                        \
    for (int i = 0;                                                           \
         i < strlen(name) + strlen("[RUNNING] ") + REMOVE_2_ADDITIONAL_CHARS; \
         i++)                                                                 \
      PRINT("\b");                                                            \
    PRINT("[FAILED] %s - expected %lu but got %lu (%s:%d)\n", (name),         \
          (ulong)(x), (ulong)(y), (file), (line));                            \
  } while (0);

#define run_test(test_name)                                         \
  if (failed == 0) {                                                \
    if (_test_init_func != (void (*)(void))0) _test_init_func();    \
    PRINT("[RUNNING] %s", #test_name);                              \
    test_name(#test_name);                                          \
  }                                                                 \
  if (failed == 0) {                                                \
    for (int i = 0; i < strlen(#test_name) + strlen("[RUNNING] ") + \
                            REMOVE_2_ADDITIONAL_CHARS;              \
         i++)                                                       \
      PRINT("\b");                                                  \
    PRINT("[DONE] %s   \n", #test_name);                            \
  }

// Prints the execution message differently.
#define run_test_break_msg(test_name)    \
  if (failed == 0) {                     \
    PRINT("[RUNNING] %s\n", #test_name); \
    test_name(#test_name);               \
  }                                      \
  if (failed == 0) PRINT("[DONE] %s\n", #test_name);

/**
 * NUMERIC VALIDATIONS
 */

#define ASSERT_UINT_EQ(x, y)                               \
  do {                                                     \
    typeof(x) _xval = (x);                                 \
    typeof(y) _yval = (y);                                 \
    if (_xval != _yval) {                                  \
      print_error(name, _xval, _yval, __FILE__, __LINE__); \
      failed = 1;                                          \
      return;                                              \
    }                                                      \
  } while (0)

#define EXPECT_UINT_EQ(x, y)                               \
  do {                                                     \
    typeof(x) _xval = (x);                                 \
    typeof(y) _yval = (y);                                 \
    if (_xval != _yval) {                                  \
      print_error(name, _xval, _yval, __FILE__, __LINE__); \
      failed = 1;                                          \
    }                                                      \
  } while (0)

/**
 * BOOLEAN VALIDATIONS
 */

#define ASSERT_TRUE(x)                                                        \
  if (!(x)) {                                                                 \
    for (int i = 0;                                                           \
         i < strlen(#x) + strlen("[RUNNING] ") + REMOVE_2_ADDITIONAL_CHARS;   \
         i++)                                                                 \
      PRINT("\b");                                                            \
    PRINT("[FAILED] %s - expected true for " #x " (%s:%d)\n", name, __FILE__, \
          __LINE__);                                                          \
    failed = 1;                                                               \
    return;                                                                   \
  }

#define EXPECT_TRUE(x)                                                        \
  if (!(x)) {                                                                 \
    for (int i = 0;                                                           \
         i < strlen(#x) + strlen("[RUNNING] ") + REMOVE_2_ADDITIONAL_CHARS;   \
         i++)                                                                 \
      PRINT("\b");                                                            \
    PRINT("[FAILED] %s - expected true for " #x " (%s:%d)\n", name, __FILE__, \
          __LINE__);                                                          \
    failed = 1;                                                               \
  }

#define ASSERT_FALSE(x)                                                       \
  if (x) {                                                                    \
    for (int i = 0;                                                           \
         i < strlen(#x) + strlen("[RUNNING] ") + REMOVE_2_ADDITIONAL_CHARS;   \
         i++)                                                                 \
      PRINT("\b");                                                            \
    PRINT("[FAILED] %s - expected false for " #x "(%s:%d)\n", name, __FILE__, \
          __LINE__);                                                          \
    failed = 1;                                                               \
    return;                                                                   \
  }

#define ASSERT_EQ(x, y) ASSERT_TRUE(x == y)
#define ASSERT_NE(x, y) ASSERT_FALSE(x == y)
#define ASSERT_GE(x, y) ASSERT_TRUE(x >= y)
#define ASSERT_GT(x, y) ASSERT_TRUE(x > y)

#define EXPECT_FALSE(x)                                                       \
  if (x) {                                                                    \
    for (int i = 0;                                                           \
         i < strlen(#x) + strlen("[RUNNING] ") + REMOVE_2_ADDITIONAL_CHARS;   \
         i++)                                                                 \
      PRINT("\b");                                                            \
    PRINT("[FAILED] %s - expected false for " #x "(%s:%d)\n", name, __FILE__, \
          __LINE__);                                                          \
    failed = 1;                                                               \
  }

// In some special cases we want to fail the test not upon wrong value,
// but upon other scenarios, such as called function.
#define FAIL_TEST(msg)                                                      \
  do {                                                                      \
    PRINT("\n[FAILED] - failed test because %s (%s:%d)\n", (msg), __FILE__, \
          __LINE__);                                                        \
    failed = 1;                                                             \
    exit(EXIT_STATUS_FAILURE);                                              \
  } while (0)

/**
 * OBJECT FILE SYSTEM SPECIFIC VALIDATIONS
 */
#define ASSERT_NO_ERR(x) ASSERT_UINT_EQ(NO_ERR, (x))

#endif /* XV6_TEST_H */
