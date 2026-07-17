/*
 * test_aros_mkdir.c -- minimal empirical probe for the "doupath"
 * question raised while investigating "lg2 init peo" failing with
 * "failed to make directory './.'".
 *
 * Our cross-compiled binaries resolve mkdir()/realpath()/etc. through
 * a runtime CrtBase indirection (confirmed via `nm`: no nixmain/
 * doupath/path_u2a symbols anywhere in our own binaries), so whether
 * AROS's Unix->AmigaDOS path translation (which would turn "./peo"
 * into a clean "peo" before calling CreateDir()) is actually active
 * for a normal `lg2` run can't be determined by cross-compiling or
 * reading source -- only by running on the real target. This is the
 * plain libc mkdir() libgit2's p_mkdir() calls, nothing of ours in
 * the way.
 *
 * Build (cross, on Pop!_OS):
 *   x86_64-aros-gcc test_aros_mkdir.c -o test_aros_mkdir
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    const char *name = "test_aros_mkdir_probe";
    char path[64];

    snprintf(path, sizeof(path), "./%s", name);

    printf("mkdir(\"%s\", 0755) = ", path);

    if (mkdir(path, 0755) == 0)
    {
        printf("OK\n");
        rmdir(path);
    }
    else
    {
        printf("FAILED, errno = %d (%s)\n", errno, strerror(errno));
    }

    return 0;
}
