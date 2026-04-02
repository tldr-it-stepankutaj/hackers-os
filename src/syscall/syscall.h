#pragma once
#include "../kernel/kernel.h"

// Syscall numbers
static constexpr u64 SYS_EXIT    = 0;
static constexpr u64 SYS_WRITE   = 1;
static constexpr u64 SYS_READ    = 2;
static constexpr u64 SYS_GETPID  = 3;
static constexpr u64 SYS_YIELD   = 4;
static constexpr u64 SYS_EXEC    = 5;
static constexpr u64 SYS_OPEN    = 6;
static constexpr u64 SYS_CLOSE   = 7;
static constexpr u64 SYS_READDIR = 8;

namespace Syscall {

void init();

} // namespace Syscall
