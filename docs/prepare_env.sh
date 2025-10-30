#!/bin/bash

set -e

python3 -m venv /tmp/virtdocs
source /tmp/virtdocs/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
