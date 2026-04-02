#pragma once
#include "../kernel/kernel.h"

static constexpr u32 FAT32_MAX_FILENAME = 128;
static constexpr u32 FAT32_SECTOR_SIZE = 512;

struct Fat32DirEntry {
    char name[FAT32_MAX_FILENAME];
    u32 size;
    u32 first_cluster;
    bool is_dir;
};

namespace FAT32 {

bool init();
bool list_dir(const char *path, Fat32DirEntry *entries, u32 max_entries, u32 *count);
bool read_file(const char *path, void *buf, u32 buf_size, u32 *bytes_read);
bool file_exists(const char *path);
u32 file_size(const char *path);

} // namespace FAT32
