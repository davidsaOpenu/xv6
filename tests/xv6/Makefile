CURRENT_MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))

include $(dir $(CURRENT_MAKEFILE_PATH))../../user/common.user.Makefile
# print the value of the variable ULIB
$(info $(ULIB))

clean:
	rm -f _* $(ULIB)
	rm -rf *.o *.d *.asm *.sym
