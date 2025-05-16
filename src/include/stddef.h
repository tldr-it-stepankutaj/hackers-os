#ifndef _STDDEF_H
#define _STDDEF_H

// Include our custom stdint.h for size_t
#include <stdint.h>

// Define NULL pointer
#define NULL ((void*)0)

// Define offsetof macro
#define offsetof(type, member) __builtin_offsetof(type, member)

#endif // _STDDEF_H
