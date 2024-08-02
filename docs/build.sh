#!/bin/bash

set -e

python -m venv /tmp/virtdocs
source /tmp/virtdocs/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
cd source && make html 2>&1 | grep -v "WARNING" && \
    [ ${PIPESTATUS[0]} -eq 0 ] && echo "Documentation build successful!" || \
    echo "Documentation build failed!"
