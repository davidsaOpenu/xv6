#!/bin/bash

virtualenv virtdocs
source virtdocs/bin/activate
pip install -r requirements.txt
cd source; make html
