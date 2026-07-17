/*
 * aros_realpath.h -- realpath() replacement for AROS.
 *
 * AROS's native realpath() IS correctly declared under the
 * _GNU_SOURCE guard (confirmed: libgit2's cmake/DefaultCFlags.cmake
 * unconditionally sets -D_GNU_SOURCE, verified against the actual
 * compiler invocation in a verbose build log, not assumed) -- but the
 * implementation itself is broken. Isolated testing on real AROS
 * hardware (test_realpath.c at the repo root) showed realpath(".")
 * returning NULL/ENOENT, which is impossible for a correct
 * implementation (the current directory always exists). This isn't
 * an edge case in AROS's colon/volume path syntax -- the underlying
 * implementation doesn't work at all, for any input tried.
 *
 * Rather than repair an implementation of unknown depth of breakage
 * (and POSIX path-walking semantics don't map cleanly onto AROS's
 * volume/assign-based filesystem model anyway -- the same reason
 * getaddrinfo() got a from-scratch AROS-native implementation instead
 * of a patched POSIX one, see src/aros_dns.c), this uses AmigaDOS's
 * own native path-resolution mechanism: Lock() a target, then
 * NameFromLock() asks the owning filesystem handler for its fully
 * qualified, canonical name. See src/aros_realpath.c for the
 * implementation.
 *
 * Included into deps/libgit2/src/util/unix/realpath.c via
 * patches/libgit2-aros-realpath.patch (patch #6). That file already
 * implements every bit of realpath()'s NULL-buffer/allocator contract
 * generically on top of a plain call to `realpath(pathname, resolved)`
 * (see p_realpath() there) -- redirecting just that one name, same
 * pattern as aros_dns.h's getaddrinfo() shim, means none of that logic
 * needs to be duplicated here.
 *
 * AROS's own <stdlib.h> still declares its own (broken) realpath()
 * unconditionally alongside this -- that declaration is simply never
 * used by unix/realpath.c once this header's #define takes effect for
 * the rest of the file. No collision: unlike aros_dns.h's struct
 * addrinfo situation, this is just an unused prototype sitting
 * alongside ours, not a conflicting type definition.
 */

#ifndef AROS_REALPATH_H
#define AROS_REALPATH_H

char *aros_realpath(const char *pathname, char *resolved);

#define realpath(pathname, resolved) aros_realpath(pathname, resolved)

#endif /* AROS_REALPATH_H */
