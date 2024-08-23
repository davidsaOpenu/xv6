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

clean:
	rm -f $(ULIB) _*
	rm -rf *.o *.d *.asm *.sym
