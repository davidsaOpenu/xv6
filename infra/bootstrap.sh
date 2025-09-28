#!/bin/bash

set -euo pipefail

IMAGE=ubuntu-24.04-server-cloudimg-amd64-podman.img
QEMU_IMAGE=/home/davidsa/public_html/$IMAGE

trap 'echo [ERROR] Bootstrap: Command failed at "$BASH_COMMAND"; exit 1' ERR

find_free_port() {
    local port
    for port in {2224..2299}; do
        if ! netstat -tuln | grep -q ":$port "; then
            echo "$port"
            return
        fi
    done
    echo 2224  # fallback
}

copy_to_vm() {
    echo "Copying xv6 folder to VM..."
    scp -o StrictHostKeyChecking=no -P "$ssh_port" -r xv6 elk@localhost:/opt/elk
}

build() {
    echo "[*] CI: Starting build"
    ssh -n -tt -o StrictHostKeyChecking=no -p "$ssh_port" elk@localhost "cd /opt/elk/xv6 && ./podman.sh"
    echo "[*] CI: Build completed"
}

ssh_port=$(find_free_port)
echo "[*] SSH port mapped to: $ssh_port"

qemu-system-x86_64 \
    -m 2048 \
    -smp 4 \
    -hda "$QEMU_IMAGE" \
    -nic "user,hostfwd=tcp::$ssh_port-:22" \
    -snapshot \
    -enable-kvm \
    -display none \
    -serial stdio &

VM_PID=$!
sleep 5

if ! kill -0 $VM_PID 2>/dev/null; then
    echo "ERROR: VM process failed to start or crashed immediately"
    echo "VM PID $VM_PID is not running"
    exit 1
fi

echo "VM process is running, waiting for SSH connectivity..."

# Wait for VM to launch (check SSH connectivity)
echo "Waiting for VM to be ready..."

while true; do
    i=1
    if ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=no -p "$ssh_port" elk@localhost "echo VM is ready" 2>/dev/null; then
    echo "VM is ready after $i attempts"
    break
    fi
    echo "Attempt $i: VM not ready yet, waiting..."
    i=$((i + 1))
    sleep 5
done

time copy_to_vm
time build

echo "[*] CI Done, bye bye!"