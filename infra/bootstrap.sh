#!/bin/bash

IMAGE=ubuntu-24.04-server-cloudimg-amd64-podman.img
QEMU_IMAGE=/home/davidsa/public_html/$IMAGE

find_free_port() {
    local port
    for port in {2224..2299}; do
        if ! netstat -tuln | grep -q ":$port "; then
            echo $port
            return
        fi
    done 
    echo 2224  # fallback
}


# cd "$TEMPDIR" || exit 1
# rsync -aP "$QEMU_IMAGE" .

free_tcp_port=$(find_free_port)
echo "Using port: $free_tcp_port"

# echo $free_tcp_port > vm_port.txt
# echo $TMP_IMAGE > vm_image.txt

qemu-system-x86_64 \
    -m 2048 \
    -smp 4 \
    -hda $QEMU_IMAGE \
    -nic user,hostfwd=tcp::$free_tcp_port-:22 \
    -snapshot \
    -enable-kvm \
    -nographic &

VM_PID=$!
# echo $VM_PID > vm_pid.txt
# echo "Started VM with PID: $VM_PID"

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
if ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=no -p $free_tcp_port elk@localhost "echo VM is ready" 2>/dev/null; then
echo "VM is ready after $i attempts"
break
fi
echo "Attempt $i: VM not ready yet, waiting..."
sleep 5
done

echo "Copying xv6 folder to VM..."
scp -o StrictHostKeyChecking=no -P $free_tcp_port -r xv6 elk@localhost:/opt/elk && \
ssh -o StrictHostKeyChecking=no -p $free_tcp_port elk@localhost "cd /opt/elk/xv6; ./podman-ci.sh"