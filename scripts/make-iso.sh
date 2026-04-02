#!/bin/bash
# Create a bootable UEFI ISO for HackersOS (AArch64)
#
# Creates an ISO with an EFI System Partition containing
# the kernel and a GRUB bootloader (if available) or
# a minimal startup.nsh for UEFI Shell.
#
# Requirements: xorriso, mtools, dosfstools

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

# Clean
rm -rf "$ISO_DIR" "$EFI_IMG" "$OUTPUT"
mkdir -p "$ISO_DIR"

# Create EFI System Partition (FAT32 image)
echo "[1/2] Creating EFI System Partition..."
EFI_SIZE_KB=2880  # 2.88MB (standard floppy — always works with FAT12)
dd if=/dev/zero of="$EFI_IMG" bs=1024 count=$EFI_SIZE_KB 2>/dev/null

if command -v mkfs.fat &>/dev/null; then
    mkfs.fat "$EFI_IMG" 2>/dev/null
elif command -v mformat &>/dev/null; then
    mformat -i "$EFI_IMG" :: 2>/dev/null
else
    echo "Error: need mkfs.fat or mformat"
    exit 1
fi

# Populate EFI partition
echo "  Creating EFI directory structure..."
mmd -i "$EFI_IMG" ::EFI || true
mmd -i "$EFI_IMG" ::EFI/BOOT || true

# Try to create GRUB EFI binary
HAVE_GRUB=false
if command -v grub-mkstandalone &>/dev/null; then
    # Check if arm64-efi format is available
    grub-mkstandalone --format=arm64-efi --output=/dev/null --version 2>/dev/null && HAVE_GRUB=true || true
fi

if [ "$HAVE_GRUB" = true ]; then
    echo "  Using GRUB ARM64 EFI bootloader..."

    # Create GRUB config
    mkdir -p /tmp/grub-embed
    cat > /tmp/grub-embed/grub.cfg << 'EOF'
set timeout=3
set default=0

menuentry "HackersOS (AArch64)" {
    linux /kernel.bin
}
EOF

    grub-mkstandalone \
        --format=arm64-efi \
        --output=/tmp/BOOTAA64.EFI \
        --locales="" \
        --fonts="" \
        "boot/grub/grub.cfg=/tmp/grub-embed/grub.cfg"

    mcopy -i "$EFI_IMG" /tmp/BOOTAA64.EFI ::EFI/BOOT/BOOTAA64.EFI
    rm -f /tmp/BOOTAA64.EFI
    rm -rf /tmp/grub-embed
else
    echo "  GRUB not available for arm64-efi, creating UEFI Shell startup script..."
fi

# Always add kernel to EFI partition
echo "  Copying kernel ($KERNEL → EFI)..."
mcopy -i "$EFI_IMG" "$KERNEL" ::kernel.bin || { echo "Warning: mcopy kernel failed"; }

# Create startup.nsh (UEFI Shell autorun script — fallback if no GRUB)
echo "kernel.bin" > /tmp/startup.nsh
mcopy -i "$EFI_IMG" /tmp/startup.nsh ::startup.nsh || true
rm -f /tmp/startup.nsh

# Copy disk image if available
for img in "$PROJECT_DIR/hackers-os-disk.img" \
           "$PROJECT_DIR/release/hackers-os-disk.img" \
           "$PROJECT_DIR/disk.img"; do
    if [ -f "$img" ]; then
        echo "  Including disk image..."
        mcopy -i "$EFI_IMG" "$img" ::disk.img 2>/dev/null || true
        break
    fi
done

# Create ISO
echo "[2/2] Creating ISO image..."
cp "$EFI_IMG" "$ISO_DIR/efi.img"
# Also put kernel at ISO root for direct access
cp "$KERNEL" "$ISO_DIR/kernel.bin"
cp "$PROJECT_DIR/build/kernel8.img" "$ISO_DIR/kernel8.img" 2>/dev/null || true

if command -v xorriso &>/dev/null; then
    xorriso -as mkisofs \
        -R -J -V "HACKEROS" \
        -o "$OUTPUT" \
        -partition_offset 16 \
        -append_partition 2 0xef "$EFI_IMG" \
        -appended_part_as_gpt \
        -eltorito-alt-boot \
        -e --interval:appended_partition_2:all:: \
        -no-emul-boot \
        "$ISO_DIR" 2>/dev/null
else
    echo "Error: xorriso not found. Install: sudo apt install xorriso / brew install xorriso"
    exit 1
fi

echo ""
echo "=== ISO created ==="
ls -lh "$OUTPUT"
echo ""
echo "Boot in QEMU with UEFI firmware:"
echo "  qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -nographic \\"
echo "    -bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \\"
echo "    -cdrom build/hackers-os.iso"
echo ""
echo "Or write to USB/SD with:"
echo "  dd if=build/hackers-os.iso of=/dev/sdX bs=4M status=progress"
