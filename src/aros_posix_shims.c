/*
 * aros_posix_shims.c -- see src/aros-shims/aros_posix_shims.h for how
 * these get wired into libgit2. Each function's honest limitation is
 * documented individually below.
 */

#include "aros-shims/aros_posix_shims.h"

#include <unistd.h>

/*
 * AROS has no multi-user passwd database at all (see the NOTIMPL
 * comment in the real <pwd.h>) -- there is no uid to look up. The
 * only caller (libgit2's sysdir.c get_passwd_home()) uses this
 * strictly as a fallback for locating $HOME when the environment
 * variable isn't set, and already handles "no entry found" as a
 * plain (non-fatal) miss. Returning 0 with *result = NULL is the
 * textbook POSIX way to report exactly that -- not an error, just
 * nothing to report -- matching how glibc's own getpwuid_r() behaves
 * for an unknown uid.
 */
int aros_getpwuid_r(uid_t uid, struct passwd *pwd, char *buf,
                     size_t buflen, struct passwd **result)
{
    (void)uid;
    (void)pwd;
    (void)buf;
    (void)buflen;

    *result = NULL;
    return 0;
}

/*
 * AROS has no session-ID concept (no setsid()-style process groups
 * tying back to a controlling terminal in the Unix sense). The only
 * caller (libgit2's rand.c) XORs this in as one of several ingredients
 * (pid, ppid, pgid, uid, gid, wall clock) for a non-cryptographic RNG
 * seed used for things like temp file name jitter -- not security
 * sensitive (see the honesty warning in aros_entropy.h for where we
 * DO care about that). Any fixed value is fine here; -1 mirrors the
 * usual "no such session" POSIX error return.
 */
pid_t aros_getsid(pid_t pid)
{
    (void)pid;
    return -1;
}

/*
 * AROS's <unistd.h> has lseek() but no pread()/pwrite() at all.
 * THREADSAFE=OFF for this project (see docs/BUILDING.md) means there
 * is never a second thread that could race us between the seek and
 * the read/write, so plain lseek-then-read/write-then-restore is
 * exactly as safe here as the real syscall would be for our use case
 * (libgit2 uses these for random-access pack/index reads, never
 * concurrently on the same fd from multiple threads in this build).
 */
ssize_t aros_pread(int fd, void *buf, size_t count, off_t offset)
{
    off_t saved = lseek(fd, 0, SEEK_CUR);
    ssize_t ret;

    if (saved < 0 || lseek(fd, offset, SEEK_SET) < 0)
        return -1;

    ret = read(fd, buf, count);
    lseek(fd, saved, SEEK_SET);

    return ret;
}

ssize_t aros_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    off_t saved = lseek(fd, 0, SEEK_CUR);
    ssize_t ret;

    if (saved < 0 || lseek(fd, offset, SEEK_SET) < 0)
        return -1;

    ret = write(fd, buf, count);
    lseek(fd, saved, SEEK_SET);

    return ret;
}
