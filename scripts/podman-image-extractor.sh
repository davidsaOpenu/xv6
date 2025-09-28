#!/usr/bin/env bash

set -x

if [[ "$#" -ne 3 ]]; then
    echo "usage: $0 [podman-tarfile] [output-directory] [mkfs-executable]"
    exit 1
fi

TAR="$1"
BASEDIR="$2"
MKFS="$3"

if [[ ! -d "$BASEDIR" ]]; then
    echo "Error: '$BASEDIR' is not a direcory"
    exit 1
fi

if [[ ! -f "$TAR" ]]; then
    echo "Error: the file '$TAR' does not exist"
    exit 1
fi

if [[ ! -f "$MKFS" ]]; then
    echo "Error: the mkfs executable '$MKFS' does not exist"
    exit 1
fi

BASEDIR="$(realpath -e "$BASEDIR")"
OUTDIR_NAME="$(basename "$TAR" .tar)"
FULL_OUT_DIR="${BASEDIR}/${OUTDIR_NAME}"


echo "[+] Creating a new direcotry '$OUTDIR_NAME' in '$BASEDIR'"
mkdir -p "$FULL_OUT_DIR"

FILETAR="$(jq -r '.[0]["Layers"][0]' "$FULL_OUT_DIR/manifest.json")"
FILETAR_FULLPATH="${FULL_OUT_DIR}/${FILETAR}"
FILETAR_OUTDIR="$FULL_OUT_DIR/files"

echo "[+] Extracting tar file"
tar xvf "$TAR" -C "$FULL_OUT_DIR"


echo "[+] Actual files are in:"
echo "$FILETAR_FULLPATH"

echo "[+] Extracting 'actual' tar file"
mkdir -p "$FILETAR_OUTDIR"
tar xvf "$FILETAR_FULLPATH" -C "$FILETAR_OUTDIR"

echo "[+] Building xv6fs"
mkdir -p "$FULL_OUT_DIR/build"
$MKFS "${FULL_OUT_DIR}/build/${OUTDIR_NAME}" 1 $FILETAR_OUTDIR/*

exit 0 # odd that mkfs exits with 1