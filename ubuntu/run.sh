#!/bin/bash

ISO=ubuntu-24.04.3-live-server-amd64.iso
DISK=ubuntu-server.qc2
SRCDIR=~/src/xv6
# qemu-image create -f qcow2 100G "$DISK"

qemu-system-x86_64 \
    -enable-kvm \
    -m 4G \
    -smp 4 \
    -cdrom "$ISO" \
    -hda "$DISK" \
    -display sdl \
    -fsdev local,path="$SRCDIR",security_model=mapped-xattr,id=xv9p \
    -device virtio-9p-pci,fsdev=xv9p,mount_tag=xv6 \
    -net nic \
    -net user,hostfwd=tcp::2222-:22
