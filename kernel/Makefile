################################################
# Xv6 kernel Makefile
################################################
CURRENT_MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
INCLUDE_COMMON += -I$(realpath $(CURRENT_MAKEFILE_DIR)) 

include ../common.mk

OBJS = \
	kvector.o\
	device/buf_cache.o\
	device/bio.o\
	console.o\
	device/device.o\
	exec.o\
	fs/vfs_file.o\
	fs/native_fs.o\
	fs/vfs_fs.o\
	device/ide.o\
	ioapic.o\
	kalloc.o\
	kbd.o\
	lapic.o\
	fs/native_log.o\
	main.o\
	kmount.o\
	mount_ns.o\
	pid_ns.o\
	mp.o\
	namespace.o\
	picirq.o\
	pipe.o\
	fs/procfs.o\
	proc.o\
	sleeplock.o\
	spinlock.o\
	string.o\
	swtch.o\
	syscall.o\
	sysfile.o\
	sysmount.o\
	sysnamespace.o\
	sysproc.o\
	trapasm.o\
	trap.o\
	uart.o\
	vectors.o\
	vm.o\
	udiv.o\
	steady_clock.o\
	klib.o\
	fs/cgfs.o\
	cgroup.o\
	cpu_account.o\
	device/obj_disk.o\
	device/obj_cache.o\
	fs/obj_fs.o\
	device/obj_device.o\
	device/ide_device.o\
	device/loop_device.o\
	fs/fs.o


ALL= bootblock entryother initcode kernel.bin vectors.S

all: $(ALL)


bootblock: bootasm.S bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c bootmain.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c bootasm.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
	
	$(OBJDUMP) -S bootblock.o > bootblock.asm
	$(OBJCOPY) -S -O binary -j .text bootblock.o bootblock
	./sign.pl bootblock

entryother: entryother.S
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c entryother.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o bootblockother.o entryother.o
	$(OBJCOPY) -S -O binary -j .text bootblockother.o entryother
	$(call dump_objs,$@)

initcode: initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -c initcode.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o initcode.out initcode.o
	$(OBJCOPY) -S -O binary -j .text initcode.out initcode
	$(call dump_objs,$@)

kernel.bin: entry.o entryother initcode kernel.ld $(OBJS)
	$(LD) $(LDFLAGS) -T kernel.ld -o kernel.bin entry.o $(OBJS) -b binary initcode entryother
	$(call dump_objs,$@)

tags: $(OBJS) entryother.S _init
	etags *.S *.c

vectors.S: vectors.pl
	perl vectors.pl > vectors.S

fs/%.o: fs/%.c
	$(CC) $(CFLAGS) -c $< -o $@

device/%.o: device/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(ALL) initcode.out
	rm -rf *.o *.d *.asm *.sym
