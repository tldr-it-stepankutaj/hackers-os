#include "../kernel/kernel.h"

extern "C" {

void *memset(void *s, int c, usize n) {
    u8 *p = static_cast<u8*>(s);
    while (n--) *p++ = static_cast<u8>(c);
    return s;
}

void *memcpy(void *dest, const void *src, usize n) {
    u8 *d = static_cast<u8*>(dest);
    const u8 *s = static_cast<const u8*>(src);
    while (n--) *d++ = *s++;
    return dest;
}

int memcmp(const void *s1, const void *s2, usize n) {
    const u8 *a = static_cast<const u8*>(s1);
    const u8 *b = static_cast<const u8*>(s2);
    for (usize i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
    }
    return 0;
}

usize strlen(const char *s) {
    usize len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return static_cast<u8>(*s1) - static_cast<u8>(*s2);
}

int strncmp(const char *s1, const char *s2, usize n) {
    for (usize i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return static_cast<u8>(s1[i]) - static_cast<u8>(s2[i]);
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, usize n) {
    usize i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == static_cast<char>(c)) return const_cast<char*>(s);
        s++;
    }
    if (c == '\0') return const_cast<char*>(s);
    return NULL;
}

} // extern "C"
