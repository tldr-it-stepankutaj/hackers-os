# Makefile for C++ OS

# Compiler and linker settings
CC = g++
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-exceptions -fno-rtti -Wall -Wextra -c
LDFLAGS = -m32 -T linker.ld -nostdlib -lgcc

# Source files
ASM_SOURCES = $(wildcard src/*.asm)
CPP_SOURCES = $(wildcard src/*.cpp)

# Object files
ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)
CPP_OBJECTS = $(CPP_SOURCES:.cpp=.o)
OBJECTS = $(ASM_OBJECTS) $(CPP_OBJECTS)

# Target kernel binary
KERNEL = kernel.bin

# ISO file
ISO = cpp-os.iso

# QEMU settings
QEMU = qemu-system-i386
QEMU_FLAGS = -cdrom $(ISO)

# Default target
all: $(ISO)

# Build kernel
$(KERNEL): $(OBJECTS)
	ld $(LDFLAGS) -o $@ $^

# Build assembly files
%.o: %.asm
	nasm -f elf32 $< -o $@

# Build C++ files
%.o: %.cpp
	$(CC) $(CFLAGS) -o $@ $

# Create ISO image
$(ISO): $(KERNEL)
	mkdir -p iso/boot
	cp $(KERNEL) iso/boot/
	grub-mkrescue -o $(ISO) iso

# Run in QEMU
run: $(ISO)
	$(QEMU) $(QEMU_FLAGS)

# Debug in QEMU with GDB
debug: $(ISO)
	$(QEMU) $(QEMU_FLAGS) -s -S &
	gdb -ex "target remote localhost:1234" -ex "symbol-file $(KERNEL)"

# Clean up
clean:
	rm -f $(OBJECTS) $(KERNEL) $(ISO)
	rm -rf iso/boot/$(KERNEL)

.PHONY: all run debug clean