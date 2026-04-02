#!/bin/bash
# Build test programs and create a disk image with them
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
CROSS=aarch64-unknown-linux-gnu

echo "=== Building test programs ==="

cd "$SCRIPT_DIR"

# Build minimal test binaries
${CROSS}-gcc -static -nostdlib -o hello hello.S
${CROSS}-gcc -static -nostdlib -o sysinfo sysinfo.S

echo "  hello:   $(file hello | cut -d: -f2)"
echo "  sysinfo: $(file sysinfo | cut -d: -f2)"

# Create test disk image (64MB FAT32)
DISK="$PROJECT_DIR/test-disk.img"
dd if=/dev/zero of="$DISK" bs=1M count=64 2>/dev/null

if command -v mkfs.fat &>/dev/null; then
    mkfs.fat -F 32 -n TESTDISK "$DISK"
else
    # Use mformat as fallback (from mtools)
    mformat -F -v TESTDISK -i "$DISK" ::
fi

# Copy test binaries to disk using mtools
if command -v mcopy &>/dev/null; then
    mcopy -i "$DISK" hello ::hello
    mcopy -i "$DISK" sysinfo ::sysinfo

    # Copy companion apps if they exist
    for app in "$PROJECT_DIR/apps"/*; do
        [ -f "$app" ] || continue
        name=$(basename "$app")
        echo "  Adding: $name"
        mcopy -i "$DISK" "$app" "::$name" 2>/dev/null || echo "  (skipped $name — too large for test disk)"
    done

    echo ""
    echo "Disk contents:"
    mdir -i "$DISK" ::
else
    echo "Warning: mtools not installed, disk image is empty"
    echo "Install: brew install mtools"
fi

echo ""
echo "Test disk: $DISK"
echo ""
echo "Run: qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -nographic \\"
echo "       -kernel build/kernel.bin \\"
echo "       -drive file=test-disk.img,if=none,format=raw,id=hd0 \\"
echo "       -device virtio-blk-device,drive=hd0"
