################################################
# Common Makefile for all xv6 components
# contains common rules and variables for all Makefiles of xv6.
################################################

COMMON_MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
# .PRECIOUS: %.o

TOOLPREFIX =

QEMU=qemu-system-i386
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
	-Wno-error=stringop-overflow \
	-fno-stack-protector
	
#x86
HOST_CPU_TSC_FREQ := $(shell cat /proc/cpuinfo | grep -i "cpu mhz" | head -n 1 | rev | cut -d ' ' -f 1 | rev | cut -d '.' -f 1)*1000
CFLAGS += -DXV6_TSC_FREQUENCY=$(HOST_CPU_TSC_FREQ)

ifeq ($(pause_debug), true)
CFLAGS += -DXV6_WAIT_FOR_DEBUGGER=1
else
CFLAGS += -DXV6_WAIT_FOR_DEBUGGER=0
endif

OFLAGS = -O2
CFLAGS += -DSTORAGE_DEVICE_SIZE=327680
ASFLAGS = -m32 -gdwarf-2 -Wa,-divide $(INCLUDE_COMMON)


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

# -include *.d