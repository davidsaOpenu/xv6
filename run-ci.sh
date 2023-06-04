i#!/bin/bash

#set -euo # errexit,nounset,pipefail
set -uo # nounset,pipefail


###################################################################################################
# clang-format
# clang-format -dump-config > .clang-format
changed_files=$(find . \( -iname "*.c" -o -iname "*.h" -o -iname "*.S" \) -exec clang-format -i {} \; -exec git diff --name-only {} \;)


if [ -n "$changed_files" ]; then
  echo "Files were changed by clang-format:"
  echo "$changed_files"
  git diff --color=always &> required_changes.txt
  cat required_changes.txt
  false
fi

###################################################################################################
# run cpp style checker
python3 -m venv myenv
. myenv/bin/activate
pip install cpplint
find . -regex ".*\.c\|.*\.h" | xargs cpplint  --filter=-legal/copyright,-readability/casting,-build/include_subdir,-build/header_guard,-runtime/int,-readability/braces

###################################################################################################
# run static analyzer
#!/bin/bash


CPPCHECK=$(whereis -b cppcheck | awk '{print $2;}')

if [ -z "$CPPCHECK" ]; then
  echo CPPCHECK is undefined
fi
ERROR_CODE=20
${CPPCHECK} --error-exitcode=${ERROR_CODE} --enable=portability,information,performance,warning --inconclusive --xml --xml-version=2 . 2> cppcheck.xml
if [ $? -eq  $ERROR_CODE ]; then
  cppcheck-htmlreport --file=cppcheck.xml --report-dir=cppcheck-report --source-dir=. --title="Cppcheck Report"
fi

exit 0
###################################################################################################
# compile
make clean
make

###################################################################################################
#  Run tests
./runtests.exp mylog.txt
