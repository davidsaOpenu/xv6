################################################
# Common Makefile for all xv6 components
# contains common rules and variables for all Makefiles of xv6.
################################################

COMMON_MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

# Cross-compiling (e.g., on Mac OS X)
# TOOLPREFIX = i386-jos-elf

# Using native tools (e.g., on X86 Linux)
# TOOLPREFIX =

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i386-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
	then echo ''; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your TOOLPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# If the makefile can't find QEMU, specify its path here
# QEMU = qemu-system-i386

# Try to infer the correct QEMU
ifndef QEMU
QEMU = $(shell if which qemu > /dev/null 2>&1; \
	then echo qemu; exit; \
	elif which qemu-system-i386 > /dev/null 2>&1; \
	then echo qemu-system-i386; exit; \
	elif which qemu-system-x86_64 > /dev/null 2>&1; \
	then echo qemu-system-x86_64; exit; \
	else \
	qemu=/Applications/Q.app/Contents/MacOS/i386-softmmu.app/Contents/MacOS/i386-softmmu; \
	if test -x $$qemu; then echo $$qemu; exit; fi; fi; \
	echo "***" 1>&2; \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2; \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2; \
	echo "*** or have you tried setting the QEMU variable in Makefile?" 1>&2; \
	echo "***" 1>&2; exit 1)
endif

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
INCLUDE_COMMON += -I$(realpath $(COMMON_MAKEFILE_DIR)) -I$(realpath $(COMMON_MAKEFILE_DIR)include)
########## CFLAGS ##########
CFLAGS = -static -MD -m32 -mno-sse -std=gnu99 -Wall -Werror -Wstack-usage=4096 \
	-fno-pic -fno-builtin -fno-strict-aliasing -fno-omit-frame-pointer $(OFLAGS) $(INCLUDE_COMMON) \
	-Wno-error=infinite-recursion \
	-Wno-error=stringop-overflow
	
#x86
HOST_CPU_TSC_FREQ := $(shell cat /proc/cpuinfo | grep -i "cpu mhz" | head -n 1 | rev | cut -d ' ' -f 1 | rev | cut -d '.' -f 1)*1000
#ARM
#HOST_CPU_TSC_FREQ := $(shell cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq )
CFLAGS += -DXV6_TSC_FREQUENCY=$(HOST_CPU_TSC_FREQ)

ifeq ($(pause_debug), true)
CFLAGS += -DXV6_WAIT_FOR_DEBUGGER=1
else
CFLAGS += -DXV6_WAIT_FOR_DEBUGGER=0
endif

OFLAGS = -O2
CFLAGS += -DSTORAGE_DEVICE_SIZE=327680
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
############################

ASFLAGS = -m32 -gdwarf-2 -Wa,-divide $(INCLUDE_COMMON)
# FreeBSD ld wants ``elf_i386_fbsd''
LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null | head -n 1)


# Variables for user programs -- user library path and objects.
USER_LIB_BASE = $(realpath $(COMMON_MAKEFILE_DIR)/user/lib)
ULIB_OBJ_NAMES = ulib.o usys.o printf.o umalloc.o tty.o mutex.o
ULIB = $(addprefix $(USER_LIB_BASE)/, $(ULIB_OBJ_NAMES))

USER_LD_FLAGS = $(LDFLAGS) -T $(realpath $(dir $(COMMON_MAKEFILE_DIR))/user/userspace.ld) -N -e main -Ttext 0
USER_CFLAGS = $(CFLAGS) -fno-builtin -I$(realpath $(COMMON_MAKEFILE_DIR)/user)

# Host (mostly tests)
# NOTE! gcc-multilib is required to build 32-bit binaries on 64-bit systems.
HOST_CFLAGS = -static -m32 -MD -std=gnu99 -Wall -Werror -Wno-builtin-declaration-mismatch -DHOST_TESTS
HOST_CFLAGS += $(INCLUDE_COMMON) -I$(realpath $(COMMON_MAKEFILE_DIR)/tests)

-include *.d

# This function dumps objects and symbols to .asm and .sym files if DUMP_OBJ_AND_SYMS is set to 1,
# and according to the object type (e.g. .text section for executables).
define dump_objs
    if [ "$(DUMP_OBJ_AND_SYMS)" = "1" ]; then \
		$(OBJDUMP) -h $1 | grep -q .text && $(OBJDUMP) -S $1 > $1.asm; \
		$(OBJDUMP) -t $1 | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $1.sym; \
    fi
endef