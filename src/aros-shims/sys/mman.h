/*
 * sys/mman.h shim for AROS.
 *
 * AROS has no sys/mman.h at all (unlike select(), which existed but
 * in the wrong form -- this one is simply absent). This header only
 * declares what libgit2's src/util/unix/map.c needs to COMPILE.
 * The actual implementation lives in src/aros_mman.c and is a real
 * (but "fake") mmap: malloc + read, not a genuine memory mapping.
 * See the caveats there before relying on this for anything beyond
 * read-only pack file access.
 *
 * Safe to add globally to the include path (unlike the select()
 * fix): since this header doesn't exist on AROS at all, there is no
 * risk of colliding with some other already-working resolution of
 * <sys/mman.h> the way proto/socket.h collided with filter.c's
 * "shutdown" struct field.
 */

#ifndef AROS_SYS_MMAN_H
#define AROS_SYS_MMAN_H

#include <stddef.h>
#include <sys/types.h>

#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

#define MAP_SHARED  0x01
#define MAP_PRIVATE 0x02

#define MAP_FAILED  ((void *) -1)

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int   munmap(void *addr, size_t length);

#endif /* AROS_SYS_MMAN_H */
