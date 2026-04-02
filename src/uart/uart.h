#pragma once
#include "../kernel/kernel.h"

namespace UART {

void init();
void putc(char c);
char getc();
void puts(const char *s);
void puthex(u64 value);
void putdec(i64 value);
void printf(const char *fmt, ...);

} // namespace UART
