CURRENT_MAKEFILE_PATH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

INCLUDE_COMMON += -I$(realpath $(CURRENT_MAKEFILE_PATH)/../) -I$(realpath $(CURRENT_MAKEFILE_PATH)/../../)
$(info $(INCLUDE_COMMON))
include $(dir $(CURRENT_MAKEFILE_PATH))../../user/common.user.Makefile
# print the value of the variable ULIB
$(info $(ULIB))

clean:
	rm -f _* $(ULIB)
	rm -rf *.o *.d *.asm *.sym
