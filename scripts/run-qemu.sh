#!/bin/bash
# Run HackersOS in QEMU
# Usage: ./scripts/run-qemu.sh [--disk] [--debug]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
KERNEL="$PROJECT_DIR/build/kernel.bin"

if [ ! -f "$KERNEL" ]; then
    echo "Error: kernel.bin not found. Run 'make build' first."
    exit 1
fi

QEMU_ARGS=(
    -M virt
    -cpu cortex-a72
    -m 256M
    -nographic
    -kernel "$KERNEL"
    -serial mon:stdio
)

# Add disk if requested
if [[ "$*" == *"--disk"* ]]; then
    DISK_IMG="$PROJECT_DIR/disk.img"
    if [ ! -f "$DISK_IMG" ]; then
        echo "Creating disk image..."
        "$SCRIPT_DIR/make-disk-image.sh"
    fi
    QEMU_ARGS+=(
        -drive "file=$DISK_IMG,if=none,format=raw,id=hd0"
        -device virtio-blk-device,drive=hd0
    )
fi

# Add GDB debug if requested
if [[ "$*" == *"--debug"* ]]; then
    QEMU_ARGS+=(-s -S)
    echo "Waiting for GDB connection on :1234..."
    echo "Connect with: aarch64-none-elf-gdb -ex 'target remote :1234' build/kernel.elf"
fi

exec qemu-system-aarch64 "${QEMU_ARGS[@]}"
