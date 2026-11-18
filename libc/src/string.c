/* String functions. memcpy, memmove, memset, and memcmp come from the Rust
 * compiler builtins that are linked in with the runtime. */
#include <string.h>

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n != 0; p++, n--) {
        if (*p == (unsigned char)c)
            return (void *)p;
    }
    return NULL;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

size_t strnlen(const char *s, size_t max)
{
    size_t n = 0;
    while (n < max && s[n] != '\0')
        n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n != 0; a++, b++, n--) {
        if (*a != *b || *a == '\0')
            return (unsigned char)*a - (unsigned char)*b;
    }
    return 0;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != '\0') {
    }
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

char *strcat(char *dst, const char *src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
}

char *strncat(char *dst, const char *src, size_t n)
{
    char *d = dst + strlen(dst);
    for (; n != 0 && *src != '\0'; n--)
        *d++ = *src++;
    *d = '\0';
    return dst;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c)
            return (char *)s;
        if (*s == '\0')
            return NULL;
    }
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    for (;; s++) {
        if (*s == (char)c)
            last = s;
        if (*s == '\0')
            return (char *)last;
    }
}

char *strstr(const char *haystack, const char *needle)
{
    size_t n = strlen(needle);
    if (n == 0)
        return (char *)haystack;
    for (; *haystack != '\0'; haystack++) {
        if (*haystack == *needle && strncmp(haystack, needle, n) == 0)
            return (char *)haystack;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept)
{
    size_t n = 0;
    while (s[n] != '\0' && strchr(accept, s[n]) != NULL)
        n++;
    return n;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t n = 0;
    while (s[n] != '\0' && strchr(reject, s[n]) == NULL)
        n++;
    return n;
}

char *strerror(int errnum)
{
    (void)errnum;
    return "error";
}
