#!/bin/bash

set -euo # errexit,nounset,pipefail
if false; then
#########################################################################
# clang-format
# clang-format -style=google -dump-config > .clang-format
changed_files=$(find . -regex ".*\.[c|h]$" -exec clang-format -i {} \; \
    -exec git diff --name-only {} \;)

if [ -n "$changed_files" ]; then
    echo "Files were changed by clang-format:"
    echo "$changed_files"
    git diff --color=always &> required_changes.txt
    cat required_changes.txt
    exit 1
fi

#########################################################################
# install and run cpp style checker
python3 -m venv ~/.venv/myenv
. ~/.venv/myenv/bin/activate
pip install cpplint
# build/include_what_you_use relates to libc headers - see NOLINT() in files.
# runtime/printf recommands to choose the right one out of libc s/n/printf/c.
FILTERS=-legal/copyright,-readability/casting,-build/include_subdir,
FILTERS+=-build/header_guard,-runtime/int,-readability/braces,-runtime/printf
find . -regex ".*\.[c|h]$" | xargs cpplint --filter=$FILTERS

########################################################################
# install and run bashate
pip install bashate
find . -iname "*.sh" -exec bashate {} \; > ~/.venv/bashate-out
cat ~/.venv/bashate-out

# Grep returns 0 if the string was found and 1 otherwise.
# ! negate the return value of grep to fail the tests if
#  warnings/errors were found.
! grep "warning(s) found" ~/.venv/bashate-out 1>/dev/null
! grep "error(s) found" ~/.venv/bashate-out 1>/dev/null

# Check whitespaces in .sh files
# Second grep is a cheat for the return value together with "!" for set -euo.
! find . -type f -name "*.sh" -exec \
    grep -nHE "[[:space:]]+$" {} \; | grep -e ".*"

########################################################################
# run static analyzer
ERROR_CODE=20
# TODO(David): Check why cppcheck gerrit plugin doesn't create html report.
cppcheck --error-exitcode=${ERROR_CODE} --inline-suppr \
    --enable=portability,information,performance,warning --inconclusive \
    -DSTORAGE_DEVICE_SIZE=1 --xml --xml-version=2 . 2> cppcheck.xml
# Gerrit pluging creates htmlreports automatically from cppcheck.
# cppcheck-htmlreport --file=cppcheck.xml --report-dir=cppcheck-report \
#    --source-dir=. --title="Cppcheck Report"
fi
########################################################################
# compile
make clean
make

########################################################################
#  Run tests
LOG_FILE="mylog.txt"
./runtests.exp $LOG_FILE


########################################################################
#  Last verification
lines=$(tail -5 $LOG_FILE | grep  "ALL TESTS PASSED" | wc -l)
if [ $lines -ne 1 ]; then
    echo "ALL TESTS PASSED string was not found"
    exit 1
fi

echo "SUCCESS"
exit 0