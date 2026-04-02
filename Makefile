# HackersOS - AArch64 Bare-Metal Kernel
# Convenience wrapper around CMake

BUILD_DIR = build
TOOLCHAIN = aarch64-toolchain.cmake

.PHONY: all build run run-disk debug clean disk-image install-deps

all: build

build:
	cmake -B $(BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel

run: build
	cmake --build $(BUILD_DIR) --target run

run-disk: build disk-image
	cmake --build $(BUILD_DIR) --target run-disk

debug: build
	cmake --build $(BUILD_DIR) --target debug

clean:
	rm -rf $(BUILD_DIR)

# Create a FAT32 disk image with apps
disk-image:
	./scripts/make-disk-image.sh

# Install cross-compiler and QEMU on macOS
install-deps:
	@echo "Installing AArch64 cross-compiler and QEMU..."
	brew install --cask gcc-aarch64-embedded || brew install aarch64-elf-gcc 2>/dev/null || true
	@echo ""
	@echo "If the above failed, install manually:"
	@echo "  Option 1: brew tap messense/macos-cross-toolchains && brew install aarch64-unknown-linux-gnu"
	@echo "  Option 2: Download from https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
	@echo ""
	brew install qemu
	@echo ""
	@echo "Verify installation:"
	@which aarch64-none-elf-gcc || which aarch64-elf-gcc || which aarch64-linux-gnu-gcc || echo "  [!] No cross-compiler found"
	@which qemu-system-aarch64 || echo "  [!] QEMU not found"
