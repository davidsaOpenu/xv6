#!/bin/bash

set -euo # errexit,nounset,pipefail

RED='\033[0;31m'
NC='\033[0m' # No Color

#########################################################################
# clang-format
# clang-format -style=google -dump-config > .clang-format
# changed_files=$(find . -regex ".*\.[c|h]$" -exec clang-format -i {} \; \
#     -exec git diff --name-only {} \;)

# if [ -n "$changed_files" ]; then
#     echo "Files were changed by clang-format:"
#     echo "$changed_files"
#     git diff --color=always &> required_changes.txt
#     cat required_changes.txt
#     exit 1
# fi

#########################################################################
# install and run cpp style checker
source $XV6_VENV/bin/activate
# build/include_what_you_use relates to libc headers - see NOLINT() in files.
# runtime/printf recommands to choose the right one out of libc s/n/printf/c.
FILTERS=-legal/copyright,-readability/casting,-build/include_subdir,
FILTERS+=-build/header_guard,-runtime/int,-readability/braces,-runtime/printf
find . -regex ".*\.[c|h]$" | xargs cpplint --filter=$FILTERS

########################################################################
# install and run bashate
find . -iname "*.sh" -exec bashate {} \; > $XV6_VENV/bashate-out
cat $XV6_VENV/bashate-out

# Grep returns 0 if the string was found and 1 otherwise.
# ! negate the return value of grep to fail the tests if
#  warnings/errors were found.
! grep "warning(s) found" $XV6_VENV/bashate-out 1>/dev/null || exit 1
! grep "error(s) found" $XV6_VENV/bashate-out 1>/dev/null || exit 1

# Check whitespaces in .sh files
# Second grep is a cheat for the return value together with "!" for set -euo.
! find . -type f -name "*.sh" -exec grep -nHE "[[:space:]]+$" {} \; \
    | grep -e ".*" || exit 1

########################################################################
# TODO(David): Check why cppcheck plugin skips html report creation from
#              time to time

# run static analyzer
ERROR_CODE=20

# suppress finding standard include headers, scan only custom header files.
cppcheck --error-exitcode=${ERROR_CODE} \
    --inline-suppr --suppress=missingIncludeSystem \
    --enable=portability,information,performance,warning --inconclusive \
    -DSTORAGE_DEVICE_SIZE=1 "-I$(pwd)" \
    --xml --xml-version=2 . 2> cppcheck.xml || \
    { echo "${RED}Failed: please check cppcheck.xml for details.${NC}"; \
    exit 1; }
# Gerrit pluging creates htmlreports automatically from cppcheck.
# cppcheck-htmlreport --file=cppcheck.xml --report-dir=cppcheck-report \
#    --source-dir=. --title="Cppcheck Report"

########################################################################
#  Run guest tests
make clean
make TEST_POUCHFILES=1

LOG_FILE="mylog.txt"
./runtests.exp $LOG_FILE

lines=$(tail -5 $LOG_FILE | grep  "ALL TESTS PASSED" | wc -l)
if [ $lines -ne 1 ]; then
    echo "ALL TESTS PASSED string was not found"
    exit 1
fi

#  Run host tests
make clean
make host-tests

HOST_TESTS_LOG_FILE="host_tests_log.txt"
./kvector_tests | tee $HOST_TESTS_LOG_FILE
./objfs_tests | tee --append $HOST_TESTS_LOG_FILE


lines=$(cat $HOST_TESTS_LOG_FILE | grep  "FAILED" | wc -l)
if [ $lines -ne 0 ]; then
    echo "FAILED string was found -- host tests failed"
    exit 1
fi

########################################################################
#  Run documentation build
cd docs; ./build.sh # the scripts fails on errors and/or warnings

echo "SUCCESS"
exit 0
