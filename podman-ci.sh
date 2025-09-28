#!/usr/bin/env bash

help() {
cat << EOF
podman-ci utility
    <no arguments>          build & test everything
    --skip-build:           do not build
    --skip-guest-tests:     do not run guest-tests
    --skip-host-tests:      do not run host-tests
    -h, --help:             show this screen
EOF
}

SKIP_GUEST_TESTS=false
SKIP_BUILD=false
SKIP_HOST_TESTS=true        # FIXME: change to false in the future.

while [[ "$#" -ne 0 ]]; do
    case "$1" in
        --skip-build)
            SKIP_BUILD=true
        ;;
        --skip-guest-tests)
            SKIP_GUEST_TESTS=true
        ;;
        --skip-host-tests)
            SKIP_HOST_TESTS=true
        ;;
        --help | -h)
            help
            exit
        ;;
    esac

    shift
done

pkill -e qemu-system-i386



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

HOST_TESTS_DIR=$(realpath "build/tests/host")
HOST_TESTS=$(find "$HOST_TESTS_DIR" -executable -print)
    for t in $HOST_TESTS; do
        $t 2>&1     | sed 's/^/[HOSTTESTS]\t/'
    done
else
    echo "[    SKIP] Host Tests"
fi
