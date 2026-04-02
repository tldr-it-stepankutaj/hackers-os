#pragma once
#include "../kernel/kernel.h"

namespace Scheduler {

void init();
void yield();
void schedule();

} // namespace Scheduler
