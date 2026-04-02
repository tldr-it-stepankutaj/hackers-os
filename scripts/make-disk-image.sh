#!/bin/bash
# Create a FAT32 disk image for HackersOS
# This image is used as the root filesystem in QEMU

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DISK_IMG="$PROJECT_DIR/disk.img"
MOUNT_DIR="/tmp/hackers-os-disk"
DISK_SIZE_MB=64

echo "=== Creating HackersOS disk image ==="

# Create empty disk image
echo "[1/4] Creating ${DISK_SIZE_MB}MB disk image..."
dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB 2>/dev/null

# Format as FAT32
echo "[2/4] Formatting as FAT32..."
if command -v mkfs.fat &>/dev/null; then
    mkfs.fat -F 32 -n HACKEROS "$DISK_IMG"
elif command -v newfs_msdos &>/dev/null; then
    # macOS
    newfs_msdos -F 32 -v HACKEROS "$DISK_IMG"
else
    echo "Error: No FAT32 formatter found (need mkfs.fat or newfs_msdos)"
    exit 1
fi

# Copy files using mtools (works without root/mount)
echo "[3/4] Copying files to disk image..."
if command -v mcopy &>/dev/null; then
    # Create directories
    mmd -i "$DISK_IMG" ::bin 2>/dev/null || true
    mmd -i "$DISK_IMG" ::etc 2>/dev/null || true

    # Copy apps if they exist
    if [ -d "$PROJECT_DIR/apps" ]; then
        for f in "$PROJECT_DIR/apps"/*; do
            if [ -f "$f" ] && [ -x "$f" ]; then
                echo "  Adding: $(basename "$f")"
                mcopy -i "$DISK_IMG" "$f" "::bin/$(basename "$f")"
            fi
        done
    fi

    # Create a welcome file
    echo "Welcome to HackersOS!" > /tmp/hackers-os-welcome.txt
    echo "Type 'help' for available commands." >> /tmp/hackers-os-welcome.txt
    echo "" >> /tmp/hackers-os-welcome.txt
    echo "Developed by TLDR;IT" >> /tmp/hackers-os-welcome.txt
    echo "https://www.tldr-it.com" >> /tmp/hackers-os-welcome.txt
    mcopy -i "$DISK_IMG" /tmp/hackers-os-welcome.txt ::welcome.txt
    rm /tmp/hackers-os-welcome.txt
else
    echo "Warning: mtools not installed. Disk image created but empty."
    echo "Install with: brew install mtools (macOS) or apt install mtools (Linux)"
fi

echo "[4/4] Done!"
echo ""
echo "Disk image: $DISK_IMG (${DISK_SIZE_MB}MB FAT32)"
echo "Run with:   make run-disk"
