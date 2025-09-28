#!/usr/bin/env bash

set -euo pipefail

help() {
cat << EOF
podman-ci utility
    <no arguments>          build & test everything
    --skip-build:           do not build
    --skip-guest-tests:     do not run guest-tests
    --skip-host-tests:      do not run host-tests
    --skip-lint:            do not run linters
    -h, --help:             show this screen
EOF
}

trap 'echo [ERROR] CI: Command failed at "$BASH_COMMAND"; exit 1' ERR

SKIP_LINT=false
SKIP_BUILD=false
SKIP_GUEST_TESTS=false
SKIP_HOST_TESTS=false


while [[ "$#" -ne 0 ]]; do
    case "$1" in
        --skip-build)
            SKIP_BUILD=true
        ;;
        --skip-lint)
            SKIP_LINT=true
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

echo "[Podman CI Starting]"

lint_shell() {
    if ! command -v shellcheck; then
        echo "Skipping linting: shellcheck is not installed"
        return 0
    fi

    shell_scripts=$(find . -name '*.sh'  -not -path './docs/*' -not -path './build/*')
    for script in $shell_scripts; do
        shellcheck "$script"          | sed "s/^/[SHELLCHECK]\t/";

        # shellcheck disable=SC2181 # Because of sed
        if [ "$?" -ne 0 ]; then
            echo "FAILED ON SHELLCHECK: $script"
            exit 1
        fi
    done
}

lint_clang_format() {
    if ! command -v clang-format-16; then
        echo "Skipping linting: clang-format-16 is not installed"
        return 0
    fi

    source_files=$(find . -name '*.[ch]'  -not -path './docs/*' -not -path './build/*')

    for f in $source_files; do
        clang-format-16 -i --verbose "$f" 2>&1 1>/dev/null | sed "s/^/[CLANG-FORMAT]\t/";
    done
}

lint_eol_spaces() {
    if [[ "$#" -ne 1 ]]; then
        echo "FATAL: lint_eol_spaces requires one argument"
	exit 1
    fi

    spacelines="$(grep -Pc '\s+$' "$1")"

    if [[ ! "$spacelines" -eq 0 ]]; then
        echo "Error: Extraneous spaces found in file '$1':"
        grep -PrnH '\s+$' "$1"
        exit 1
    fi
}


lint_expect_scripts() {
    source_files=$(find tests -name '*.exp')

    for f in $source_files; do
	lint_eol_spaces "$f"
    done
}


if [[ ! "$SKIP_LINT" = "true" ]]; then
    echo "[    LINT] Shell Scripts"
    lint_shell

    echo "[    LINT] Expect scripts"
    lint_expect_scripts

    echo "[    LINT] Shell Scripts OK"
    echo "[    LINT] Clang-Format"
    lint_clang_format
    echo "[    LINT] Clang-Format OK"
fi

if [[ ! "$SKIP_BUILD" = "true" ]]; then
    make -B clean 2>&1          | sed 's/^/[    CLEAN]\t/'
    make -B 2>&1                | sed 's/^/[     MAKE]\t/'
    make -B host-tests 2>&1     | sed 's/^/[ MAKEHOST]\t/'
else
    echo "[    SKIP] Lint"
fi

# Run Guest Tests

if [[ ! "$SKIP_GUEST_TESTS" = "true" ]]; then
    expect ./tests/run-guest-tests.exp 2>&1 | sed 's/^/[  EXPECT]\t/'
else
    echo "[    SKIP] Guest Tests"
fi

# Run Host Tests

if [[ ! "$SKIP_HOST_TESTS" = "true" ]]; then

HOST_TESTS_DIR=$(realpath "build/tests/host")
HOST_TESTS=$(find "$HOST_TESTS_DIR" -executable -type f)
    for t in $HOST_TESTS; do
        $t 2>&1     | sed 's/^/[HOSTTESTS]\t/'
    done
else
    echo "[    SKIP] Host Tests"
fi
