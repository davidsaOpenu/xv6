#!/bin/bash

OUTPUT_DIR=$1
OCI_IMAGE_DIR=$2

if [ -d $OUTPUT_DIR ]; then
    rm -rf $OUTPUT_DIR
fi

echo "[OCI IMAGE EXTRACTOR] Changing dir to $OCI_IMAGE_DIR"
cd $OCI_IMAGE_DIR
mkdir -p $OUTPUT_DIR

DIGEST_OF_MANIFEST=$(jq --raw-output '.manifests[] |
    select(.mediaType != null) |
    select(.mediaType == "application/vnd.oci.image.manifest.v1+json") |
    .digest' index.json)

if [ -z $DIGEST_OF_MANIFEST ]; then
    echo "No manifest found!"
    exit 1
fi

MANIFEST_DIGEST_VAL=$(echo ${DIGEST_OF_MANIFEST}| cut -d':' -f2)
BLOBS_DIR="blobs/$(echo ${DIGEST_OF_MANIFEST}| cut -d':' -f1)"

echo "[OCI IMAGE EXTRACTOR] manifest.json is at $BLOBS_DIR/$MANIFEST_DIGEST_VAL"
cd $BLOBS_DIR
LAYERS_DIGESTS=$(jq --raw-output '.layers[] | .digest' $MANIFEST_DIGEST_VAL)
LAYERS=$(echo $LAYERS_DIGESTS | cut -d':' -f2)

if [ -z $LAYERS ]; then
    echo "[OCI IMAGE EXTRACTOR] No layers found!"
    exit 1
fi
echo "[OCI IMAGE EXTRACTOR] Layers are: $LAYERS"

echo "[OCI IMAGE EXTRACTOR] Extracting layers to $OUTPUT_DIR"
for LAYER in $LAYERS; do
    cp $LAYER $OUTPUT_DIR/$LAYER.tgz && \
    tar --overwrite -xzf $OUTPUT_DIR/$LAYER.tgz -C $OUTPUT_DIR && \
    rm $OUTPUT_DIR/$LAYER.tgz && \
    echo "[OCI IMAGE EXTRACTOR] $LAYER"
done
