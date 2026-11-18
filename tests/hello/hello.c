/* Step 1 of the port: a C function in an Atmosphere process prints through
 * the kernel's console system call. */
#include "atmo_rt.h"

static size_t length(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

static const char *const lines[] = {
    "Hello from C on Atmosphere\n",
    "a partial line, ",
    "completed by a second write\n",
};

int main(void)
{
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
        art_console_write(lines[i], length(lines[i]));
    return 42;
}
