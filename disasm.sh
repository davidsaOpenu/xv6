#!/usr/bin/env bash

if [[ "$#" -ne 1 ]]; then
    echo "Nothing to disassemble!"
    exit 1
fi

objdump --disassembler-color=extended --visualize-jumps=extended-color -SDTt build/user/pouch "$1" | less -R
