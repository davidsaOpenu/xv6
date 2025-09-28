#!/usr/bin/env bash

echo "[Podman CI Starting]"

make 2>&1 | sed 's/^/[make]\t/'