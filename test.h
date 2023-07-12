#ifndef XV6_TEST_H
#define XV6_TEST_H
/**
 Defines a simple test library.
 To define a new test use `TEST`. For example:
    ```
    TEST(my_test) {
        ASSERT_UINT_EQ(1, 1);
    }
    ```
 To run the tests, use the macro `run_test` in the main and specify the test
 name. For eaxmple:
     ```
     int main() {
        run_test(my_test);
        return 0;
     }
     ```
 */

#include "user.h"
#define PRINT(...) printf(1, __VA_ARGS__)
#include "test_commons.h"

#endif /* XV6_TEST_H */
