#!/bin/bash
# This scripts builds xv6, and runs tests on the guest and host.

set -euo # errexit,nounset,pipefail

RED='\033[0;31m'
NC='\033[0m' # No Color

########################################################################
#  Run guest tests
make clean
make TEST_POUCHFILES=1

LOG_FILE="expect_tests.log"
./tests/runtests.exp $LOG_FILE

lines=$(tail -5 $LOG_FILE | grep  "ALL TESTS PASSED" | wc -l)
if [ $lines -ne 1 ]; then
    echo "ALL TESTS PASSED string was not found"
    exit 1
fi

#  Run host tests
make clean
make host-tests

HOSTS_TESTS_DIR="tests/host"
HOST_TESTS_LOG_FILE="host_tests.log"
./${HOSTS_TESTS_DIR}/kvector_tests | tee $HOST_TESTS_LOG_FILE
./${HOSTS_TESTS_DIR}/obj_fs_tests | tee --append $HOST_TESTS_LOG_FILE
./${HOSTS_TESTS_DIR}/buf_cache_tests | tee --append $HOST_TESTS_LOG_FILE

lines=$(cat $HOST_TESTS_LOG_FILE | grep  "FAILED" | wc -l)
if [ $lines -ne 0 ]; then
    echo "FAILED string was found -- host tests failed"
    exit 1
fi

########################################################################
#  Run documentation build
make docs

echo "SUCCESS"
exit 0
