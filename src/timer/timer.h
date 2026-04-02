#pragma once
#include "../kernel/kernel.h"

namespace Timer {

void init();
u64 get_ticks();
u64 get_frequency();
u64 uptime_seconds();

} // namespace Timer
