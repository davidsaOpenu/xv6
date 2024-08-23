################################################################################
# Contains clean target to remove all user binaries and intermediate files in subdirectories.
################################################################################

.PHONY: host clean

host:
	$(MAKE) -C host

xv6/%:
	$(MAKE) -C xv6 $*

clean:
	$(MAKE) -C xv6 clean
	$(MAKE) -C host clean
