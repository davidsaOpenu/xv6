
# make a printout
FILES = $(shell grep -v '^\#' runoff.list)
PRINT = runoff.list runoff.spec README toc.hdr toc.ftr $(FILES)

xv6.pdf: $(PRINT)
	./runoff/runoff
	ls -l xv6.pdf

print: xv6.pdf

clean:
	rm -f *.d *.o *.asm *.sym xv6.pdf runoff runoff1 runoff.list toc.hdr toc.ftr
