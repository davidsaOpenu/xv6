################################################################################
# User binaries makefile.
# Contains _binary targets for each binary in the user directory
# Contains clean target to remove all user binaries and intermediate files.
################################################################################

include ../common.mk

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

_%: %.o $(ULIB)
	$(LD) $(USER_LD_FLAGS) $^ -o $@

pouch/_%:
	$(MAKE) -C pouch _$*

clean:
	rm -f $(ULIB) _*
	rm -rf *.o *.d *.asm *.sym
	$(MAKE) -C pouch clean
