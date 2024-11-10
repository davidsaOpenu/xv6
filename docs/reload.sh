#!/bin/bash

set -e

SCRIPT_DIR=$(cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)
source /tmp/virtdocs/bin/activate
python -m pip install --upgrade pip sphinx-autobuild
sphinx-autobuild ${SCRIPT_DIR}/source ${SCRIPT_DIR}/source/_build/html
