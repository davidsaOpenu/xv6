#!/usr/bin/env bash

set -exu

SKIP_BUILD=false
SKIP_GUEST_TESTS=false
SKIP_HOST_TESTS=false
HOST_TESTS_DIR=$(realpath "build/tests/host")
HOST_TESTS=$(find "$HOST_TESTS_DIR" -executable -print)

echo "[Podman CI Starting]"

if [[ ! "$SKIP_BUILD" = "true" ]]; then
    make -B clean 2>&1          | sed 's/^/[    CLEAN]\t/'
    make -B 2>&1                | sed 's/^/[     MAKE]\t/'
    make -B host-tests 2>&1     | sed 's/^/[ MAKEHOST]\t/'
else
    echo "[    SKIP] Build"
fi

# Run Guest Tests

if [[ ! "$SKIP_GUEST_TESTS" = "true" ]]; then
    ./tests/runtests.exp 2>&1 /dev/null     | sed 's/^/[  EXPECT]\t/'
else
    echo "[    SKIP] Guest Tests"
fi

# Run Host Tests

if [[ ! "$SKIP_HOST_TESTS" = "true" ]]; then
    for t in $HOST_TESTS; do
        $t 2>&1     | sed 's/^/[HOSTTESTS]\t/'
    done
else
    echo "[    SKIP] Host Tests"
fi
