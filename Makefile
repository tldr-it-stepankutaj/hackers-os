# HackersOS - AArch64 Bare-Metal Kernel
# Convenience wrapper around CMake

BUILD_DIR = build
TOOLCHAIN = aarch64-toolchain.cmake
VERSION ?= dev

.PHONY: all build run run-disk debug clean disk-image apps release install-deps

all: build

build:
	cmake -B $(BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel

run: build
	cmake --build $(BUILD_DIR) --target run

run-disk: build apps disk-image
	cmake --build $(BUILD_DIR) --target run-disk

debug: build
	cmake --build $(BUILD_DIR) --target debug

clean:
	rm -rf $(BUILD_DIR) release/

# Cross-compile companion apps (network-scanner, dnsutils, jwt_inspector)
apps:
	./scripts/build-apps.sh

# Create a FAT32 disk image with apps
disk-image:
	./scripts/make-disk-image.sh

# Build release artifacts (QEMU tar.gz, RPi SD card ZIP, disk image)
release:
	./scripts/make-release.sh $(VERSION)

# Install cross-compiler and QEMU on macOS
install-deps:
	@echo "Installing AArch64 cross-compiler and QEMU..."
	brew tap messense/macos-cross-toolchains 2>/dev/null || true
	brew install aarch64-unknown-linux-gnu 2>/dev/null || true
	brew install qemu mtools dosfstools
	@echo ""
	@echo "Verify installation:"
	@which aarch64-unknown-linux-gnu-gcc || which aarch64-linux-gnu-gcc || echo "  [!] No cross-compiler found"
	@which qemu-system-aarch64 || echo "  [!] QEMU not found"
