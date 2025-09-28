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


copy_to_vm() {
    echo "Copying xv6 folder to VM..."
    scp -o StrictHostKeyChecking=no -P $ssh_port -r xv6 elk@localhost:/opt/elk
}

build() {
    echo "[*] CI: Starting build"
    ssh -o StrictHostKeyChecking=no -p $ssh_port elk@localhost "cd /opt/elk/xv6 && ./podman.sh"
    echo "[*] CI: Build completed"
}

run_test() {
    if [ "$#" -ne 1 ]; then
        echo "ERROR: run_test should be used with one argument only!"
        exit 1
    fi

    echo "[*] CI: Running single test '$1'"
    time ssh -o StrictHostKeyChecking=no -p $ssh_port elk@localhost "cd /opt/elk/xv6 && expect /opt/elk/xv6/tests/run-guest-tests.exp $1"
    echo "[*] CI: Done running single test '$1'"
}

bypass_test() {
    if [ "$#" -ne 1 ]; then
        echo "ERROR: bypass_test should be used with one argument only!"
        exit 1
    fi

    echo "[*] CI: Bypassing test '$1'"
}

run_all_tests() {
    # Basic
    run_test     mounttest
    run_test     ioctl_syscall_test
    run_test     rm_recursive_test
    run_test     cp_copy_file_test
    run_test     cp_copy_dir_test
    run_test     mount_bind_test
    run_test     umount_bind_mount_test
    run_test     command_exit_status_test

    # Console
    # FIXME: Enable this test when fully merged new console features.
    bypass_test     history_navigation_test

    # Cache
    run_test     proc_cache_entry

    # Cgroups
    run_test     cgroupstests
    run_test     cgroup_io_states_test
    run_test     pidns_tests

    # FS
    run_test     cp_simple_objfs_nativefs_copy_test
    run_test     cp_recursive_objfs_nativefs_test

    # Pouch
    run_test     prepare_pouch_images
    run_test     pouch_basic_tests
    run_test     pouch_stress_test
    run_test     pouch_cgroup_already_exists
    run_test     pouch_to_many_cnts_test
    run_test     pouch_to_many_cnts_test_remove
    run_test     pouch_list_test
    run_test     pouch_disconnect_outside_container_test
    run_test     pouch_info_container_test
    run_test     pouch_cgroup_limit_test
    run_test     remove_pouch_images
    run_test     test_all_pouch_build
    run_test     test_internal_images
}


ssh_port=$(find_free_port)
echo "[*] SSH port mapped to: $ssh_port"

qemu-system-x86_64 \
    -m 2048 \
    -smp 4 \
    -hda $QEMU_IMAGE \
    -nic user,hostfwd=tcp::$ssh_port-:22 \
    -snapshot \
    -enable-kvm \
    -nographic &

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
    if ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=no -p $ssh_port elk@localhost "echo VM is ready" 2>/dev/null; then
    echo "VM is ready after $i attempts"
    break
    fi
    echo "Attempt $i: VM not ready yet, waiting..."
    sleep 5
done

time copy_to_vm
time build
time run_all_tests

echo "[*] CI Done, bye bye!"