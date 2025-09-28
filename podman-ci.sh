#!/usr/bin/env bash

echo "[Podman CI Starting]"

make clean 2>&1             | sed 's/^/[    CLEAN]\t/'
make -B 2>&1                | sed 's/^/[     MAKE]\t/'
make host-tests -B 2>&1     | sed 's/^/[ MAKEHOST]\t/'
make guest-tests -B 2>&1    | sed 's/^/[MAKEGUEST]\t/'