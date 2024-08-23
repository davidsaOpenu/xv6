################################################################################
# User binaries makefile.
# Contains _binary targets for each binary in the user directory (see common/common.user.Makefile)
# Contains clean target to remove all user binaries and intermediate files.
################################################################################

.PHONY: all clean

include ../user/common.user.Makefile

clean:
	rm -f $(ULIB) _*
	rm -rf *.o *.d *.asm *.sym
