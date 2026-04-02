#pragma once
#include "../kernel/kernel.h"

namespace GIC {

void init();
void enable_irq(u32 intid);
void disable_irq(u32 intid);
u32 acknowledge();
void end_of_interrupt(u32 intid);
void set_priority(u32 intid, u8 priority);
void set_target(u32 intid, u8 cpu_mask);

} // namespace GIC
