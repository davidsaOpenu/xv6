#!/usr/bin/env bash

set -exu

echo "[Podman CI Starting]"

make -B clean 2>&1          | sed 's/^/[    CLEAN]\t/'
make -B 2>&1                | sed 's/^/[     MAKE]\t/'
make -B host-tests 2>&1     | sed 's/^/[ MAKEHOST]\t/'


# Tcl shenanigans
pkill qemu

