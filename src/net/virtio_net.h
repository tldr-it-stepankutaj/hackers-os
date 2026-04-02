#pragma once
#include "../kernel/kernel.h"
#include "net.h"

namespace VirtioNet {

bool init();
bool send(const void *data, u32 len);
bool receive(void *buf, u32 buf_size, u32 *received);
MacAddr get_mac();
void poll();

} // namespace VirtioNet
