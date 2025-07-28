CURRENT_MAKEFILE_PATH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

include ../../common.mk

CFLAGS += -I$(realpath $(CURRENT_MAKEFILE_PATH)/../) -I$(realpath $(CURRENT_MAKEFILE_PATH)/../../)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

_%: %.o $(ULIB)
	$(LD) $(USER_LD_FLAGS) $^ -o $@

clean:
	rm -f _* *.o *.d *.asm *.sym
