/*
 * aros_mman.c -- honest malloc+read emulation of mmap()/munmap() for
 * AROS, which has no real memory-mapping facility we've found.
 *
 * IMPORTANT LIMITATIONS -- read before relying on this beyond
 * read-only pack file access (libgit2's primary use case):
 *
 *   - Not a real mapping: the whole requested range is read into a
 *     malloc'd buffer up front. No lazy/demand paging, no sharing of
 *     physical pages between processes, no benefit from the OS page
 *     cache the way real mmap has.
 *   - MAP_SHARED writes are NOT written back to the file. We only
 *     implement enough for libgit2's actual usage, which is
 *     overwhelmingly MAP_PRIVATE, read-only pack/idx file access.
 *   - Large files cost real RAM 1:1 rather than being paged in
 *     on-demand. Fine for typical git pack files on a hobby project.
 *   - fd is used only at map time; the caller's normal close(fd)
 *     afterwards is fine, matching how real mmap decouples the
 *     mapping from the fd's lifetime.
 */

#include "sys/mman.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    void *buf;
    ssize_t total_read = 0;
    ssize_t r;

    (void)addr;   /* we never honor a requested address */
    (void)prot;   /* no real page protection is applied */
    (void)flags;  /* MAP_SHARED is accepted but not honored, see above */

    if (length == 0)
    {
        errno = EINVAL;
        return MAP_FAILED;
    }

    buf = malloc(length);
    if (!buf)
    {
        errno = ENOMEM;
        return MAP_FAILED;
    }

    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        free(buf);
        return MAP_FAILED;
    }

    while ((size_t)total_read < length)
    {
        r = read(fd, (char *)buf + total_read, length - (size_t)total_read);
        if (r < 0)
        {
            free(buf);
            return MAP_FAILED;
        }
        if (r == 0)
            break;
        total_read += r;
    }

    return buf;
}

int munmap(void *addr, size_t length)
{
    (void)length;
    free(addr);
    return 0;
}
