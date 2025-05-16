# Makefile
.PHONY: all build run clean

all: build run

build:
	cargo build --target x86_64-rust_os.json

iso: build
	mkdir -p target/isofiles/boot/grub
	cp target/x86_64-rust_os/debug/rust-os target/isofiles/boot/kernel.bin
	cp grub.cfg target/isofiles/boot/grub
	grub-mkrescue -o target/rust-os.iso target/isofiles

run: iso
	qemu-system-x86_64 -cdrom target/rust-os.iso

clean:
	cargo clean
	rm -rf target/isofiles target/rust-os.iso