MAKEFILE_DIRECTORY := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

xv6.img: kernel/bootblock kernel/kernel.bin fs.img | windows_debugging
	dd if=/dev/zero of=xv6.img count=10000
	dd if=kernel/bootblock of=xv6.img conv=notrunc
	dd if=kernel/kernel.bin of=xv6.img seek=1 conv=notrunc

xv6memfs.img: kernel/bootblock kernel/kernelmemfs
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=kernel/bootblock of=xv6memfs.img conv=notrunc
	dd if=kernel/kernelmemfs of=xv6memfs.img seek=1 conv=notrunc

include common/common.Makefile


UPROGS=\
	_cat\
	_cp\
	_echo\
	_grep\
	_init\
	_kill\
	_ln\
	_ls\
	_mkdir\
	_rm\
	_sh\
	_stressfs\
	_wc\
	_zombie\
	_mount\
	_umount\
	_timer\
	_cpu\
    _pouch\
    _ctrl_grp\
    _demo_pid_ns\
    _demo_mount_ns


# UPROGS now contains a list of all user programs that are built by the Makefile in the user directory.
# Replace it's value by the very same list, after appending user/ prefix to each item:
UPROGS_ABS=$(patsubst %, user/%, $(UPROGS))

UPROGS_TESTS=\
	tests/xv6/_forktest\
	tests/xv6/_mounttest\
	tests/xv6/_usertests\
	tests/xv6/_pidns_tests\
	tests/xv6/_cgroupstests\
	tests/xv6/_ioctltests\


TEST_ASSETS=

# Add test pouchfiles to the list of test assets, if the TEST_POUCHFILES env is set to 1
ifeq ($(TEST_POUCHFILES), 1)
	TEST_ASSETS += $(wildcard tests/pouchfiles/*)
endif

INTERNAL_DEV=\
	internal_fs_a \
	internal_fs_b \
	internal_fs_c

mkfs: mkfs.c common/fs.h
	gcc -ggdb -Werror -Wall -o mkfs mkfs.c

kernel/%:
	make -C kernel

user/%:
	make -C user $*

tests/xv6/%:
	make -C tests/xv6 $*

# Docker build & skopeo copy, create OCI images.
# Docker daemon must be running and available from this context.
images/img_internal_fs_%: images/build/img_internal_fs_%.Dockerfile
	docker build -t xv6_internal_fs_$* -f images/build/img_internal_fs_$*.Dockerfile images/build
	mkdir -p images/img_internal_fs_$*
	docker run --rm --mount type=bind,source="$(CURDIR)",target=/home/$(shell whoami)/xv6 \
		-w /home/$(shell whoami)/xv6 \
		-v /var/run/docker.sock:/var/run/docker.sock \
		quay.io/skopeo/stable:latest \
		copy docker-daemon:xv6_internal_fs_$*:latest oci:images/img_internal_fs_$*

# This is a dummy target to rebuild the OCI images for the internal fs.
# You should run this target if you have made changes to the internal fs build.
OCI_IMAGES = $(patsubst %, images/img_%, $(INTERNAL_DEV))
build_oci: $(OCI_IMAGES)

# internal_fs_%_img is a direcotry with the relevant OCI image to use for the internal fs build.
internal_fs_%: mkfs
	mkdir -p $(CURDIR)/images/metadata
	./images/oci_image_extractor.sh $(CURDIR)/images/extracted/$@ $(CURDIR)/images/img_$@
	echo $@ >> $(CURDIR)/images/metadata/all_images
	cd $(CURDIR)/images/extracted/$@ && find . -type f -exec ls -la {} \; > $(CURDIR)/images/metadata/img_$*.attr
	./mkfs $@ 1 $$(find $(CURDIR)/images/extracted/$@ -type f) $(CURDIR)/images/metadata/img_$*.attr
	

fs.img: user kernel/kernel.bin mkfs $(UPROGS_ABS) $(UPROGS_TESTS) $(INTERNAL_DEV) $(TEST_ASSETS)
	./mkfs $@ 0 README $(UPROGS_ABS) $(INTERNAL_DEV) $(TEST_ASSETS) $(UPROGS_TESTS) $(CURDIR)/images/metadata/all_images

clean: windows_debugging_clean
	make -C kernel clean
	make -C user clean
	make -C tests clean
	rm -f $(INTERNAL_DEV) mkfs xv6.img xv6memfs.img fs.img

clean_oci:
	rm -rf images/img_internal_fs_*
	docker rmi -f $(shell docker images -q -f "reference=xv6_internal_fs_*") > /dev/null 2>&1 || true

runoff:
	make -C scripts/runoff

# run in emulators

bochs : fs.img xv6.img
	if [ ! -e .bochsrc ]; then ln -s dot-bochsrc .bochsrc; fi
	bochs -q

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := cpus=2,cores=1
endif
QEMUOPTS = -drive file=fs.img,index=1,media=disk,format=raw -drive file=xv6.img,index=0,media=disk,format=raw -smp $(CPUS) -m 512 $(QEMUEXTRA) -nographic

gdb: OFLAGS = -Og -ggdb
gdb: fs.img xv6.img

qemu: fs.img xv6.img
	$(QEMU) -serial mon:stdio $(QEMUOPTS)

qemu-memfs: xv6memfs.img
	$(QEMU) -drive file=xv6memfs.img,index=0,media=disk,format=raw -smp $(CPUS) -m 256

qemu-nox: fs.img xv6.img
	$(QEMU) -nographic $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

qemu-gdb: gdb .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -serial mon:stdio $(QEMUOPTS) -S $(QEMUGDB)

qemu-nox-gdb: gdb .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -nographic $(QEMUOPTS) -S $(QEMUGDB)

# CUT HERE
# prepare dist for students
# after running make dist, probably want to
# rename it to rev0 or rev1 or so on and then
# check in that version.

EXTRA=\
	mkfs.c ulib.c user.h cat.c cp.c echo.c grep.c kill.c ln.c ls.c mkdir.c rm.c\
	stressfs.c wc.c zombie.c printf.c umalloc.c mount.c umount.c timer.c cpu.c\
	mutex.c tests/xv6/forktest.c tests/xv6/mounttest.c tests/xv6/usertests.c\
	tests/xv6/pidns_tests.c tests/xv6/cgroupstests.c tests/xv6/ioctltests.c\
	README dot-bochsrc *.pl toc.* runoff runoff1 runoff.list\
	.gdbinit.tmpl gdbutil\

dist:
	rm -rf dist
	mkdir dist
	for i in $(FILES); \
	do \
		grep -v PAGEBREAK $$i >dist/$$i; \
	done
	sed '/CUT HERE/,$$d' Makefile >dist/Makefile
	echo >dist/runoff.spec
	cp $(EXTRA) dist

dist-test:
	rm -rf dist
	make dist
	rm -rf dist-test
	mkdir dist-test
	cp dist/* dist-test
	cd dist-test; $(MAKE) print
	cd dist-test; $(MAKE) bochs || true
	cd dist-test; $(MAKE) qemu

# update this rule (change rev#) when it is time to
# make a new revision.
tar:
	rm -rf /tmp/xv6
	mkdir -p /tmp/xv6
	cp dist/* dist/.gdbinit.tmpl /tmp/xv6
	(cd /tmp; tar cf - xv6) | gzip >xv6-rev10.tar.gz  # the next one will be 10 (9/17)

windows_debugging_mkdir:
	@mkdir -p windows-debugging

windows_debugging: \
	$(patsubst windows-debugging-templates/%, windows-debugging/%, $(shell find "windows-debugging-templates" -type f))

windows-debugging/%: windows-debugging-templates/% | windows_debugging_mkdir
	@rm -f $@ && \
	cp $< windows-debugging && \
	sed -i 's@{{project_root}}@$(MAKEFILE_DIRECTORY)@g' $@

windows_debugging_clean:
	@rm -rf windows-debugging

.PHONY: dist-test dist windows_debugging windows_debugging_mkdir windows_debugging_clean

host-tests: 
	make -C tests/host

host-tests-debug: OFLAGS = -Og -ggdb
host-tests-debug: host-tests
