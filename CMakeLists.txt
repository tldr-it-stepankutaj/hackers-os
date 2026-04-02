cmake_minimum_required(VERSION 3.10)
project(HackersOS VERSION 0.1.0 LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Kernel sources
set(KERNEL_SOURCES
    src/boot/start.S
    src/boot/vectors.S
    src/process/context.S
    src/process/enter_user.S

    src/kernel/kernel.cpp
    src/kernel/panic.cpp
    src/lib/string.cpp

    src/uart/uart.cpp
    src/logo/logo.cpp

    src/interrupts/gic.cpp
    src/interrupts/exception.cpp

    src/timer/timer.cpp

    src/mm/pages.cpp
    src/mm/mmu.cpp
    src/mm/heap.cpp

    src/process/process.cpp
    src/process/scheduler.cpp

    src/syscall/syscall.cpp

    src/drivers/virtio_blk.cpp
    src/fs/fat32.cpp

    src/elf/elf.cpp
    src/shell/shell.cpp

    src/compat/linux_syscalls.cpp
    src/compat/net_syscalls.cpp

    src/net/virtio_net.cpp
    src/net/net.cpp
    src/net/tcp.cpp
    src/net/socket.cpp
    src/net/dhcp.cpp
    src/net/dns.cpp
)

# Include directories
include_directories(src)

# Kernel ELF target
add_executable(kernel.elf ${KERNEL_SOURCES})

# Linker script
set_target_properties(kernel.elf PROPERTIES
    LINK_DEPENDS "${CMAKE_SOURCE_DIR}/linker.ld"
)
target_link_options(kernel.elf PRIVATE
    -T ${CMAKE_SOURCE_DIR}/linker.ld
    -nostdlib
    -lgcc
)

# Find objcopy
find_program(OBJCOPY_BIN
    NAMES aarch64-none-elf-objcopy aarch64-elf-objcopy aarch64-linux-gnu-objcopy
)

# Create raw binary from ELF
if(OBJCOPY_BIN)
    add_custom_command(TARGET kernel.elf POST_BUILD
        COMMAND ${OBJCOPY_BIN} -O binary
            ${CMAKE_BINARY_DIR}/kernel.elf
            ${CMAKE_BINARY_DIR}/kernel.bin
        COMMENT "Creating kernel.bin (raw binary)"
    )

    # Also create kernel8.img for Raspberry Pi
    add_custom_command(TARGET kernel.elf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/kernel.bin
            ${CMAKE_BINARY_DIR}/kernel8.img
        COMMENT "Creating kernel8.img (Raspberry Pi)"
    )
endif()

# Find QEMU
find_program(QEMU qemu-system-aarch64)

if(QEMU)
    # Run in QEMU (no disk)
    add_custom_target(run
        DEPENDS kernel.elf
        COMMAND ${QEMU}
            -M virt
            -cpu cortex-a72
            -m 256M
            -nographic
            -kernel ${CMAKE_BINARY_DIR}/kernel.bin
            -serial mon:stdio
        COMMENT "Running HackersOS in QEMU"
    )

    # Run in QEMU with disk image
    add_custom_target(run-disk
        DEPENDS kernel.elf
        COMMAND ${QEMU}
            -M virt
            -cpu cortex-a72
            -m 256M
            -nographic
            -kernel ${CMAKE_BINARY_DIR}/kernel.bin
            -serial mon:stdio
            -drive file=${CMAKE_SOURCE_DIR}/disk.img,if=none,format=raw,id=hd0
            -device virtio-blk-device,drive=hd0
        COMMENT "Running HackersOS in QEMU with disk"
    )

    # Debug in QEMU (GDB)
    add_custom_target(debug
        DEPENDS kernel.elf
        COMMAND ${QEMU}
            -M virt
            -cpu cortex-a72
            -m 256M
            -nographic
            -kernel ${CMAKE_BINARY_DIR}/kernel.bin
            -serial mon:stdio
            -s -S
        COMMENT "Running HackersOS in QEMU (GDB debug mode, waiting for connection on :1234)"
    )
else()
    message(WARNING "qemu-system-aarch64 not found. Install with: brew install qemu")
endif()

message(STATUS "")
message(STATUS "=== HackersOS Build Configuration ===")
message(STATUS "  Target: AArch64 (ARM64)")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
if(QEMU)
    message(STATUS "  QEMU: ${QEMU}")
endif()
if(OBJCOPY_BIN)
    message(STATUS "  Objcopy: ${OBJCOPY_BIN}")
endif()
message(STATUS "")
message(STATUS "  Targets:")
message(STATUS "    kernel.elf  - Build the kernel")
if(QEMU)
    message(STATUS "    run         - Run in QEMU")
    message(STATUS "    run-disk    - Run in QEMU with disk.img")
    message(STATUS "    debug       - Run in QEMU with GDB server")
endif()
message(STATUS "======================================")
