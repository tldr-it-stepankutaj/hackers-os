#pragma once
#include "../kernel/kernel.h"

// ELF64 header
struct Elf64_Ehdr {
    u8  e_ident[16];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} __attribute__((packed));

// Program header
struct Elf64_Phdr {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
} __attribute__((packed));

// Constants
static constexpr u16 EM_AARCH64 = 183;
static constexpr u16 ET_EXEC = 2;
static constexpr u32 PT_LOAD = 1;
static constexpr u32 PF_X = 1;
static constexpr u32 PF_W = 2;
static constexpr u32 PF_R = 4;

namespace ELF {

// Load and execute an ELF binary from the filesystem
// Returns PID of new process, or 0 on failure
u32 load_and_exec(const char *path, const char *name);

// Validate an ELF header in memory
bool validate(const void *data, usize size);

} // namespace ELF
