/*
 * test_realpath.c -- isolated test of AROS's realpath(), bypassing
 * libgit2 entirely. Run this ON AROS (not cross-compiled-and-assumed)
 * to see the raw, unwrapped behavior.
 *
 * Build (cross, on Pop!_OS):
 *   x86_64-aros-gcc test_realpath.c -o test_realpath
 * (the toolchain wrapper already has --sysroot baked in)
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static void try_realpath(const char *input)
{
    char buf[1024];
    char *result;

    memset(buf, 0, sizeof(buf));
    errno = 0;
    result = realpath(input, buf);

    if (result == NULL)
    {
        printf("realpath(\"%s\", buf) = NULL, errno = %d (%s)\n",
               input, errno, strerror(errno));
    }
    else
    {
        printf("realpath(\"%s\", buf) = \"%s\"\n", input, result);
    }
}

int main(void)
{
    printf("=== AROS realpath() isolated test ===\n\n");

    try_realpath(".");
    try_realpath("RAM:");
    try_realpath("RAM:lennart");
    try_realpath("lennart");     /* relative, likely doesn't exist yet */
    try_realpath("RAM Disk:");   /* the actual volume name with a space */

    printf("\nDone.\n");
    return 0;
}
