set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Try common cross-compiler prefixes
find_program(CMAKE_C_COMPILER NAMES
    aarch64-none-elf-gcc
    aarch64-elf-gcc
    aarch64-unknown-linux-gnu-gcc
    aarch64-linux-gnu-gcc
)
find_program(CMAKE_CXX_COMPILER NAMES
    aarch64-none-elf-g++
    aarch64-elf-g++
    aarch64-unknown-linux-gnu-g++
    aarch64-linux-gnu-g++
)
find_program(CMAKE_ASM_COMPILER NAMES
    aarch64-none-elf-gcc
    aarch64-elf-gcc
    aarch64-unknown-linux-gnu-gcc
    aarch64-linux-gnu-gcc
)
find_program(OBJCOPY NAMES
    aarch64-none-elf-objcopy
    aarch64-elf-objcopy
    aarch64-unknown-linux-gnu-objcopy
    aarch64-linux-gnu-objcopy
)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(COMMON_FLAGS "-ffreestanding -nostdlib -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -mgeneral-regs-only -Wall -Wextra -O2")

set(CMAKE_C_FLAGS_INIT "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS} -std=c++17")
set(CMAKE_ASM_FLAGS_INIT "-ffreestanding -nostdlib")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
