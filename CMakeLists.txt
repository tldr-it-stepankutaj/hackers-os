cmake_minimum_required(VERSION 3.10)
project(CppOS VERSION 0.1.0 LANGUAGES CXX ASM)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set macOS specific settings
if(APPLE)
    # On macOS (especially Apple Silicon), we need to specify paths for tools
    # that may be installed via Homebrew
    set(HOMEBREW_PREFIX "/opt/homebrew" CACHE STRING "Homebrew installation prefix")

    # Add Homebrew paths to CMake's search paths
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
    list(APPEND CMAKE_PROGRAM_PATH "${HOMEBREW_PREFIX}/bin")

    # Check for ARM Mac (Apple Silicon)
    execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE ARCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(ARCH STREQUAL "arm64")
        message(STATUS "Detected Apple Silicon Mac")
        set(NASM_PATH "${HOMEBREW_PREFIX}/bin/nasm" CACHE FILEPATH "Path to NASM")
    else()
        message(STATUS "Detected Intel Mac")
        set(NASM_PATH "${HOMEBREW_PREFIX}/bin/nasm" CACHE FILEPATH "Path to NASM")
    endif()
else()
    # For non-macOS systems
    set(NASM_PATH "nasm" CACHE FILEPATH "Path to NASM")
endif()

# Set compiler flags for freestanding OS development
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -Wall -Wextra")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -f elf32")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32 -nostdlib -lgcc -T ${CMAKE_SOURCE_DIR}/linker.ld")

# Find required programs using custom paths
find_program(NASM nasm PATHS ${NASM_PATH} NO_DEFAULT_PATH)
find_program(NASM nasm) # Fallback to standard paths

# For macOS, GRUB and QEMU need to be installed via Homebrew
if(APPLE)
    find_program(GRUB_MKRESCUE grub-mkrescue PATHS "${HOMEBREW_PREFIX}/bin" NO_DEFAULT_PATH)
    find_program(QEMU qemu-system-i386 PATHS "${HOMEBREW_PREFIX}/bin" NO_DEFAULT_PATH)
else()
    find_program(GRUB_MKRESCUE grub-mkrescue)
    find_program(QEMU qemu-system-i386)
endif()

# Print path information for debugging
message(STATUS "NASM path: ${NASM}")
message(STATUS "GRUB-MKRESCUE path: ${GRUB_MKRESCUE}")
message(STATUS "QEMU path: ${QEMU}")

# Check if NASM is available
if(NOT NASM)
    message(FATAL_ERROR "NASM is required but not found! Please install it with 'brew install nasm' and restart CMake.")
endif()

# For macOS, we'll simulate GRUB and ISO creation - since we're running under emulation anyway
if(NOT GRUB_MKRESCUE AND APPLE)
    message(WARNING "grub-mkrescue not found on macOS. Using simplified ISO creation.")
    set(USING_SIMPLIFIED_ISO TRUE)
else()
    if(NOT GRUB_MKRESCUE)
        message(WARNING "grub-mkrescue not found. ISO creation will be disabled.")
    endif()
    set(USING_SIMPLIFIED_ISO FALSE)
endif()

if(NOT QEMU)
    message(WARNING "QEMU not found. You won't be able to run the OS with 'make run'")
endif()

# Set custom command for NASM
set(CMAKE_ASM_COMPILE_OBJECT "${NASM} <SOURCE> -o <OBJECT> -f elf32")

# Include directories - add custom include dir for our own headers
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/include)

# Check if src/include directory exists, create if not
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/src/include)
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/include)
endif()

# Create custom stdint.h and stddef.h if they don't exist
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/src/include/stdint.h)
    file(WRITE ${CMAKE_SOURCE_DIR}/src/include/stdint.h [[
#ifndef _STDINT_H
#define _STDINT_H

// Custom integer types
typedef signed char         int8_t;
typedef short               int16_t;
typedef int                 int32_t;
typedef long long           int64_t;

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef int8_t              int_least8_t;
typedef int16_t             int_least16_t;
typedef int32_t             int_least32_t;
typedef int64_t             int_least64_t;

typedef uint8_t             uint_least8_t;
typedef uint16_t            uint_least16_t;
typedef uint32_t            uint_least32_t;
typedef uint64_t            uint_least64_t;

typedef int8_t              int_fast8_t;
typedef int16_t             int_fast16_t;
typedef int32_t             int_fast32_t;
typedef int64_t             int_fast64_t;

typedef uint8_t             uint_fast8_t;
typedef uint16_t            uint_fast16_t;
typedef uint32_t            uint_fast32_t;
typedef uint64_t            uint_fast64_t;

typedef long long           intmax_t;
typedef unsigned long long  uintmax_t;

// Pointer types
typedef int32_t             intptr_t;
typedef uint32_t            uintptr_t;

// Limits
#define INT8_MIN            (-128)
#define INT16_MIN           (-32768)
#define INT32_MIN           (-2147483648)
#define INT64_MIN           (-9223372036854775808LL)

#define INT8_MAX            127
#define INT16_MAX           32767
#define INT32_MAX           2147483647
#define INT64_MAX           9223372036854775807LL

#define UINT8_MAX           255
#define UINT16_MAX          65535
#define UINT32_MAX          4294967295U
#define UINT64_MAX          18446744073709551615ULL

// Size type
typedef uint32_t            size_t;
typedef int32_t             ptrdiff_t;

#endif // _STDINT_H
]])
endif()

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/src/include/stddef.h)
    file(WRITE ${CMAKE_SOURCE_DIR}/src/include/stddef.h [[
#ifndef _STDDEF_H
#define _STDDEF_H

// Include our custom stdint.h for size_t
#include <stdint.h>

// Define NULL pointer
#define NULL ((void*)0)

// Define offsetof macro
#define offsetof(type, member) __builtin_offsetof(type, member)

#endif // _STDDEF_H
]])
endif()

# Collect source files
file(GLOB_RECURSE CXX_SOURCES "src/*.cpp")
file(GLOB_RECURSE ASM_SOURCES "src/*.asm")

# Special handling for empty source directories
if(NOT CXX_SOURCES AND NOT ASM_SOURCES)
    message(STATUS "No source files found. Creating placeholder files.")

    # Create directory structure if it doesn't exist
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src)

    # Create basic boot.asm
    file(WRITE ${CMAKE_SOURCE_DIR}/src/boot.asm [[
; boot.asm - Basic multiboot entry point
bits 32
section .multiboot
    align 4
    ; Multiboot header format
    dd 0x1BADB002               ; Magic number
    dd 0x00000003               ; Flags: align modules to 4KB boundaries, provide memory map
    dd -(0x1BADB002 + 0x00000003) ; Checksum

section .text
global _start
extern kernel_main

_start:
    ; Set up the stack
    mov esp, stack_top

    ; Pass Multiboot info to kernel
    push ebx

    ; Call the kernel
    call kernel_main

    ; If the kernel returns, just hang
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:
]])

    # Create basic kernel.cpp
    file(WRITE ${CMAKE_SOURCE_DIR}/src/kernel.cpp [[
// kernel.cpp - Basic kernel entry point
#include "kernel.h"

// Write a character to the VGA buffer
void putchar(char c, u8 color, u16* buffer, int x, int y) {
    const int index = y * 80 + x;
    buffer[index] = ((u16)color << 8) | c;
}

// Write a string to the VGA buffer
void write_string(const char* str, u8 color, u16* buffer, int x, int y) {
    for (int i = 0; str[i] != '\0'; i++) {
        putchar(str[i], color, buffer, x + i, y);
    }
}

extern "C" void kernel_main(u32 multiboot_magic) {
    // VGA buffer address
    u16* vga_buffer = (u16*)0xB8000;

    // Clear screen
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            putchar(' ', 0x07, vga_buffer, x, y);
        }
    }

    // Write a message
    write_string("Hello from CppOS!", 0x0F, vga_buffer, 30, 12);

    // Halt the system
    while (true) {
        asm volatile("hlt");
    }
}
]])

    # Create kernel.h
    file(WRITE ${CMAKE_SOURCE_DIR}/src/kernel.h [[
// kernel.h - Basic kernel definitions
#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

// Define useful aliases
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

#endif // KERNEL_H
]])

    # Create basic linker.ld if it doesn't exist
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/linker.ld)
        file(WRITE ${CMAKE_SOURCE_DIR}/linker.ld [[
/* linker.ld - Basic linker script */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
]])
    endif()

    # Create GRUB config directory
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/iso/boot/grub)

    # Create grub.cfg
    file(WRITE ${CMAKE_SOURCE_DIR}/iso/boot/grub/grub.cfg [[
set timeout=0
set default=0

menuentry "CppOS" {
    multiboot /boot/kernel.bin
    boot
}
]])

    # Update source file lists
    file(GLOB_RECURSE CXX_SOURCES "src/*.cpp")
    file(GLOB_RECURSE ASM_SOURCES "src/*.asm")
endif()

# Add executable target
add_executable(kernel.bin ${CXX_SOURCES} ${ASM_SOURCES})
set_target_properties(kernel.bin PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker.ld)

# Create boot directory structure for ISO
add_custom_command(TARGET kernel.bin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/iso/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/kernel.bin ${CMAKE_BINARY_DIR}/iso/boot/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/iso/boot/grub/grub.cfg ${CMAKE_BINARY_DIR}/iso/boot/grub/
)

# Create ISO image (conditionally based on available tools)
if(GRUB_MKRESCUE OR USING_SIMPLIFIED_ISO)
    if(USING_SIMPLIFIED_ISO)
        # On macOS without GRUB, create a simple disk image
        add_custom_target(iso
                DEPENDS kernel.bin
                COMMAND ${CMAKE_COMMAND} -E echo "Creating simplified bootable image for macOS"
                COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/kernel.bin ${CMAKE_BINARY_DIR}/cpp-os.bin
        )
    else()
        # Use standard GRUB method
        add_custom_target(iso
                DEPENDS kernel.bin
                COMMAND ${GRUB_MKRESCUE} -o ${CMAKE_BINARY_DIR}/cpp-os.iso ${CMAKE_BINARY_DIR}/iso
        )
    endif()
endif()

# Run in QEMU (conditionally)
if(QEMU)
    if(USING_SIMPLIFIED_ISO)
        # Direct kernel boot for macOS
        add_custom_target(run
                DEPENDS kernel.bin
                COMMAND ${QEMU} -kernel ${CMAKE_BINARY_DIR}/kernel.bin
        )
    else()
        # Standard ISO boot
        add_custom_target(run
                DEPENDS iso
                COMMAND ${QEMU} -cdrom ${CMAKE_BINARY_DIR}/cpp-os.iso
        )
    endif()

    # Debug in QEMU with GDB server
    if(USING_SIMPLIFIED_ISO)
        add_custom_target(debug
                DEPENDS kernel.bin
                COMMAND ${QEMU} -kernel ${CMAKE_BINARY_DIR}/kernel.bin -s -S
        )
    else()
        add_custom_target(debug
                DEPENDS iso
                COMMAND ${QEMU} -cdrom ${CMAKE_BINARY_DIR}/cpp-os.iso -s -S
        )
    endif()
endif()

# Clean up generated files
add_custom_target(clean_all
        COMMAND ${CMAKE_MAKE_PROGRAM} clean
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/iso
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/cpp-os.iso
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/cpp-os.bin
)

# Print status message
message(STATUS "Configuration complete. Available targets:")
message(STATUS "  kernel.bin - Build the kernel")
if(GRUB_MKRESCUE OR USING_SIMPLIFIED_ISO)
    message(STATUS "  iso - Build the bootable image")
endif()
if(QEMU)
    message(STATUS "  run - Run the OS in QEMU")
    message(STATUS "  debug - Run the OS in QEMU with GDB server")
endif()
message(STATUS "  clean_all - Clean all generated files")