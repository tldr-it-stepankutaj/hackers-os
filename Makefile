# Makefile for macOS M2
.PHONY: all build run clean

RUSTFLAGS := "-C link-arg=-fuse-ld=lld -C link-arg=-nostartfiles"
LLVM_PATH := $(shell brew --prefix llvm)

all: build run

build:
	RUSTFLAGS=$(RUSTFLAGS) cargo build --target x86_64-rust_os.json

run: build
	cargo bootimage
	qemu-system-x86_64 -drive format=raw,file=target/x86_64-rust_os/debug/bootimage-rust-os.bin

debug: build
	cargo bootimage
	qemu-system-x86_64 -drive format=raw,file=target/x86_64-rust_os/debug/bootimage-rust-os.bin -s -S &
	rust-gdb target/x86_64-rust_os/debug/rust-os -ex "target remote :1234"

clean:
	cargo clean