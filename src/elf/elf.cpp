#include "elf.h"
#include "../fs/fat32.h"
#include "../mm/pages.h"
#include "../mm/mmu.h"
#include "../mm/heap.h"
#include "../process/process.h"
#include "../uart/uart.h"

static const u8 ELF_MAGIC[] = {0x7F, 'E', 'L', 'F'};

namespace ELF {

bool validate(const void *data, usize size) {
    if (size < sizeof(Elf64_Ehdr)) return false;

    const Elf64_Ehdr *ehdr = static_cast<const Elf64_Ehdr*>(data);

    if (memcmp(ehdr->e_ident, ELF_MAGIC, 4) != 0) return false;
    if (ehdr->e_ident[4] != 2) return false;    // 64-bit
    if (ehdr->e_ident[5] != 1) return false;    // Little endian
    if (ehdr->e_machine != EM_AARCH64) return false;
    if (ehdr->e_type != ET_EXEC) return false;

    return true;
}

u32 load_and_exec(const char *path, const char *name) {
    // Get file size
    u32 fsize = FAT32::file_size(path);
    if (fsize == 0) {
        UART::printf("ELF: file not found: %s\n", path);
        return 0;
    }

    // Allocate buffer and read file
    void *buf = Heap::kmalloc(fsize);
    if (!buf) {
        UART::puts("ELF: out of memory\n");
        return 0;
    }

    u32 bytes_read;
    if (!FAT32::read_file(path, buf, fsize, &bytes_read)) {
        UART::puts("ELF: read failed\n");
        Heap::kfree(buf);
        return 0;
    }

    // Validate
    if (!validate(buf, bytes_read)) {
        UART::puts("ELF: invalid ELF binary\n");
        Heap::kfree(buf);
        return 0;
    }

    const Elf64_Ehdr *ehdr = static_cast<const Elf64_Ehdr*>(buf);

    // Create user page table
    u64 pgd = MMU::create_user_page_table();
    if (!pgd) {
        UART::puts("ELF: cannot create page table\n");
        Heap::kfree(buf);
        return 0;
    }

    // Load PT_LOAD segments
    const u8 *file_data = static_cast<const u8*>(buf);
    for (u16 i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = reinterpret_cast<const Elf64_Phdr*>(
            file_data + ehdr->e_phoff + i * ehdr->e_phentsize
        );

        if (phdr->p_type != PT_LOAD) continue;

        // Allocate pages for this segment
        u64 vaddr = phdr->p_vaddr & ~(PAGE_SIZE - 1);
        u64 vend = (phdr->p_vaddr + phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        u64 flags = PTE_USER_RWX;  // Simplified: all segments RWX

        for (u64 va = vaddr; va < vend; va += PAGE_SIZE) {
            u64 page = Pages::alloc_page();
            if (!page) {
                UART::puts("ELF: out of pages\n");
                Heap::kfree(buf);
                return 0;
            }
            MMU::map_user_page(pgd, va, page, flags);

            // Copy file data into page
            u64 seg_offset = va - phdr->p_vaddr;
            if (seg_offset < phdr->p_filesz) {
                u64 copy_off = phdr->p_offset + seg_offset;
                u64 copy_len = PAGE_SIZE;
                if (seg_offset + copy_len > phdr->p_filesz) {
                    copy_len = phdr->p_filesz - seg_offset;
                }
                if (copy_off + copy_len <= bytes_read) {
                    memcpy(reinterpret_cast<void*>(page),
                           file_data + copy_off, copy_len);
                }
            }
        }
    }

    // Create process
    Process *proc = Process_::create_user(name, ehdr->e_entry, pgd);
    if (!proc) {
        UART::puts("ELF: cannot create process\n");
        Heap::kfree(buf);
        return 0;
    }

    Heap::kfree(buf);
    UART::printf("ELF: loaded '%s' (pid %u, entry %x)\n",
                 name, (u64)proc->pid, ehdr->e_entry);
    return proc->pid;
}

} // namespace ELF
