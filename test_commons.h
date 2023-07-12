#ifndef TEST_COMMONS_H
#define TEST_COMMONS_H

/* This file is a general test file.
 * To use the file, you need to define a PRINT macro that is compatible with the
 * interface of printf
 */

#define REMOVE_2_ADDITIONAL_CHARS 2
#define TEST(test_name) void test_name(const char* name)

void print_error(const char* name, unsigned long int x, unsigned long int y,
                 const char* file, int line) {
  for (int i = 0;
       i < strlen(name) + strlen("[RUNNING] ") + REMOVE_2_ADDITIONAL_CHARS; i++)
    PRINT("\b");
  PRINT("[FAILED] %s - expected %d but got %d (%s:%d)\n", name, x, y, file,
        line);
}

#define run_test(test_name)                                         \
  if (failed == 0) {                                                \
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

// Prints the exectuion message differently.
#define run_test_break_msg(test_name)    \
  if (failed == 0) {                     \
    PRINT("[RUNNING] %s\n", #test_name); \
    test_name(#test_name);               \
  }                                      \
  if (failed == 0) PRINT("[DONE] %s\n", #test_name);

/**
 * NUMERIC VALIDATIONS
 */

#define ASSERT_UINT_EQ(x, y)                     \
  if ((x) != (y)) {                              \
    print_error(name, x, y, __FILE__, __LINE__); \
    failed = 1;                                  \
    return;                                      \
  }

#define EXPECT_UINT_EQ(x, y)                     \
  if ((x) != (y)) {                              \
    print_error(name, x, y, __FILE__, __LINE__); \
    failed = 1;                                  \
  }

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

/**
 * OBJECT FILE SYSTEM SPECIFIC VALIDATIONS
 */
#define ASSERT_NO_ERR(x) ASSERT_UINT_EQ(NO_ERR, (x))

#endif  // TEST_COMMONS_H