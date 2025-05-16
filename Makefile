build:
	cargo +nightly build -Z build-std=core --target x86_64-hackers_os.json

image:
	cargo +nightly bootimage

run:
	qemu-system-x86_64 -drive format=raw,file=target/x86_64-hackers_os/debug/bootimage-hackers_os.bin