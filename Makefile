# Modern makefile for openu-xv6.
# Recursive make is harmful.
#
# Maintainers:
#	Ron Shabi <ron@ronsh.net>

# Build Directory
B := build

# Toolchain prefix when cross compilation is wanted.
PREFIX :=


CC 		:= 	$(PREFIX)gcc
AS 		:= 	$(PREFIX)as
LD 		:= 	$(PREFIX)ld 2>/dev/null
AR 		:= 	$(PREFIX)ar
OBJCOPY := 	$(PREFIX)objcopy
OBJDUMP := 	$(PREFIX)objdump
HOSTCC 	:= 	gcc
PERL 	:= 	perl
PYTHON3	:= 	python3
PODMAN	:=  podman

INCLUDE_DIRS 			:= -Iinclude -Ikernel
INCLUDE_DIRS_USERLAND	:= $(INCLUDE_DIRS) -Iuser/lib

CFLAGS 	:= 	-static -MD -m32 -mno-sse -std=gnu99 -Wall -Werror -g \
			-Wstack-usage=4096 -fno-pic -fno-builtin \
			-fno-strict-aliasing -fno-omit-frame-pointer \
		  	-DHOST_CPU_TSC_FREQ=800000 \
			-DXV6_TSC_FREQUENCY=800000 \
		  	-DXV6_WAIT_FOR_DEBUGGER=0 \
		  	-DSTORAGE_DEVICE_SIZE=327680 \
		  	-fno-stack-protector \
			-nostdinc \
			-Wno-error=infinite-recursion \
			-Wno-error=stack-usage=

ASFLAGS 	:= --32 -gdwarf-2 $(INCLUDE_DIRS)

LDFLAGS 	:= -m elf_i386

IMG_FS	:= 	fs.img
IMG_XV6	:= 	xv6.img
CONTAINER_IMAGES := internal_fs_a internal_fs_b internal_fs_c


KERNEL  :=  kernel.bin

QEMU      	:= 	qemu-system-i386
QEMU_CPUS 	:= 	cpus=2,cores=1
QEMU_RAM  	:= 	512m
QEMUFLAGS 	:= 	-drive file="$(B)/$(IMG_FS)",index=1,media=disk,format=raw \
             	-drive file="$(B)/$(IMG_XV6)",index=0,media=disk,format=raw \
			 	-smp $(QEMU_CPUS) \
			 	-m $(QEMU_RAM) \
			 	-nographic \
				-snapshot

QEMU_GDB_FLAGS := -S -s

KERNEL_OBJS := \
	cgroup.o\
	console.o\
	cpu_account.o\
	device/bio.o\
	device/buf_cache.o\
	device/device.o\
	device/ide_device.o\
	device/ide.o\
	device/loop_device.o\
	device/obj_cache.o\
	device/obj_device.o\
	device/obj_disk.o\
	entry.o \
	exec.o\
	fs/cgfs.o\
	fs/fs.o \
	fs/native_fs.o\
	fs/native_log.o\
	fs/obj_fs.o\
	fs/procfs.o\
	fs/vfs_file.o\
	fs/vfs_fs.o\
	ioapic.o\
	kalloc.o\
	kbd.o\
	klib.o\
	kmount.o\
	kvector.o\
	lapic.o\
	main.o\
	mount_ns.o\
	mp.o\
	namespace.o\
	picirq.o\
	pid_ns.o\
	pipe.o\
	proc.o\
	sleeplock.o\
	spinlock.o\
	steady_clock.o\
	string.o\
	swtch.o\
	syscall.o\
	sysfile.o\
	sysmount.o\
	sysnamespace.o\
	sysproc.o\
	trap.o\
	trapasm.o\
	uart.o\
	udiv.o\
	vectors.o \
	vm.o\

# Except for pouch
USER_BINARIES :=  	cat cp cpu echo grep init kill ln ls mkdir mount rm sh \
					stressfs timer umount wc zombie ctrl_grp demo_mount_ns \
					demo_pid_ns

POUCH_BINARY := $(B)/pouch/pouch

TESTS_HOST := buf_cache_tests kvector_tests obj_fs_tests
TESTS_GUEST := cgroupstests forktest ioctltests mounttest pidns_tests usertests


KERNEL_OBJS 		:= 	$(addprefix $(B)/,$(KERNEL_OBJS))
USER_BINARIES   	:=  $(addprefix $(B)/user/,$(USER_BINARIES))
CONTAINER_IMAGES 	:=	$(addprefix $(B)/,$(CONTAINER_IMAGES))
IMG_FS				:=  $(addprefix $(B)/,$(IMG_FS))
IMG_XV6				:=  $(addprefix $(B)/,$(IMG_XV6))
KERNEL				:=  $(addprefix $(B)/,$(KERNEL))
MKFS 				:=	$(B)/mkfs
TESTS_HOST 			:=  $(addprefix $(B)/tests/host/,$(TESTS_HOST))
TESTS_GUEST			:=  $(addprefix $(B)/tests/guest/,$(TESTS_GUEST))

.PHONY: all
all: $(B) $(IMG_FS) $(IMG_XV6)

.PHONY: host-tests guest-tests
host-tests: $(B) $(TESTS_HOST)
guest-tests: $(B) $(TESTS_GUEST)

.PHONY: qemu
qemu: $(IMG_FS) $(IMG_XV6)
	$(QEMU) $(QEMUFLAGS)

qemu-gdb: $(IMG_FS) $(IMG_XV6)
	$(QEMU) $(QEMU_GDB_FLAGS) $(QEMUFLAGS)

# Filesystems
# ------------------------------------------------------------------------------
$(IMG_FS): $(MKFS) $(USER_BINARIES) $(POUCH_BINARY) $(CONTAINER_IMAGES) $(TESTS_GUEST)
	$(MKFS) $@ 0 $(USER_BINARIES) $(CONTAINER_IMAGES) $(TESTS_GUEST) $(POUCH_BINARY)

$(IMG_XV6): $(KERNEL)
	dd if=/dev/zero of=$@ count=10000
	dd if=$(B)/bootblock of=$@ conv=notrunc
	dd if=$(KERNEL) of=$@ seek=1 conv=notrunc

# Kernel
# ------------------------------------------------------------------------------
$(KERNEL): $(KERNEL_OBJS) $(B)/vectors.S kernel/kernel.ld $(B)/initcode $(B)/entryother $(B)/bootblock
	$(LD) $(LDFLAGS) -T kernel/kernel.ld -o $(B)/kernel.bin $(KERNEL_OBJS) -b binary $(B)/initcode $(B)/entryother


$(B)/initcode: kernel/initcode.S
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -nostdinc -o $(B)/initcode.o -c $<
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $(B)/initcode.out $(B)/initcode.o
	$(OBJCOPY) -O binary -j .text $(B)/initcode.out $(B)/initcode

$(B)/entryother: kernel/entryother.S
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -Ibuild/ -Ikernel/ -o $(B)/entryother.o -c $<
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o $(B)/bootblockother.o $(B)/entryother.o
	$(OBJCOPY) -O binary -j .text $(B)/bootblockother.o $(B)/entryother

$(B)/bootblock: kernel/bootasm.S kernel/bootmain.c
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -fno-pic -O -nostdinc -Ikernel/ -o $(B)/bootmain.o -c kernel/bootmain.c
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -fno-pic -nostdinc -Ikernel -o $(B)/bootasm.o -c kernel/bootasm.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o $(B)/bootblock.o $(B)/bootasm.o $(B)/bootmain.o
	$(OBJCOPY) -O binary -j .text $(B)/bootblock.o $(B)/bootblock
	./kernel/sign.pl $(B)/bootblock

$(B)/vectors.S: kernel/vectors.pl
	$(PERL) $< > $@

$(B)/vectors.o: $(B)/vectors.S
	cpp $(INCLUDE_DIRS)  $< > $(addsuffix .s,$@)
	$(AS) $(ASFLAGS) $(INCLUDE_DIRS) -c -o $@ $(addsuffix .s,$@)

$(B)/%.o: kernel/%.c
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c -o $@ $<

$(B)/%.o: kernel/%.S
	cpp $(INCLUDE_DIRS)  $< > $(addsuffix .s,$@)
	$(AS) $(ASFLAGS) $(INCLUDE_DIRS) -c -o $@ $(addsuffix .s,$@)

# Userland Binaries
# ------------------------------------------------------------------------------
USER_CFLAGS := $(CFLAGS) -nostdlib -nostdinc

$(B)/user/%: user/%.c $(B)/userlib/userlib.a
	@echo -e "\033[35m[USERBUILD] Building $@\033[0m"
	$(CC) $(USER_CFLAGS) $(INCLUDE_DIRS_USERLAND) \
	-c -o $@.o $<
	$(LD) -o $@ -T user/userspace.ld -N -e main -Ttext 0 $@.o $(abspath $(B)/userlib/userlib.a)

# Userland Library (userlib)
# ------------------------------------------------------------------------------
USERLIB_OBJS := mutex.o printf.o tty.o ulib.o umalloc.o usys.o
USERLIB_OBJS := $(addprefix $(B)/userlib/,$(USERLIB_OBJS))

$(B)/userlib/userlib.a: $(USERLIB_OBJS) user/lib/user.h
	$(AR) --target elf32-i386 rcs $@ $(filter %.o, $^)

$(B)/userlib/%.o: user/lib/%.c
	$(CC) $(USER_CFLAGS) $(INCLUDE_DIRS_USERLAND) -o $@ -c $<

$(B)/userlib/%.o: user/lib/%.S
	cpp $(INCLUDE_DIRS_USERLAND)  $< > $(addsuffix .s,$@)
	$(AS) $(ASFLAGS) $(INCLUDE_DIRS_USERLAND) -c -o $@ $(addsuffix .s,$@)

# Pouch Executable
# ------------------------------------------------------------------------------
POUCH_OBJECTS := $(B)/pouch/pouch.o \
				 $(B)/pouch/image.o \
				 $(B)/pouch/container.o \
				 $(B)/pouch/configs.o \
				 $(B)/pouch/build.o \
				 $(B)/pouch/util.o \
				 $(B)/pouch/start.o
				
POUCH_INCLUDES := $(INCLUDE_DIRS_USERLAND) -Iuser

$(B)/pouch/%.o: user/pouch/%.c
	@echo -e "\033[36m[POUCH] BUILDING POUCH OBJECT $@\033[0m"
	$(CC) $(USER_CFLAGS) $(POUCH_INCLUDES) -o $@ -c $<  # PouchBuild

$(POUCH_BINARY): $(POUCH_OBJECTS) $(B)/userlib/userlib.a
	@echo -e "\033[36m[POUCH] LINKING POUCH\033[0m"
	$(LD) -o $@ -T user/userspace.ld -N -e main -Ttext 0 $^

# Pouch Images
# ------------------------------------------------------------------------------
$(B)/internal_fs_a: $(B)/images/user images/img_internal_fs_a.Dockerfile images/hello.txt
$(B)/internal_fs_b: $(B)/images/user images/img_internal_fs_b.Dockerfile images/test.txt
$(B)/internal_fs_c: $(B)/images/user images/img_internal_fs_c.Dockerfile images/test.txt images/hello.txt

$(CONTAINER_IMAGES):
	@echo ------------------BUILD POUCH IMAGE $(@F)------------------
	$(PODMAN) build -t xv6__$(@F) -f $(filter %.Dockerfile,$^) $(B)/images
	$(PODMAN) save localhost/xv6__$(@F) > $(B)/images/$(@F).tar
	mkdir -p $(B)/images/$(@F)
	tar xf $(B)/images/$(@F).tar -C $(B)/images/$(@F)
	./scripts/podman-image-extractor.sh $(B)/images/$(@F).tar $(B)/images $(MKFS)

	# Copy artifact to buildroot for convenience
	cp -v $(B)/images/$(@F)/build/$(@F) $(B)/$(@F:img_%=%)

$(B)/images/user: $(USER_BINARIES) $(POUCH_BINARY)
	mkdir -p $(B)/images/user
	cp -v $^ $(B)/images/user

# Host utilities
# ------------------------------------------------------------------------------
$(MKFS): mkfs.c
	$(HOSTCC) -o $@ $<

# Tests
# ------------------------------------------------------------------------------
TESTCFLAGS := -m32 -static -Wno-builtin-declaration-mismatch -I. -Itests -Iinclude -Ikernel -Og -ggdb -DHOST_TESTS=1 -std=gnu99

TESTFRAMEWORK := tests/framework/test.h

$(B)/tests/host/obj_fs_tests: 		$(B)/tests/host/obj_fs_tests.o \
									$(B)/tests/host/device_obj_disk_ktbin.o \
									$(B)/tests/host/device_obj_cache_ktbin.o \
									$(B)/tests/host/device_buf_cache_ktbin.o \
									$(B)/tests/host/kvector_ktbin.o \
									$(B)/tests/host/device_ktbin.o \
									$(B)/tests/host/common_mocks.o

$(B)/tests/host/kvector_tests: 		$(B)/tests/host/kvector_tests.o \
									$(B)/tests/host/kvector_ktbin.o \
									$(B)/tests/host/common_mocks.o

$(B)/tests/host/buf_cache_tests: 	$(B)/tests/host/buf_cache_tests.o \
									$(B)/tests/host/device_buf_cache_ktbin.o \
									$(B)/tests/host/common_mocks.o

$(TESTS_HOST):
	$(HOSTCC) $(TESTCFLAGS) $^ -o $@

$(B)/tests/host/%_tests.o: tests/host/%_tests.c $(TESTS_FRAMEWORK) tests/host/common_mocks.h
	$(HOSTCC) $(TESTCFLAGS) -c -o $@ $<

$(B)/tests/host/common_mocks.o: tests/host/common_mocks.c tests/host/common_mocks.h
	$(HOSTCC) $(TESTCFLAGS) -c -o $@ $<

$(B)/tests/host/device_%_ktbin.o: kernel/device/%.c
	$(HOSTCC) $(TESTCFLAGS) -c -o $@ $<

$(B)/tests/host/%_ktbin.o: kernel/%.c
	$(HOSTCC) $(TESTCFLAGS) -c -o $@ $<

# Guest Tests
# ------------------------------------------------------------------------------
GUEST_TEST_CFLAGS := $(CFLAGS) -Itests -Itests/xv6 -I. -nostdlib -nostdinc
GUEST_LDFLAGS := -T user/userspace.ld -N -e main -Ttext 0


$(B)/tests/guest/%: tests/xv6/%.c $(B)/userlib/userlib.a
	$(CC) $(GUEST_TEST_CFLAGS) $(INCLUDE_DIRS) $< -c -o $@.o
	$(LD) $(GUEST_LDFLAGS) $@.o $(B)/userlib/userlib.a -o $@

$(B):
	mkdir -p $(B)
	mkdir -p $(B)/device
	mkdir -p $(B)/fs
	mkdir -p $(B)/user
	mkdir -p $(B)/pouch
	mkdir -p $(B)/userlib
	mkdir -p $(B)/tests
	mkdir -p $(B)/tests/host
	mkdir -p $(B)/tests/guest

	# Prepare for image building
	cp -rv images $(B)/

.PHONY: clean
clean:
	rm -rf "$(B)"
