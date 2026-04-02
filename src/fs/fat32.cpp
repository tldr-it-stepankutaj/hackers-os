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
static u32 num_fats = 2;
static u32 fat_size_sectors = 0;
static u32 total_clusters = 0;

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
    num_fats = bpb->num_fats;
    fat_size_sectors = bpb->fat_size_32;
    data_start_sector = fat_start_sector + num_fats * fat_size_sectors;
    root_cluster = bpb->root_cluster;
    total_clusters = (bpb->total_sectors_32 - data_start_sector) / sectors_per_cluster;
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

// ============================================================
// Write support
// ============================================================

// Write a FAT entry (update all copies of FAT)
static bool set_fat_entry(u32 cluster, u32 value) {
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_start_sector + (fat_offset / bytes_per_sector);
    u32 entry_offset = fat_offset % bytes_per_sector;

    for (u32 f = 0; f < num_fats; f++) {
        u32 sector = fat_sector + f * fat_size_sectors;
        if (!VirtioBlk::read_sector(sector, sector_buf)) return false;
        u32 *entry = reinterpret_cast<u32*>(&sector_buf[entry_offset]);
        *entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);
        if (!VirtioBlk::write_sector(sector, sector_buf)) return false;
    }
    return true;
}

// Allocate a free cluster
static u32 alloc_cluster() {
    for (u32 c = 2; c < total_clusters + 2; c++) {
        // Read the raw FAT entry
        u32 fat_offset = c * 4;
        u32 fat_sector_num = fat_start_sector + (fat_offset / bytes_per_sector);
        u32 entry_offset = fat_offset % bytes_per_sector;
        if (!VirtioBlk::read_sector(fat_sector_num, sector_buf)) continue;
        u32 raw = *reinterpret_cast<u32*>(&sector_buf[entry_offset]) & 0x0FFFFFFF;
        if (raw == 0) {
            // Mark as end-of-chain
            set_fat_entry(c, 0x0FFFFFF8);
            // Zero the cluster data
            u32 sector = cluster_to_sector(c);
            u8 zero[512];
            memset(zero, 0, sizeof(zero));
            for (u32 s = 0; s < sectors_per_cluster; s++) {
                VirtioBlk::write_sector(sector + s, zero);
            }
            return c;
        }
    }
    return 0;  // No free clusters
}

// Convert filename to 8.3 format
static void string_to_fat_name(const char *name, u8 *fat_name) {
    memset(fat_name, ' ', 11);
    int i = 0;
    // Name part (up to 8 chars)
    for (; i < 8 && *name && *name != '.'; i++, name++) {
        fat_name[i] = (*name >= 'a' && *name <= 'z') ? *name - 32 : *name;
    }
    // Skip to extension
    while (*name && *name != '.') name++;
    if (*name == '.') {
        name++;
        for (int j = 0; j < 3 && *name; j++, name++) {
            fat_name[8 + j] = (*name >= 'a' && *name <= 'z') ? *name - 32 : *name;
        }
    }
}

bool write_file(const char *path, const void *data, u32 size) {
    if (!mounted) return false;

    DirEntry de;
    if (!resolve_path(path, &de)) return false;
    if (de.attr & ATTR_DIRECTORY) return false;

    u32 cluster = (static_cast<u32>(de.first_cluster_hi) << 16) | de.first_cluster_lo;
    u32 remaining = size;
    const u8 *src = static_cast<const u8*>(data);

    // Write data following existing cluster chain, allocating new clusters as needed
    u32 prev_cluster = 0;
    while (remaining > 0) {
        if (is_end_cluster(cluster) || cluster == 0) {
            // Need a new cluster
            u32 new_c = alloc_cluster();
            if (new_c == 0) return false;
            if (prev_cluster) {
                set_fat_entry(prev_cluster, new_c);
            }
            cluster = new_c;
        }

        u32 sector = cluster_to_sector(cluster);
        for (u32 s = 0; s < sectors_per_cluster && remaining > 0; s++) {
            u32 to_write = remaining < bytes_per_sector ? remaining : bytes_per_sector;
            memset(sector_buf, 0, bytes_per_sector);
            memcpy(sector_buf, src, to_write);
            if (!VirtioBlk::write_sector(sector + s, sector_buf)) return false;
            src += to_write;
            remaining -= to_write;
        }

        prev_cluster = cluster;
        cluster = next_cluster(cluster);
    }

    // Update directory entry with new size
    // (simplified: we'd need to find and update the dir entry on disk)
    // For now, the file must already exist with the right size
    return true;
}

bool create_file(const char *dir_path, const char *filename) {
    if (!mounted) return false;

    DirEntry de;
    u32 dir_cluster;
    if (dir_path[0] == '/' && dir_path[1] == '\0') {
        dir_cluster = root_cluster;
    } else {
        if (!resolve_path(dir_path, &de)) return false;
        if (!(de.attr & ATTR_DIRECTORY)) return false;
        dir_cluster = (static_cast<u32>(de.first_cluster_hi) << 16) | de.first_cluster_lo;
    }

    // Find empty slot in directory
    u32 cluster = dir_cluster;
    while (!is_end_cluster(cluster)) {
        u32 sector = cluster_to_sector(cluster);
        for (u32 s = 0; s < sectors_per_cluster; s++) {
            if (!VirtioBlk::read_sector(sector + s, sector_buf)) return false;

            DirEntry *entries = reinterpret_cast<DirEntry*>(sector_buf);
            u32 entries_per = bytes_per_sector / sizeof(DirEntry);

            for (u32 e = 0; e < entries_per; e++) {
                if (entries[e].name[0] == 0x00 || entries[e].name[0] == 0xE5) {
                    // Found empty slot
                    memset(&entries[e], 0, sizeof(DirEntry));
                    string_to_fat_name(filename, entries[e].name);
                    entries[e].attr = ATTR_ARCHIVE;

                    // Allocate first cluster
                    u32 first = alloc_cluster();
                    if (first == 0) return false;
                    entries[e].first_cluster_hi = (first >> 16) & 0xFFFF;
                    entries[e].first_cluster_lo = first & 0xFFFF;
                    entries[e].file_size = 0;

                    return VirtioBlk::write_sector(sector + s, sector_buf);
                }
            }
        }
        cluster = next_cluster(cluster);
    }
    return false;  // Directory full
}

bool delete_file(const char *path) {
    if (!mounted) return false;

    // Find the file's directory entry on disk and mark as deleted
    // (simplified version: mark first byte as 0xE5 and free clusters)

    DirEntry de;
    if (!resolve_path(path, &de)) return false;
    if (de.attr & ATTR_DIRECTORY) return false;

    // Free cluster chain
    u32 cluster = (static_cast<u32>(de.first_cluster_hi) << 16) | de.first_cluster_lo;
    while (!is_end_cluster(cluster) && cluster >= 2) {
        u32 next = next_cluster(cluster);
        set_fat_entry(cluster, 0);  // Mark as free
        cluster = next;
    }

    // Note: we'd need to also mark the directory entry as deleted (0xE5)
    // This requires tracking which sector/offset the dir entry was found at
    return true;
}

} // namespace FAT32
