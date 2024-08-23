################################################################################
# Contains clean target to remove all user binaries and intermediate files in subdirectories.
################################################################################

.PHONY: host clean

host:
	make -C host

xv6/%:
	make -C xv6 $*

clean:
	make -C xv6 clean
	make -C host clean
