CURRENT_MAKEFILE_PATH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CFLAGS += -I$(realpath $(CURRENT_MAKEFILE_PATH)/../) -I$(realpath $(CURRENT_MAKEFILE_PATH)/../../)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	$(call dump_objs,$@)

_%: %.o $(ULIB)
	$(LD) $(USER_LD_FLAGS) $^ -o $@
	$(call dump_objs,$@)

clean:
	rm -f _* *.o *.d *.asm *.sym
