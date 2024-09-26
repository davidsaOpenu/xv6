MAKEFILE_DIRECTORY := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

include ../../common.mk

HOST_TESTS_EXECUTABLES = kvector_tests obj_fs_tests
KERNEL_BASE_DIR=$(realpath $(MAKEFILE_DIRECTORY)/../../kernel)

all: $(HOST_TESTS_EXECUTABLES)

gdb: OFLAGS = -Og -ggdb

gdb: $(HOST_TESTS_EXECUTABLES)

HOST_CFLAGS += -I$(KERNEL_BASE_DIR)

# Object file system related files
# TODO integrate with the rest of xv6 sources - would be done in later part.
obj_fs_tests: obj_fs_tests.o device_obj_disk_ktbin.o device_obj_cache_ktbin.o device_buf_cache_ktbin.o kvector_ktbin.o common_mocks.o
	$(CC) $(HOST_CFLAGS) $(OFLAGS) $^ -std=gnu99 -o $@

kvector_tests:  kvector_ktbin.o kvector_tests.o common_mocks.o
	$(CC) $(HOST_CFLAGS) $(OFLAGS) $^ -std=gnu99 -o $@

common_mocks.o: common_mocks.c common_mocks.h
	$(CC) $(HOST_CFLAGS) $(OFLAGS) -c -o $@ $<


device_%_ktbin.o: $(KERNEL_BASE_DIR)/device/%.c
	$(CC) $(HOST_CFLAGS) $(OFLAGS) -c -o $@ $<

%_ktbin.o: $(KERNEL_BASE_DIR)/%.c
	$(CC) $(HOST_CFLAGS) $(OFLAGS) -c -o $@ $<

%_tests.o: %_tests.c common_mocks.h ../framework/test.h
	$(CC) $(HOST_CFLAGS) $(OFLAGS) -c -o $@ $<

clean:
	rm -f $(HOST_TESTS_EXECUTABLES)
	rm -rf *.o *.d *.asm *.sym
