
include ../../common.mk

_pouch: pouch.o image.o container.o configs.o build.o util.o start.o $(ULIB)
	$(LD) $(USER_LD_FLAGS) $^ -o $@

%.o: %.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

clean:
	rm -f $(ULIB) _*
	rm -rf *.o *.d *.asm *.sym
