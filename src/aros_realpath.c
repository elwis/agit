/*
 * aros_realpath.c -- see src/aros-shims/aros_realpath.h for the full
 * rationale (AROS's native realpath() is declared but fundamentally
 * broken; this is dos.library's Lock()/NameFromLock() instead).
 *
 * Unlike bsdsocket.library (see aros_net.c), dos.library needs no
 * explicit OpenLibrary()/CloseLibrary(): proto/dos.h resolves DOSBase
 * through __aros_getbase_DOSBase(), an accessor AROS's C runtime
 * already provides for every hosted process -- dos.library is a core
 * library every AROS process needs from the moment argv/envp exist,
 * unlike the optional bsdsocket.library. Confirmed by reading
 * proto/dos.h and inline/dos.h directly in $SYSROOT (the
 * __DOS_LIBBASE macro expands to __aros_getbase_DOSBase() when no
 * caller-supplied DOSBase is in scope), not assumed.
 *
 * Call-ordering note (verified by reading libgit2's actual source, not
 * assumed): every call site that matters for `lg2 init`/`lg2 clone`
 * (git_repository_init_ext()'s repo_init_directories() in
 * src/libgit2/repository.c) creates the target directory tree
 * (git_futils_mkdir()) BEFORE calling git_fs_path_prettify_dir(), i.e.
 * before p_realpath() ever runs -- see the "prettify both directories
 * now that they are created" comment directly above that call in
 * repository.c. git_repository_init() (used by both `lg2 init` and,
 * transitively, `lg2 clone`) always sets GIT_REPOSITORY_INIT_MKPATH,
 * so this is unconditional for both commands. That means this
 * implementation does NOT need to replicate glibc realpath()'s GNU
 * extension of resolving a path whose final component doesn't exist
 * yet -- every path this project's call sites hand us already exists
 * on disk. If a future libgit2 bump adds a p_realpath() call ahead of
 * directory creation, that assumption needs re-checking.
 */

#include "aros-shims/aros_realpath.h"
#include "aros-shims/aros_path_shims.h"

#include <proto/dos.h>
#include <dos/dos.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>

/*
 * None of libgit2's current p_realpath() call sites (src/util/fs_path.c,
 * src/libgit2/repository.c) pass a buffer smaller than GIT_PATH_MAX
 * (4096, include/git2/common.h) -- p_realpath()'s signature carries no
 * length, exactly like real POSIX realpath(), so this is the same
 * implicit "caller guarantees PATH_MAX-ish space" contract every other
 * platform's realpath() relies on, just pinned to a literal instead of
 * <limits.h>'s PATH_MAX (AROS's own PATH_MAX is unreliable for the same
 * reason its realpath() is -- see the header for the full story).
 */
#define AROS_REALPATH_BUF_SIZE 4096

/* Maps a dos.library IoErr() code onto the nearest POSIX errno. Only
 * covers the codes Lock()/NameFromLock() actually document returning;
 * everything else becomes a generic EIO rather than guessing. */
static void aros_realpath_set_errno(LONG dos_err)
{
    switch (dos_err)
    {
    case ERROR_OBJECT_NOT_FOUND:
    case ERROR_DIR_NOT_FOUND:
        errno = ENOENT;
        break;
    case ERROR_INVALID_COMPONENT_NAME:
    case ERROR_DEVICE_NOT_MOUNTED:
    case ERROR_OBJECT_WRONG_TYPE:
        errno = ENOTDIR;
        break;
    case ERROR_LINE_TOO_LONG:
        errno = ENAMETOOLONG;
        break;
    case ERROR_NO_FREE_STORE:
        errno = ENOMEM;
        break;
    default:
        errno = EIO;
        break;
    }
}

char *aros_realpath(const char *pathname, char *resolved)
{
    BPTR lock;
    char local[AROS_REALPATH_BUF_SIZE];
    char *out = resolved ? resolved : local;

    if (!pathname)
    {
        errno = EINVAL;
        return NULL;
    }

    /*
     * "." is the one input real code always passes -- libgit2 starts
     * every discovery/prettify walk from the current directory -- and
     * is exactly the input AROS's broken native realpath() failed on
     * (see test_realpath.c). AmigaDOS's path grammar has no "."
     * token at all: confirmed by reading AddPart()'s autodoc
     * (rom/dos/addpart.c) -- "/" is a plain component separator (or,
     * used leading, a "go up one level" operator), but a component
     * that is literally "." is just an ordinary (almost always
     * nonexistent) filename to any AmigaDOS-compliant filesystem
     * handler, never a "stay here" marker the way Unix treats it.
     * Lock(".", ...) therefore fails the same way Lock("./.", ...)
     * would: "./." splits into two literal "." components, and the
     * FIRST one already isn't found. Strip every leading "./" first
     * (libgit2's git_str_joinpath()/repo_init_directories() can and
     * does construct compound forms like "./.git" -> dirname -> "."
     * -> to_dir -> "./" for a plain `lg2 init .`, not just a bare
     * "."), then treat whatever remains -- "" or a lone "." -- as
     * "current directory, unchanged".
     *
     * The portable, handler-independent AmigaDOS idiom for "a lock on
     * this directory, unchanged" is an EMPTY name string resolved
     * against an existing lock -- see RootDir() in AROS's own
     * rom/dos/lock.c, which uses exactly this idiom
     * (fs_LocateObject(..., "", SHARED_LOCK, ...)) to lock a volume's
     * root relative to a DevProc lock. Substituting "" here relies on
     * that guaranteed-everywhere mechanism instead of the unverified
     * (and, per the above, wrong) assumption that "." parses as
     * "current directory" in any handler.
     *
     * The "./" stripping itself is shared with every other AROS POSIX
     * call site (see aros_path_shims.h) -- confirmed empirically on
     * real hardware that AROS's own Unix->AmigaDOS path translation is
     * not active for this project's binaries, so this isn't a
     * realpath()-specific quirk. Only the "collapse the fully-stripped
     * remainder to the Lock()-specific '' idiom" step below is
     * specific to this function.
     */
    pathname = aros_strip_dotslash(pathname);

    if (pathname[0] == '\0' || strcmp(pathname, ".") == 0)
        pathname = "";

    lock = Lock((CONST_STRPTR)pathname, ACCESS_READ);
    if (!lock)
    {
        aros_realpath_set_errno(IoErr());
        return NULL;
    }

    if (!NameFromLock(lock, (STRPTR)out, AROS_REALPATH_BUF_SIZE))
    {
        LONG err = IoErr();
        UnLock(lock);
        aros_realpath_set_errno(err);
        return NULL;
    }

    UnLock(lock);

    /*
     * POSIX realpath(path, NULL) allocates and returns a caller-owned
     * buffer sized to fit the result exactly. p_realpath()
     * (deps/libgit2/src/util/unix/realpath.c) relies on this: when
     * resolved == NULL it git__strdup()s whatever we return and
     * free()s the original -- so the original must be a plain
     * malloc()-family allocation, which strdup() is.
     */
    if (!resolved)
    {
        char *dup = strdup(local);
        if (!dup)
        {
            errno = ENOMEM;
            return NULL;
        }
        return dup;
    }

    return resolved;
}
