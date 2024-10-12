
include ../../common.mk

_pouch: pouch.o image.o container.o configs.o build.o util.o $(ULIB)
	$(LD) $(USER_LD_FLAGS) $^ -o $@
	$(call dump_objs,$@)

%.o: %.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<
	$(call dump_objs,$@)

clean:
	rm -f $(ULIB) _*
	rm -rf *.o *.d *.asm *.sym
