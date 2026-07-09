/*
 * aros_posix_shims.h -- small POSIX gaps AROS's headers don't declare
 * at all: getpwuid_r(), getsid(), pread(), pwrite().
 *
 * Discovered linking examples/lg2 against libgit2.a: these are called
 * from vendor code (sysdir.c's $HOME fallback, rand.c's non-crypto
 * seed mixing, and pack/index random-access reads via the p_pread/
 * p_pwrite macros in posix.h) but AROS's <pwd.h>/<unistd.h> either
 * omit them entirely or explicitly mark them "NOTIMPL" (see the
 * commented-out getpwuid_r/getpwnam_r declarations in
 * include/pwd.h) -- so no prototype exists to satisfy the compiler,
 * and no implementation exists to satisfy the linker.
 *
 * See src/aros_posix_shims.c for the implementations and the
 * rationale for each one individually. Included into
 * deps/libgit2/src/util/posix.h via
 * patches/libgit2-aros-posix-shims.patch, since that header is
 * transitively included by every libgit2 translation unit that needs
 * any of these four (pack.c, indexer.c, commit_graph.c, midx.c,
 * sysdir.c, rand.c) -- one shared insertion point instead of patching
 * each call site.
 */

#ifndef AROS_POSIX_SHIMS_H
#define AROS_POSIX_SHIMS_H

#include <sys/types.h>
#include <pwd.h>

int aros_getpwuid_r(uid_t uid, struct passwd *pwd, char *buf,
                     size_t buflen, struct passwd **result);
pid_t aros_getsid(pid_t pid);
ssize_t aros_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t aros_pwrite(int fd, const void *buf, size_t count, off_t offset);

/*
 * None of the four real names are declared anywhere in AROS's
 * headers (getpwuid_r/getpwnam_r are explicitly commented out as
 * NOTIMPL in <pwd.h>; getsid/pread/pwrite don't appear in <unistd.h>
 * at all), so mapping them as macros here carries no collision risk.
 */
#define getpwuid_r(uid, pwd, buf, buflen, result) \
        aros_getpwuid_r(uid, pwd, buf, buflen, result)
#define getsid(pid)              aros_getsid(pid)
#define pread(fd, buf, n, off)   aros_pread(fd, buf, n, off)
#define pwrite(fd, buf, n, off)  aros_pwrite(fd, buf, n, off)

#endif /* AROS_POSIX_SHIMS_H */
