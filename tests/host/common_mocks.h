#ifndef TESTS_HOST_COMMON_MOCKS
#define TESTS_HOST_COMMON_MOCKS

#define NUMBER_OF_PAGES 10000

void init_mocks_environment();

void start_expect_panic(void);

void stop_expect_panic(void);

int is_panic_handler_process();

/* Testing a panic requires a special handling.
 * In case of expecting a panic to occur, wrap your code
 * that should raise a panic by this macro.
 */
#define EXPECT_PANIC(code)            \
  do {                                \
    start_expect_panic();             \
    if (is_panic_handler_process()) { \
      code                            \
    }                                 \
    stop_expect_panic();              \
  } while (0)

#endif /* TESTS_HOST_COMMON_MOCKS */
