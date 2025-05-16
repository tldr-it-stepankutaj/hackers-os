cmake_minimum_required(VERSION 3.10)
project(CppOS VERSION 0.1.0 LANGUAGES CXX ASM)

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

# Set compiler flags for 32-bit OS development
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -nostdlib -nostdinc -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -Wall -Wextra")
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
    message(FATAL_ERROR "NASM is required but not found! Please install it with 'brew install nasm' and restart CLion.")
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

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Collect source files
file(GLOB_RECURSE CXX_SOURCES "src/*.cpp")
file(GLOB_RECURSE ASM_SOURCES "src/*.asm")

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
    add_custom_target(debug
            DEPENDS ${USING_SIMPLIFIED_ISO} ? kernel.bin : iso
            COMMAND ${USING_SIMPLIFIED_ISO} ?
            ${QEMU} -kernel ${CMAKE_BINARY_DIR}/kernel.bin -s -S :
            ${QEMU} -cdrom ${CMAKE_BINARY_DIR}/cpp-os.iso -s -S
    )
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