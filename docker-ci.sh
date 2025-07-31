#!/bin/bash

# Display help for this CI script
help() {
cat <<EOF
docker-ci.sh: a CI script for xv6

Commands:
    build           Build a docker image from the Dockerfile.
    test            Run xv6 test suite.
    shell           Run bash inside the container.
    format          Run clang-format on the codebase.
    lint            Run various linting tools on the codebase.
    help            Display this message and exit.

Maintainers:
    Ron Shabi <ron@ronsh.net>
EOF
}

print_error() {
    tput setaf 1
    echo -n "ERROR: "
    tput sgr0
    echo "$1"
}

print_success() {
    tput setaf 2
    echo -n "OK: "
    tput sgr0
    echo "$1"
}

print_warning() {
    tput setaf 3
    echo -n "WARN: "
    tput sgr0
    echo "$1"
}

DEFAULT_TAG=xv6-ubuntu-2204

build() {
    docker build -t "$DEFAULT_TAG" . && \
    print_success "Built container $DEFAULT_TAG"
}

shell() {
    docker run \
        --rm \
        -it \
        -v ".:/xv6" \
        -w "/xv6" \
        "$DEFAULT_TAG"
}

format() {
    bash ./scripts/format.sh && \
    print_success "Run clang-format"
}

case "$1" in
    "build" )
        build ;;
    "test" )
        print_error "Not implemented yet :("
        exit 1 ;;
    "shell" )
        shell ;;
    "help" )
        help
        exit 0 ;; 
    "format" )
        format ;;
    "lint" )
        print_error "Not implemented yet :("
        exit 1 ;;
    * )
        help
        exit 1 ;;
esac
