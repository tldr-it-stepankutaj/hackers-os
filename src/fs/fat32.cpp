#include "fat32.h"
#include "../drivers/virtio_blk.h"
#include "../mm/heap.h"
#include "../uart/uart.h"

// FAT32 BPB (BIOS Parameter Block)
struct BPB {
    u8  jmp[3];
    u8  oem[8];
    u16 bytes_per_sector;
    u8  sectors_per_cluster;
    u16 reserved_sectors;
    u8  num_fats;
    u16 root_entry_count;   // 0 for FAT32
    u16 total_sectors_16;
    u8  media_type;
    u16 fat_size_16;        // 0 for FAT32
    u16 sectors_per_track;
    u16 num_heads;
    u32 hidden_sectors;
    u32 total_sectors_32;
    u32 fat_size_32;
    u16 ext_flags;
    u16 fs_version;
    u32 root_cluster;
    u16 fs_info;
    u16 backup_boot;
    u8  reserved[12];
    u8  drive_number;
    u8  reserved1;
    u8  boot_sig;
    u32 volume_id;
    u8  volume_label[11];
    u8  fs_type[8];
} __attribute__((packed));

// Directory entry (32 bytes)
struct DirEntry {
    u8  name[11];
    u8  attr;
    u8  nt_reserved;
    u8  create_time_tenth;
    u16 create_time;
    u16 create_date;
    u16 access_date;
    u16 first_cluster_hi;
    u16 write_time;
    u16 write_date;
    u16 first_cluster_lo;
    u32 file_size;
} __attribute__((packed));

static constexpr u8 ATTR_READ_ONLY = 0x01;
static constexpr u8 ATTR_HIDDEN    = 0x02;
static constexpr u8 ATTR_SYSTEM    = 0x04;
static constexpr u8 ATTR_VOLUME_ID = 0x08;
static constexpr u8 ATTR_DIRECTORY = 0x10;
static constexpr u8 ATTR_ARCHIVE   = 0x20;
static constexpr u8 ATTR_LFN       = 0x0F;

static bool mounted = false;
static u32 sectors_per_cluster = 0;
static u32 fat_start_sector = 0;
static u32 data_start_sector = 0;
static u32 root_cluster = 0;
static u32 bytes_per_sector = 512;

static u8 sector_buf[512] __attribute__((aligned(16)));

static u32 cluster_to_sector(u32 cluster) {
    return data_start_sector + (cluster - 2) * sectors_per_cluster;
}

static u32 next_cluster(u32 cluster) {
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_start_sector + (fat_offset / bytes_per_sector);
    u32 entry_offset = fat_offset % bytes_per_sector;

    if (!VirtioBlk::read_sector(fat_sector, sector_buf)) return 0x0FFFFFF8;

    u32 val = *reinterpret_cast<u32*>(&sector_buf[entry_offset]);
    return val & 0x0FFFFFFF;
}

static bool is_end_cluster(u32 cluster) {
    return cluster >= 0x0FFFFFF8;
}

// Convert 8.3 name to normal string
static void fat_name_to_string(const u8 *fat_name, char *out) {
    int i = 0;
    // Name part (8 chars)
    for (int j = 0; j < 8 && fat_name[j] != ' '; j++) {
        out[i++] = (fat_name[j] >= 'A' && fat_name[j] <= 'Z')
                   ? fat_name[j] + 32 : fat_name[j];
    }
    // Extension (3 chars)
    if (fat_name[8] != ' ') {
        out[i++] = '.';
        for (int j = 8; j < 11 && fat_name[j] != ' '; j++) {
            out[i++] = (fat_name[j] >= 'A' && fat_name[j] <= 'Z')
                       ? fat_name[j] + 32 : fat_name[j];
        }
    }
    out[i] = '\0';
}

// Compare path component to 8.3 name
static bool name_matches(const u8 *fat_name, const char *name) {
    char converted[FAT32_MAX_FILENAME];
    fat_name_to_string(fat_name, converted);
    return strcmp(converted, name) == 0;
}

// Find entry in cluster chain
static bool find_in_dir(u32 dir_cluster, const char *name, DirEntry *result) {
    u32 cluster = dir_cluster;

    while (!is_end_cluster(cluster)) {
        u32 sector = cluster_to_sector(cluster);
        for (u32 s = 0; s < sectors_per_cluster; s++) {
            if (!VirtioBlk::read_sector(sector + s, sector_buf)) return false;

            DirEntry *entries = reinterpret_cast<DirEntry*>(sector_buf);
            u32 entries_per_sector = bytes_per_sector / sizeof(DirEntry);

            for (u32 e = 0; e < entries_per_sector; e++) {
                if (entries[e].name[0] == 0x00) return false;  // End of dir
                if (entries[e].name[0] == 0xE5) continue;      // Deleted
                if (entries[e].attr == ATTR_LFN) continue;     // Skip LFN
                if (entries[e].attr & ATTR_VOLUME_ID) continue;

                if (name_matches(entries[e].name, name)) {
                    *result = entries[e];
                    return true;
                }
            }
        }
        cluster = next_cluster(cluster);
    }
    return false;
}

// Resolve path to directory entry
static bool resolve_path(const char *path, DirEntry *result) {
    u32 current_cluster = root_cluster;

    // Skip leading /
    if (*path == '/') path++;
    if (*path == '\0') {
        // Root directory
        result->first_cluster_hi = (root_cluster >> 16) & 0xFFFF;
        result->first_cluster_lo = root_cluster & 0xFFFF;
        result->attr = ATTR_DIRECTORY;
        result->file_size = 0;
        return true;
    }

    char component[FAT32_MAX_FILENAME];
    while (*path) {
        int i = 0;
        while (*path && *path != '/') {
            if (i < FAT32_MAX_FILENAME - 1) component[i++] = *path;
            path++;
        }
        component[i] = '\0';
        if (*path == '/') path++;

        if (!find_in_dir(current_cluster, component, result)) return false;

        if (*path) {
            // More components — this must be a directory
            if (!(result->attr & ATTR_DIRECTORY)) return false;
            current_cluster = (static_cast<u32>(result->first_cluster_hi) << 16) |
                              result->first_cluster_lo;
        }
    }
    return true;
}

namespace FAT32 {

bool init() {
    if (!VirtioBlk::read_sector(0, sector_buf)) {
        UART::puts("  [FAIL] Cannot read boot sector\n");
        return false;
    }

    BPB *bpb = reinterpret_cast<BPB*>(sector_buf);

    // Validate
    if (bpb->bytes_per_sector != 512) {
        UART::puts("  [FAIL] Unsupported sector size\n");
        return false;
    }

    bytes_per_sector = bpb->bytes_per_sector;
    sectors_per_cluster = bpb->sectors_per_cluster;
    fat_start_sector = bpb->reserved_sectors;
    data_start_sector = fat_start_sector + bpb->num_fats * bpb->fat_size_32;
    root_cluster = bpb->root_cluster;
    mounted = true;

    UART::printf("  [ok] FAT32: %u sectors/cluster, root cluster %u\n",
                 (u64)sectors_per_cluster, (u64)root_cluster);
    return true;
}

bool list_dir(const char *path, Fat32DirEntry *entries, u32 max_entries, u32 *count) {
    if (!mounted) return false;

    DirEntry de;
    u32 dir_cluster;

    if (path[0] == '/' && path[1] == '\0') {
        dir_cluster = root_cluster;
    } else {
        if (!resolve_path(path, &de)) return false;
        if (!(de.attr & ATTR_DIRECTORY)) return false;
        dir_cluster = (static_cast<u32>(de.first_cluster_hi) << 16) | de.first_cluster_lo;
    }

    *count = 0;
    u32 cluster = dir_cluster;

    while (!is_end_cluster(cluster) && *count < max_entries) {
        u32 sector = cluster_to_sector(cluster);
        for (u32 s = 0; s < sectors_per_cluster && *count < max_entries; s++) {
            if (!VirtioBlk::read_sector(sector + s, sector_buf)) return false;

            DirEntry *raw = reinterpret_cast<DirEntry*>(sector_buf);
            u32 entries_per = bytes_per_sector / sizeof(DirEntry);

            for (u32 e = 0; e < entries_per && *count < max_entries; e++) {
                if (raw[e].name[0] == 0x00) return true;
                if (raw[e].name[0] == 0xE5) continue;
                if (raw[e].attr == ATTR_LFN) continue;
                if (raw[e].attr & ATTR_VOLUME_ID) continue;

                Fat32DirEntry &out = entries[*count];
                fat_name_to_string(raw[e].name, out.name);
                out.size = raw[e].file_size;
                out.first_cluster = (static_cast<u32>(raw[e].first_cluster_hi) << 16) |
                                    raw[e].first_cluster_lo;
                out.is_dir = (raw[e].attr & ATTR_DIRECTORY) != 0;
                (*count)++;
            }
        }
        cluster = next_cluster(cluster);
    }
    return true;
}

bool read_file(const char *path, void *buf, u32 buf_size, u32 *bytes_read) {
    if (!mounted) return false;

    DirEntry de;
    if (!resolve_path(path, &de)) return false;
    if (de.attr & ATTR_DIRECTORY) return false;

    u32 fsize = de.file_size;
    if (fsize > buf_size) fsize = buf_size;

    u32 cluster = (static_cast<u32>(de.first_cluster_hi) << 16) | de.first_cluster_lo;
    u32 remaining = fsize;
    u8 *dst = static_cast<u8*>(buf);

    while (remaining > 0 && !is_end_cluster(cluster)) {
        u32 sector = cluster_to_sector(cluster);
        for (u32 s = 0; s < sectors_per_cluster && remaining > 0; s++) {
            if (!VirtioBlk::read_sector(sector + s, sector_buf)) return false;
            u32 to_copy = remaining < bytes_per_sector ? remaining : bytes_per_sector;
            memcpy(dst, sector_buf, to_copy);
            dst += to_copy;
            remaining -= to_copy;
        }
        cluster = next_cluster(cluster);
    }

    *bytes_read = fsize;
    return true;
}

bool file_exists(const char *path) {
    if (!mounted) return false;
    DirEntry de;
    return resolve_path(path, &de);
}

u32 file_size(const char *path) {
    if (!mounted) return 0;
    DirEntry de;
    if (!resolve_path(path, &de)) return 0;
    return de.file_size;
}

} // namespace FAT32
