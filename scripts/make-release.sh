#!/bin/bash
# Create release artifacts locally
# Usage: ./scripts/make-release.sh [version]
# Example: ./scripts/make-release.sh v0.1.0

set -e

VERSION="${1:-dev}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
RELEASE_DIR="$PROJECT_DIR/release"

cd "$PROJECT_DIR"

echo "========================================"
echo "  HackersOS Release Builder — $VERSION"
echo "========================================"
echo ""

# 1. Build kernel
echo "[1/5] Building kernel..."
make build 2>&1 | tail -3

KERNEL_SIZE=$(ls -lh build/kernel.bin | awk '{print $5}')
echo "  kernel.bin: $KERNEL_SIZE"

# 2. Build apps (optional, skip if deps not available)
echo "[2/5] Building companion apps..."
./scripts/build-apps.sh 2>&1 | tail -5 || echo "  (some apps skipped)"

# 3. Create disk image
echo "[3/5] Creating disk image..."
DISK="$RELEASE_DIR/hackers-os-disk.img"
mkdir -p "$RELEASE_DIR"
dd if=/dev/zero of="$DISK" bs=1M count=64 2>/dev/null

if command -v mkfs.fat &>/dev/null; then
    mkfs.fat -F 32 -n HACKEROS "$DISK" 2>/dev/null
elif command -v mformat &>/dev/null; then
    mformat -F -v HACKEROS -i "$DISK" :: 2>/dev/null
fi

if command -v mcopy &>/dev/null; then
    # Test programs
    if [ -d tests ]; then
        CROSS=$(which aarch64-unknown-linux-gnu-gcc 2>/dev/null || which aarch64-linux-gnu-gcc 2>/dev/null || echo "")
        if [ -n "$CROSS" ]; then
            cd tests
            $CROSS -static -nostdlib -o hello hello.S 2>/dev/null && mcopy -i "$DISK" hello ::hello
            $CROSS -static -nostdlib -o sysinfo sysinfo.S 2>/dev/null && mcopy -i "$DISK" sysinfo ::sysinfo
            cd "$PROJECT_DIR"
        fi
    fi

    # Companion apps
    for app in apps/*; do
        [ -f "$app" ] || continue
        name=$(basename "$app")
        mcopy -i "$DISK" "$app" "::$name" 2>/dev/null || true
    done

    # Welcome
    echo "Welcome to HackersOS $VERSION" > /tmp/hos-welcome.txt
    echo "Type 'help' for commands." >> /tmp/hos-welcome.txt
    echo "https://www.tldr-it.com" >> /tmp/hos-welcome.txt
    mcopy -i "$DISK" /tmp/hos-welcome.txt ::welcome.txt
fi

# 4. Create QEMU package
echo "[4/5] Creating QEMU package..."
QEMU_DIR="$RELEASE_DIR/hackers-os-qemu"
rm -rf "$QEMU_DIR"
mkdir -p "$QEMU_DIR"

cp build/kernel.bin "$QEMU_DIR/"
cp build/kernel.elf "$QEMU_DIR/"
cp "$DISK" "$QEMU_DIR/disk.img"

cat > "$QEMU_DIR/run.sh" << 'RUNEOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
KERNEL="$SCRIPT_DIR/kernel.bin"
ARGS=(-M virt -cpu cortex-a72 -m 256M -nographic -kernel "$KERNEL")

if [[ "$*" == *"--disk"* ]] && [ -f "$SCRIPT_DIR/disk.img" ]; then
  ARGS+=(-drive "file=$SCRIPT_DIR/disk.img,if=none,format=raw,id=hd0" -device virtio-blk-device,drive=hd0)
fi
if [[ "$*" == *"--net"* ]]; then
  ARGS+=(-device virtio-net-device,netdev=net0 -netdev user,id=net0)
fi
if [[ "$*" == *"--debug"* ]]; then
  ARGS+=(-s -S)
  echo "GDB server on :1234"
fi

exec qemu-system-aarch64 "${ARGS[@]}"
RUNEOF
chmod +x "$QEMU_DIR/run.sh"

cd "$RELEASE_DIR"
tar czf hackers-os-qemu.tar.gz -C "$RELEASE_DIR" hackers-os-qemu
cd "$PROJECT_DIR"

# 5. Create Raspberry Pi SD card package
echo "[5/5] Creating Raspberry Pi SD card package..."
RPI_DIR="$RELEASE_DIR/hackers-os-rpi-sdcard"
rm -rf "$RPI_DIR"
mkdir -p "$RPI_DIR"

cp build/kernel8.img "$RPI_DIR/"

cat > "$RPI_DIR/config.txt" << 'EOF'
# HackersOS Raspberry Pi Configuration
arm_64bit=1
kernel=kernel8.img
enable_uart=1
gpu_mem=16
dtoverlay=disable-bt
EOF

cat > "$RPI_DIR/README.txt" << 'EOF'
HackersOS - Raspberry Pi SD Card
=================================

1. Format SD card as FAT32
2. Copy ALL files from this archive to the SD card root
3. Download RPi firmware (bootcode.bin, start.elf, fixup.dat) from:
   https://github.com/raspberrypi/firmware/tree/master/boot
   and copy them to the SD card root
4. Connect USB-to-serial adapter:
   - GPIO 14 (pin 8)  = TX
   - GPIO 15 (pin 10) = RX
   - GND (pin 6)
5. Open serial terminal at 115200 baud (8N1)
6. Power on the Raspberry Pi

Supported: Raspberry Pi 3, 3B+, 4, 400, 5

https://www.tldr-it.com
EOF

# Try to download RPi firmware
if command -v curl &>/dev/null; then
    echo "  Downloading RPi firmware..."
    FIRMWARE_URL="https://github.com/raspberrypi/firmware/raw/master/boot"
    curl -sL "$FIRMWARE_URL/bootcode.bin" -o "$RPI_DIR/bootcode.bin" 2>/dev/null || true
    curl -sL "$FIRMWARE_URL/start.elf" -o "$RPI_DIR/start.elf" 2>/dev/null || true
    curl -sL "$FIRMWARE_URL/fixup.dat" -o "$RPI_DIR/fixup.dat" 2>/dev/null || true
fi

# Copy disk image for SD card too
[ -f "$DISK" ] && cp "$DISK" "$RPI_DIR/disk.img"

cd "$RPI_DIR"
zip -r "$RELEASE_DIR/hackers-os-rpi-sdcard.zip" . 2>/dev/null
cd "$PROJECT_DIR"

# Summary
echo ""
echo "========================================"
echo "  Release Artifacts ($VERSION)"
echo "========================================"
ls -lh "$RELEASE_DIR"/*.tar.gz "$RELEASE_DIR"/*.zip "$RELEASE_DIR"/*.img 2>/dev/null
echo ""
echo "  Kernel:     build/kernel.bin ($KERNEL_SIZE)"
echo "  RPi:        build/kernel8.img"
echo ""
echo "To create a GitHub release:"
echo "  git tag $VERSION && git push origin $VERSION"
