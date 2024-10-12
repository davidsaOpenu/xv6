#!/bin/bash
# This script builds XV6's user binaries so they can be added in the container images.

make clean
make user
