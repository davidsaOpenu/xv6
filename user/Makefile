################################################################################
# User binaries makefile.
# Contains _binary targets for each binary in the user directory
# Contains clean target to remove all user binaries and intermediate files.
################################################################################

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	$(call dump_objs,$@)

_%: %.o $(ULIB)
	$(LD) $(USER_LD_FLAGS) $^ -o $@
	$(call dump_objs,$@)

_pouch:
	make -C pouch
	cp pouch/_pouch .

clean:
	rm -f $(ULIB) _*
	rm -rf *.o *.d *.asm *.sym
	$(MAKE) -C pouch clean
