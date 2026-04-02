#pragma once

using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using i8  = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;

using usize = u64;
using isize = i64;

#define NULL nullptr

extern "C" {
    void *memset(void *s, int c, usize n);
    void *memcpy(void *dest, const void *src, usize n);
    int memcmp(const void *s1, const void *s2, usize n);
    usize strlen(const char *s);
    int strcmp(const char *s1, const char *s2);
    int strncmp(const char *s1, const char *s2, usize n);
    char *strcpy(char *dest, const char *src);
    char *strncpy(char *dest, const char *src, usize n);
    char *strcat(char *dest, const char *src);
    char *strchr(const char *s, int c);
}
