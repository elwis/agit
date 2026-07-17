/*
 * test_aros_realpath.c -- isolated test of aros_realpath() (the
 * Lock()/NameFromLock()-based replacement for AROS's broken native
 * realpath(), see src/aros_realpath.c), bypassing libgit2 entirely.
 * Run this ON AROS (not cross-compiled-and-assumed) and compare
 * against test_realpath.c's output for the same five inputs.
 *
 * Also includes a raw Lock()/UnLock() diagnostic, bypassing
 * aros_realpath()'s own "./" normalization entirely, to answer
 * directly: does AmigaDOS's native path parser understand "." or
 * "./." as "current directory" at all, or does aros_realpath()'s
 * normalization (stripping leading "./" before ever calling Lock())
 * genuinely need to exist? See the "." handling comment in
 * src/aros_realpath.c for the reasoning (AddPart()'s autodoc shows
 * "." has no special meaning in AmigaDOS path grammar); this is the
 * empirical check for that reasoning.
 *
 * Build (cross, on Pop!_OS):
 *   x86_64-aros-gcc -I src -I src/aros-shims \
 *       test_aros_realpath.c src/aros_realpath.c -o test_aros_realpath
 * (the toolchain wrapper already has --sysroot baked in)
 */

#include "aros-shims/aros_realpath.h"

#include <proto/dos.h>
#include <dos/dos.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

static void try_realpath(const char *input)
{
    char buf[1024];
    char *result;

    memset(buf, 0, sizeof(buf));
    errno = 0;
    result = aros_realpath(input, buf);

    if (result == NULL)
    {
        printf("aros_realpath(\"%s\", buf) = NULL, errno = %d (%s)\n",
               input, errno, strerror(errno));
    }
    else
    {
        printf("aros_realpath(\"%s\", buf) = \"%s\"\n", input, result);
    }
}

/* Raw Lock()/UnLock(), no normalization -- shows exactly what
 * AmigaDOS's own path parser does with an input, unmediated by
 * aros_realpath()'s "./" stripping. */
static void try_raw_lock(const char *input)
{
    BPTR lock = Lock((CONST_STRPTR)input, ACCESS_READ);

    if (!lock)
    {
        printf("Lock(\"%s\", ACCESS_READ) = 0 (failed), IoErr() = %ld\n",
               input, (long)IoErr());
    }
    else
    {
        char namebuf[1024];
        memset(namebuf, 0, sizeof(namebuf));
        if (NameFromLock(lock, (STRPTR)namebuf, sizeof(namebuf)))
            printf("Lock(\"%s\", ACCESS_READ) = succeeded, NameFromLock = \"%s\"\n",
                   input, namebuf);
        else
            printf("Lock(\"%s\", ACCESS_READ) = succeeded, but NameFromLock failed, IoErr() = %ld\n",
                   input, (long)IoErr());
        UnLock(lock);
    }
}

int main(void)
{
    const char *testdir = "test_aros_realpath_tmpdir";
    char compound[256];

    printf("=== raw Lock()/UnLock() diagnostic (no normalization) ===\n\n");
    try_raw_lock(".");
    try_raw_lock("./.");
    try_raw_lock("./");

    printf("\n=== aros_realpath() isolated test ===\n\n");

    try_realpath(".");
    try_realpath("./.");
    try_realpath("./");
    try_realpath("RAM:");
    try_realpath("RAM:lennart");
    try_realpath("lennart");     /* relative, likely doesn't exist yet */
    try_realpath("RAM Disk:");   /* the actual volume name with a space */

    /*
     * Compound "./<name>" case, using a directory this test creates
     * and cleans up itself rather than a hardcoded name that would
     * need to survive between runs.
     */
    printf("\n=== compound \"./<name>\" test (self-contained) ===\n\n");
    if (mkdir(testdir, 0755) != 0 && errno != EEXIST)
    {
        printf("mkdir(\"%s\") failed, errno = %d (%s) -- skipping compound test\n",
               testdir, errno, strerror(errno));
    }
    else
    {
        snprintf(compound, sizeof(compound), "./%s", testdir);
        try_realpath(compound);
        rmdir(testdir);
    }

    printf("\nDone.\n");
    return 0;
}
