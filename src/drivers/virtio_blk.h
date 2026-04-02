#pragma once
#include "../kernel/kernel.h"

namespace VirtioBlk {

bool init();
bool read_sector(u64 sector, void *buf);
bool write_sector(u64 sector, const void *buf);
u64 get_capacity();

} // namespace VirtioBlk
