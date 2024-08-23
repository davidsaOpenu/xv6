################################################################################
# Common makefile with common variables and rules for xv6 user/kernel programs.
################################################################################

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

########## CFLAGS ##########
CFLAGS = -static -MD -m32 -mno-sse -gstabs -std=gnu99 -Wall -Werror -Wstack-usage=4096 \
	-fno-pic -fno-builtin -fno-strict-aliasing -fno-omit-frame-pointer $(OFLAGS) \
	-I$(MAKEFILE_DIRECTORY) -I$(MAKEFILE_DIRECTORY)/tests/framework

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

ASFLAGS = -m32 -gdwarf-2 -Wa,-divide
# FreeBSD ld wants ``elf_i386_fbsd''
LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null | head -n 1)

-include *.d
