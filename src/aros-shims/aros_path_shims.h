/*
 * aros_path_shims.h -- shared "./"-prefix stripping for AROS's POSIX
 * call sites.
 *
 * Confirmed empirically on real AROS hardware (AROS One, VirtualBox):
 * AROS's Unix->AmigaDOS path translation ("doupath" in AROS's own
 * posixc library, see doc/Building.md's "AROS's broken realpath()")
 * is NOT active for this project's cross-compiled binaries -- a bare
 * `mkdir("./somename", 0755)` fails with ENOENT. That means any path
 * with a leading "./" reaches AmigaDOS's native Lock()/CreateDir()-
 * style parser completely unmodified, and that parser has no "./"
 * concept at all (confirmed via AddPart()'s autodoc -- see
 * src/aros_realpath.c's "." handling for the fuller root-cause
 * writeup). This isn't specific to realpath() or mkdir() individually
 * -- it's true of every AROS POSIX call that takes a path, so rather
 * than patch libgit2 call sites one at a time (fragile against future
 * libgit2 bumps, and agit's own future code would have to remember
 * the same rule), every one of AROS's `p_*` POSIX wrappers strips the
 * prefix itself -- see patches/libgit2-aros-path-normalize.patch,
 * which redirects deps/libgit2/src/util/unix/posix.h's and
 * src/util/posix.c's path-taking macros/functions through
 * aros_strip_dotslash() below.
 *
 * This function ONLY strips the prefix -- it does not collapse a
 * fully-stripped result ("" or a lone ".") to anything else. That
 * substitution (an empty name resolved against an existing lock) is
 * verified correct specifically for Lock() (see aros_realpath.c and
 * AROS's own rom/dos/lock.c RootDir()); whether AmigaDOS's
 * CreateDir()/Examine()/Open() etc. treat an empty name the same way
 * is a separate, unverified question, so callers that need that
 * behavior (currently just aros_realpath()) handle it themselves on
 * top of this.
 */

#ifndef AROS_PATH_SHIMS_H
#define AROS_PATH_SHIMS_H

/*
 * Strips every leading "./" from path (e.g. "././peo" -> "peo").
 * Never allocates -- just advances the pointer -- so it's safe to use
 * directly as a macro argument expression. NULL in, NULL out.
 */
const char *aros_strip_dotslash(const char *path);

#endif /* AROS_PATH_SHIMS_H */
