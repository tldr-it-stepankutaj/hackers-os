#!/bin/bash
# Create a bootable UEFI ISO for HackersOS (AArch64)
# Uses GRUB ARM64 EFI as bootloader
#
# Requirements: xorriso, grub (arm64-efi), mtools, dosfstools

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ISO_DIR="$PROJECT_DIR/build/iso-root"
KERNEL="$PROJECT_DIR/build/kernel.bin"
EFI_IMG="$PROJECT_DIR/build/efi.img"
OUTPUT="$PROJECT_DIR/build/hackers-os.iso"

if [ ! -f "$KERNEL" ]; then
    echo "Error: kernel.bin not found. Run 'make build' first."
    exit 1
fi

echo "=== Creating bootable ISO ==="

# Clean and prepare
rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/grub" "$ISO_DIR/EFI/BOOT"

# Copy kernel
cp "$KERNEL" "$ISO_DIR/boot/kernel.bin"

# Copy disk image if available
if [ -f "$PROJECT_DIR/hackers-os-disk.img" ]; then
    cp "$PROJECT_DIR/hackers-os-disk.img" "$ISO_DIR/boot/disk.img"
elif [ -f "$PROJECT_DIR/release/hackers-os-disk.img" ]; then
    cp "$PROJECT_DIR/release/hackers-os-disk.img" "$ISO_DIR/boot/disk.img"
fi

# Create GRUB config
cat > "$ISO_DIR/boot/grub/grub.cfg" << 'EOF'
set timeout=5
set default=0

menuentry "HackersOS v0.1 (AArch64)" {
    echo "Loading HackersOS kernel..."
    linux /boot/kernel.bin
    boot
}

menuentry "HackersOS v0.1 (serial console)" {
    echo "Loading HackersOS kernel (serial)..."
    linux /boot/kernel.bin console=ttyAMA0
    boot
}
EOF

# Create EFI boot image
# This is a FAT32 image containing GRUB EFI binary
EFI_SIZE=4  # MB

echo "[1/3] Creating EFI boot image..."

# Try to find or create GRUB EFI binary
GRUB_EFI=""

# Option 1: grub-mkstandalone (best — creates self-contained EFI binary)
if command -v grub-mkstandalone &>/dev/null; then
    grub-mkstandalone \
        --format=arm64-efi \
        --output="$ISO_DIR/EFI/BOOT/BOOTAA64.EFI" \
        --locales="" \
        --fonts="" \
        "boot/grub/grub.cfg=$ISO_DIR/boot/grub/grub.cfg" 2>/dev/null
    GRUB_EFI="$ISO_DIR/EFI/BOOT/BOOTAA64.EFI"
fi

# Option 2: Copy pre-built GRUB EFI
if [ -z "$GRUB_EFI" ] || [ ! -f "$GRUB_EFI" ]; then
    # Look for system GRUB EFI binary
    for path in \
        /usr/lib/grub/arm64-efi/monolithic/grubaa64.efi \
        /usr/share/grub/arm64-efi/monolithic/grubaa64.efi \
        /boot/efi/EFI/ubuntu/grubaa64.efi \
        /usr/lib/grub/arm64-efi; do
        if [ -f "$path" ]; then
            cp "$path" "$ISO_DIR/EFI/BOOT/BOOTAA64.EFI"
            GRUB_EFI="$ISO_DIR/EFI/BOOT/BOOTAA64.EFI"
            break
        fi
    done
fi

# Option 3: Use grub-mkimage to build a minimal EFI binary
if [ -z "$GRUB_EFI" ] || [ ! -f "$GRUB_EFI" ]; then
    if command -v grub-mkimage &>/dev/null && [ -d /usr/lib/grub/arm64-efi ]; then
        grub-mkimage \
            --format=arm64-efi \
            --output="$ISO_DIR/EFI/BOOT/BOOTAA64.EFI" \
            --prefix="/boot/grub" \
            part_gpt part_msdos fat iso9660 normal boot linux \
            configfile loopback chain efifwsetup efi_gop \
            ls search search_label search_fs_uuid search_fs_file 2>/dev/null
        GRUB_EFI="$ISO_DIR/EFI/BOOT/BOOTAA64.EFI"
    fi
fi

if [ -z "$GRUB_EFI" ] || [ ! -f "$GRUB_EFI" ]; then
    echo "Warning: Could not create GRUB EFI binary."
    echo "Install grub-efi-arm64-bin: sudo apt install grub-efi-arm64-bin"
    echo "Creating ISO without EFI boot (kernel-only)..."
fi

# Create FAT32 EFI System Partition image
echo "[2/3] Creating EFI System Partition..."
dd if=/dev/zero of="$EFI_IMG" bs=1M count=$EFI_SIZE 2>/dev/null

if command -v mkfs.fat &>/dev/null; then
    mkfs.fat -F 12 "$EFI_IMG" 2>/dev/null
elif command -v mformat &>/dev/null; then
    mformat -i "$EFI_IMG" -F ::
fi

if command -v mcopy &>/dev/null; then
    mmd -i "$EFI_IMG" ::EFI 2>/dev/null || true
    mmd -i "$EFI_IMG" ::EFI/BOOT 2>/dev/null || true
    if [ -f "$ISO_DIR/EFI/BOOT/BOOTAA64.EFI" ]; then
        mcopy -i "$EFI_IMG" "$ISO_DIR/EFI/BOOT/BOOTAA64.EFI" ::EFI/BOOT/BOOTAA64.EFI
    fi
    mmd -i "$EFI_IMG" ::boot 2>/dev/null || true
    mmd -i "$EFI_IMG" ::boot/grub 2>/dev/null || true
    mcopy -i "$EFI_IMG" "$ISO_DIR/boot/grub/grub.cfg" ::boot/grub/grub.cfg
    mcopy -i "$EFI_IMG" "$KERNEL" ::boot/kernel.bin
fi

# Create ISO with xorriso
echo "[3/3] Creating ISO image..."
if command -v xorriso &>/dev/null; then
    xorriso -as mkisofs \
        -R -J \
        -o "$OUTPUT" \
        -partition_offset 16 \
        -append_partition 2 0xef "$EFI_IMG" \
        -appended_part_as_gpt \
        -eltorito-alt-boot \
        -e --interval:appended_partition_2:all:: \
        -no-emul-boot \
        -isohybrid-gpt-basdat \
        "$ISO_DIR" 2>/dev/null

    echo ""
    echo "=== ISO created ==="
    ls -lh "$OUTPUT"
    echo ""
    echo "Boot with QEMU:"
    echo "  qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -nographic \\"
    echo "    -bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \\"
    echo "    -cdrom $OUTPUT"
elif command -v genisoimage &>/dev/null; then
    genisoimage -R -J -o "$OUTPUT" "$ISO_DIR" 2>/dev/null
    echo "ISO created (without EFI boot): $OUTPUT"
else
    echo "Error: xorriso or genisoimage not found"
    echo "Install: sudo apt install xorriso  OR  brew install xorriso"
    exit 1
fi
