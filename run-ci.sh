#!/bin/bash

#set -euo # errexit,nounset,pipefail
# TODO: uncomment -euo when run-ci.sh passes all tests
set -uo # nounset,pipefail


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
python3 -m venv /tmp/myenv
. /tmp/myenv/bin/activate
pip install cpplint
FILTERS=-legal/copyright,-readability/casting,-build/include_subdir,
FILTERS+=-build/header_guard,-runtime/int,-readability/braces,
FILTERS+=-build/include_what_you_use,-runtime/printf,-readability/check,
FILTERS+=-build/include,-runtime/casting
find . -regex ".*\.[c|h]$" | xargs cpplint --filter=$FILTERS

# TODO: remove when run-ci.sh passes all tests
if [ $? -ne 0 ]; then
    echo "Cpplint comments are not completed."
    exit 1
fi

########################################################################
# install and run bashate
pip install bashate
find . -iname "*.sh" -exec bashate {} \; > /tmp/bashate-out
cat /tmp/bashate-out

grep "warning(s) found" /tmp/bashate-out 1>/dev/null
if [ $? -eq 0 ]; then
    echo "Bashate comments are not completed."
    exit 1
fi

########################################################################
# run static analyzer
ERROR_CODE=20
cppcheck --error-exitcode=${ERROR_CODE} --inline-suppr \
        --enable=portability,information,performance,warning \
    --inconclusive --suppress=missingIncludeSystem -DSTORAGE_DEVICE_SIZE=1 \
    --xml --xml-version=2 . 2> cppcheck.xml
if [ $? -eq  $ERROR_CODE ]; then
    cppcheck-htmlreport --file=cppcheck.xml --report-dir=cppcheck-report \
        --source-dir=. --title="Cppcheck Report"
    echo "##### Cppcheck.xml #####"
    cat cppcheck.xml
    exit 1
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
