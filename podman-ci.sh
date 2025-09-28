#!/usr/bin/env bash

cd images/build
podman image build -t test1 -f img_internal_fs_a.Dockerfile .
podman image save test1 --format oci-dir -o testdir