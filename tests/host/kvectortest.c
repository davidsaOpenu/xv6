#include <stdio.h>
#include <string.h>

#include "../framework/test.h"
#include "common_mocks.h"
#include "defs.h"
#include "kvector.h"

#define isInfoEqual(info1, info2)                                    \
  ((info1).age == (info2).age && (info1).height == (info2).height && \
   strcmp((info1).name, (info2).name) == 0)  // NOLINT

// API BEING TESTED
vector newvector(unsigned int size, unsigned int typesize);
void freevector(vector* v);
void memmove_into_vector_bytes(vector dstvec, unsigned int dstbyteoffset,
                               char* src, unsigned int size);
void memmove_into_vector_elements(vector dstvec, unsigned int dstelementoffset,
                                  char* src, unsigned int size);
void memmove_from_vector(char* dst, vector vec, unsigned int elementoffset,
                         unsigned int elementcount);
char* getelementpointer(vector v, unsigned int index);

TEST(test_initialize_vector) {
  vector v;

  for (int vectorsize = 1; vectorsize < 1000; vectorsize += 100) {
    for (int typesize = 1; typesize < 100; typesize += 20) {
      v = newvector(vectorsize, typesize);
      ASSERT_TRUE(v.valid);
      freevector(&v);
    }
  }
}

TEST(test_read_and_write_data) {
  // Some composite data
  struct info {
    int age;
    float height;
    char name[10];
  };

  struct info students[5] = {{.age = 17, .height = 1.7, .name = "Daniel"},
                             {.age = 18, .height = 1.8, .name = "Mike"},
                             {.age = 27, .height = 2.0, .name = "Jordan"},
                             {.age = 48, .height = 1.6, .name = "Elinor"},
                             {.age = 32, .height = 1.85, .name = "Michael"}};

  vector composite[] = {newvector(5, (sizeof(struct info))),
                        newvector(5, (sizeof(struct info)))};

  memmove_into_vector_bytes(composite[0], 0, (char*)&(students[0]),
                            5 * (sizeof(struct info)));
  memmove_into_vector_elements(composite[1], 0, (char*)&(students[0]), 5);

  int currentVector;
  for (currentVector = 0; currentVector < 2; currentVector++) {
    int currentElement;
    for (currentElement = 1; currentElement < 5; currentElement++) {
      struct info currentStudent;
      memmove_from_vector((char*)(&currentStudent), composite[currentVector],
                          currentElement, 1);
      ASSERT_TRUE(isInfoEqual(currentStudent, students[currentElement]));
    }
  }
}

TEST(test_move_bytes) {
  vector v = newvector(3, 1);
  char str[] = "abc";
  memmove_into_vector_bytes(v, 0, str, 3);
  char str2[sizeof(str)] = {0};
  memmove_from_vector(str2, v, 0, 3);

  ASSERT_TRUE(!strcmp(str, str2));
}

TEST(test_move_bytes_with_offset) {
  vector v = newvector(3, 1);
  char str[] = "abc";
  memmove_into_vector_bytes(v, 1, str, 2);  // "ab" --> vector[empty, a, b]
  char str2[sizeof(str)] = {0};             // str2 is empty string
  memmove_from_vector(str2, v, 1, 2);       // str2 = {'a','b',0}

  ASSERT_TRUE(!strcmp("ab", str2));
}

TEST(test_move_elements) {
  vector v = newvector(3, sizeof(int));
  int numbers[] = {10, 20, 40};
  memmove_into_vector_elements(v, 0, (char*)numbers, 3);

  for (int i = 0; i < 3; i++) {
    ASSERT_TRUE(*(int*)getelementpointer(v, i) == numbers[i]);
  }
}

TEST(test_move_elements_with_offset) {
  vector v = newvector(3, sizeof(int));
  int numbers[] = {10, 20, 40};
  memmove_into_vector_elements(v, 1, (char*)numbers, 2);

  for (int i = 1; i < 3; i++) {
    ASSERT_TRUE(*(int*)getelementpointer(v, i) == numbers[i - 1]);
  }
}

// Should be called before each test
void init_test() { init_mocks_environment(); }

INIT_TESTS_PLATFORM();

int main() {
  SET_TEST_INITIALIZER(&init_test);

  run_test(test_initialize_vector);
  run_test(test_read_and_write_data);
  run_test(test_move_bytes);
  run_test(test_move_bytes_with_offset);
  run_test(test_move_elements);
  run_test(test_move_elements_with_offset);

  PRINT_TESTS_RESULT("KVECTORTESTS");
  return CURRENT_TESTS_RESULT();
}
