# Building agit for AROS x86_64 on Linux


## Prerequisites

- AROS x86_64 cross toolchain (`x86_64-aros-gcc`, `x86_64-aros-ld`,
  etc.) in `PATH`
- An AROS x86_64 development sysroot (the `AROS/Development`
  directory from a `core-linux-x86_64` build)
- CMake >= 3.22
- Git >= 2.x with submodule support
- `curl` (to fetch `cacert.pem`, see below)

Set the sysroot path once per shell session (Note: you should wrap this in a script):

```bash
export SYSROOT=$HOME/Aros/arosbuilds/core-linux-x86_64-d/bin/linux-x86_64/AROS/Development
```

## 1. Clone and initialize submodules

```bash
git clone --recurse-submodules https://github.com/elwis/agit.git
cd agit
```

If you already have a checkout without submodules initialized:

```bash
git submodule update --init --recursive
```

Verify the submodules are pinned correctly (not sitting on some
random development branch commit):

```bash
git -C deps/libgit2 describe --tags   # should say v1.9.4
git -C deps/mbedtls describe --tags   # should say v3.6.6
```

## 2. Apply the AROS patches to libgit2

The libgit2 submodule tree must stay pristine (never commit modified
submodule content -- see "Why we patch this way" below). Instead, we
apply tracked patch files each time, in this order (order doesn't
actually matter -- they touch disjoint files -- but this is the order
they were developed in):

```bash
git apply patches/libgit2-aros-select.patch      --directory=deps/libgit2
git apply patches/libgit2-aros-getaddrinfo.patch --directory=deps/libgit2
git apply patches/libgit2-aros-posix-shims.patch --directory=deps/libgit2
git apply patches/libgit2-aros-lg2-init.patch    --directory=deps/libgit2
git apply patches/libgit2-aros-lg2-cmake.patch   --directory=deps/libgit2
git apply patches/libgit2-aros-realpath.patch    --directory=deps/libgit2
git apply patches/libgit2-aros-path-normalize.patch --directory=deps/libgit2
git apply patches/libgit2-aros-sock-cloexec.patch  --directory=deps/libgit2
git apply patches/libgit2-aros-cred.patch          --directory=deps/libgit2
git apply patches/libgit2-aros-fs-path-root.patch  --directory=deps/libgit2
git apply patches/libgit2-aros-push-head.patch    --directory=deps/libgit2
```

To undo all of them later (e.g. before bumping the libgit2 submodule
to a new tag):

```bash
git -C deps/libgit2 checkout -- .
```

Verify `git -C deps/libgit2 status` is clean before and after this
step whenever you're not actively mid-build -- the submodule should
never sit dirty at rest.

## 3. Build mbedTLS

```bash
rm -rf build-mbedtls
mkdir build-mbedtls && cd build-mbedtls

cmake -S ../deps/mbedtls -B . \
    -DCMAKE_TOOLCHAIN_FILE=$(pwd)/../cmake/aros-x86_64.cmake \
    -DENABLE_TESTING=OFF \
    -DENABLE_PROGRAMS=OFF

cmake --build . -j$(nproc)
cd ..
```

Produces `build-mbedtls/library/libmbedcrypto.a`, `libmbedx509.a`,
`libmbedtls.a`.

All AROS-specific mbedTLS configuration lives in
`cmake/mbedtls-user-config.h` and is pulled in automatically via
`MBEDTLS_USER_CONFIG_FILE` (baked into `cmake/aros-x86_64.cmake`'s
`CMAKE_C_FLAGS_INIT`). You should not need to pass anything extra on
the command line for this step.

## 4. Build libgit2 (and the lg2 example CLI)

```bash
rm -rf build-libgit2
mkdir build-libgit2 && cd build-libgit2

cmake -S ../deps/libgit2 -B . \
    -DCMAKE_TOOLCHAIN_FILE=$(pwd)/../cmake/aros-x86_64.cmake \
    -DUSE_HTTPS=mbedTLS \
    -DUSE_SSH=OFF \
    -DTHREADSAFE=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_CLI=OFF \
    -DBUILD_EXAMPLES=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DUSE_BUNDLED_ZLIB=ON \
    -DCMAKE_C_STANDARD=99 \
    -DCMAKE_C_EXTENSIONS=ON \
    -DGIT_IO_SELECT=1 \
    -DNEED_LIBRT=OFF \
    -DCMAKE_C_FLAGS="--sysroot=$SYSROOT -DMBEDTLS_NO_PLATFORM_ENTROPY -DMBEDTLS_PLATFORM_MS_TIME_ALT -DMBEDTLS_USER_CONFIG_FILE=\\\"mbedtls-user-config.h\\\" -I$(pwd)/../cmake -I$(pwd)/../src/aros-shims -I$(pwd)/../src" \
    -DMBEDTLS_INCLUDE_DIR=$(pwd)/../deps/mbedtls/include \
    -DMBEDTLS_LIBRARY=$(pwd)/../build-mbedtls/library/libmbedtls.a \
    -DMBEDCRYPTO_LIBRARY=$(pwd)/../build-mbedtls/library/libmbedcrypto.a \
    -DMBEDX509_LIBRARY=$(pwd)/../build-mbedtls/library/libmbedx509.a

cmake --build . -j$(nproc)
cd ..
```

Produces `build-libgit2/libgit2.a` and, since `BUILD_EXAMPLES=ON`,
`build-libgit2/examples/lg2` -- libgit2's own minimal example CLI
(clone/log/status/etc in one binary), cross-linked against our
`libgit2.a` plus agit's AROS glue code (`src/aros_net.c`,
`aros_dns.c`, `aros_entropy.c`, `aros_time.c`, `aros_mman.c`,
`aros_posix_shims.c`, `aros_realpath.c` -- wired in via
`patches/libgit2-aros-lg2-cmake.patch`, never baked into
`libgit2.a` itself, same principle as everything else in this
project). This is our integration test before writing agit's own
frontend -- see "Testing lg2 on AROS" below.

### Why every non-obvious flag is there

| Flag | Reason |
|---|---|
| `USE_SSH=OFF` | No modern SSH client exists for AROS x86_64; PAT-over-HTTPS is the whole point of this project |
| `THREADSAFE=OFF` | AROS pthread support is unverified/uncertain; avoid the question entirely |
| `BUILD_SHARED_LIBS=OFF` | The `.so` link step requires `-lrt`, which doesn't exist on AROS. We want a static lib anyway. |
| `USE_BUNDLED_ZLIB=ON` | Avoids depending on an unverified system zlib in the sysroot |
| `CMAKE_C_STANDARD=99` | libgit2 forces C90 by default (`CMakeLists.txt` sets `CMAKE_C_STANDARD "90"` as a non-forced cache variable). Under strict C90, `inline` is not a keyword, and AROS's `aros/stdc/ctype.h` / `aros/posixc/stdio.h` rely on `inline`-based macros (`__header_inline`, `__ctype_make_func`) with no C90 fallback -- they fail to parse. Since this cache variable has no `FORCE`, setting it ourselves via `-D` wins before libgit2's own `set()` call runs. No source patch needed. |
| `GIT_IO_SELECT=1` | libgit2's CMake uses `check_symbol_exists(select sys/select.h GIT_IO_SELECT)` to decide how to implement `poll()`. AROS's `aros/posixc/sys/select.h` deliberately leaves `select()` as `NOTIMPL` (the real one only exists via the `WaitSelect()` macro in `clib/bsdsocket_protos.h`, reached through `<proto/socket.h>`), so the symbol check fails and libgit2 falls through to `#error no poll compatible implementation`. Forcing this pre-empts the (failing) auto-detection, same "unforced cache variable" trick as `CMAKE_C_STANDARD`. |
| `-I.../cmake` | So `MBEDTLS_USER_CONFIG_FILE="mbedtls-user-config.h"` resolves (libgit2 includes mbedTLS headers directly, so it needs the same override header mbedTLS's own build used) |
| `-I.../src/aros-shims` | Provides `sys/mman.h`, `aros_dns.h`, `aros_posix_shims.h`, and `aros_realpath.h` -- see the deviation table entries below for what each covers. All are pure declaration headers; the real implementations (`src/aros_*.c`) are linked in later, only at final application (lg2/agit) link time, never needed to build `libgit2.a` itself. |
| `-I.../src` | So `examples/lg2.c` can `#include "aros_net.h"` (added by `patches/libgit2-aros-lg2-init.patch`) -- needed once `BUILD_EXAMPLES=ON` |
| `BUILD_EXAMPLES=ON` | Builds `examples/lg2`, libgit2's own minimal example CLI -- our integration test before writing agit's own frontend |
| `CMAKE_C_EXTENSIONS=ON` | libgit2's own `CMakeLists.txt` sets `option(CMAKE_C_EXTENSIONS OFF)` (strict `-std=c99`, no GNU extensions), which breaks as soon as any translation unit includes AROS's `<inline/exec.h>` (`aros_net.c` is the first to do so, for `OpenLibrary()`/`CloseLibrary()`): its `AROS_LIBREQ()` macro uses the bare `asm` keyword, which strict ISO C99 doesn't recognize as a keyword at all (`error: 'asm' undeclared`) -- only `__asm__` is guaranteed there. GNU extensions restore `asm` as a valid spelling. Same "unforced `option()`, override via `-D`" trick as `CMAKE_C_STANDARD`/`GIT_IO_SELECT` below -- enabling extensions is a pure superset of strict C99, so it can't break anything that compiled under the stricter mode. |
| `NEED_LIBRT=OFF` | `src/CMakeLists.txt` runs `check_library_exists(rt clock_gettime "time.h" NEED_LIBRT)`, which reports a false positive under our toolchain: with `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` (needed elsewhere so CMake never tries to *run* a cross-compiled test binary), the underlying try-compile only has to archive object code into a `.a`, which never actually has to resolve `-lrt` the way a real executable link would -- so the check silently "succeeds" even though AROS's sysroot has no `librt` at all. This stays invisible while only building `libgit2.a` (static libs don't need every symbol resolved until final link -- same reason the `mbedTLS` no-`-lrt` static-only choice works), but breaks the `lg2` *executable* link with `cannot find -lrt`. `check_library_exists` skips its own test entirely if the result variable is already defined, so forcing it via `-D` (same unforced-cache-variable trick as `CMAKE_C_STANDARD`) sidesteps the false positive instead of trying to make `-lrt` actually resolve. |

### A trap to never repeat: `CMAKE_C_FLAGS` clobbering

**Never** pass a bare `-DCMAKE_C_FLAGS="..."` without including
`--sysroot=$SYSROOT` and the mbedTLS user-config flags. CMake only
seeds `CMAKE_C_FLAGS` from the toolchain file's `CMAKE_C_FLAGS_INIT`
if `CMAKE_C_FLAGS` isn't already set. Passing it on the command line
sets the cache variable directly, silently discarding everything the
toolchain file would have added -- this happened twice tonight
(once losing `--sysroot` entirely, causing `pthread.h`/`Threads`
detection to fail with a confusing unrelated error). Always
reconstruct the *full* flag string, as shown in the command above,
never a partial one.

### Quote escaping inside CMAKE_C_FLAGS

The `\\\"mbedtls-user-config.h\\\"` above isn't a typo -- it's
necessary because the string passes through three layers of parsing:
the shell invoking `cmake`, CMake's own string handling, and finally
the shell that executes the generated Makefile's compiler command
line. Using `\"` (single-escaped) loses the backslash somewhere in
that chain and the compiler ends up seeing an unquoted filename,
which fails with `#include expects "FILENAME" or <FILENAME>`. The
quadruple-backslash form survives all three layers intact. If you
ever need to add another quoted define here, follow the same pattern.

## 5. Fetch the CA certificate bundle

Not part of either library build, but needed to actually run any TLS
client (see `hello-tls.c`):

```bash
curl -o cacert.pem https://curl.se/ca/cacert.pem
```

Copy `cacert.pem` alongside any built binary that will connect over
HTTPS on the AROS machine (`PROGDIR:cacert.pem` at runtime).

`lg2` needs to be told this explicitly at runtime -- see
`patches/libgit2-aros-lg2-init.patch`, which calls
`git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS, "PROGDIR:cacert.pem", NULL)`
right after `git_libgit2_init()`. Without it, libgit2 falls back to
`GIT_DEFAULT_CERT_LOCATION`, which is baked in at compile time from
CMake's `CERT_LOCATION` variable -- a path on the *build* machine
(Pop!_OS's `/usr/lib/ssl/certs/ca-certificates.crt`), meaningless on
the AROS target. `streams/mbedtls.c`'s `git_mbedtls_stream_global_init()`
just `stat()`s that path and silently skips loading any CA chain if
it's missing, rather than erroring -- so this fails quietly (TLS
handshakes reject the server certificate) rather than loudly if you
ever remove that runtime call.

## Does libgit2 use mbedTLS's own sockets, or its own? (why DNS was solvable)

Before touching DNS at all, this needed answering, since it decides
the whole approach: does libgit2's mbedTLS integration call mbedTLS's
own `mbedtls_net_connect()`/`_send()`/`_recv()` (the `MBEDTLS_NET_C`
module we deliberately disabled -- see `cmake/mbedtls-user-config.h`),
or does it use its own socket abstraction wrapped by
`mbedtls_ssl_set_bio()`?

```bash
grep -rn "mbedtls_net_\|mbedtls_ssl_set_bio" deps/libgit2/src/libgit2/streams/
```

Answer: libgit2 has its own `git_socket_stream`
(`streams/socket.c`/`.h`), and its mbedTLS TLS stream
(`streams/mbedtls.c`) wraps it via
`mbedtls_ssl_set_bio(st->ssl, st->io, bio_write, bio_read, NULL)` --
`MBEDTLS_NET_C` is never touched. `aros_net.c` isn't used by `lg2` at
all for actual socket I/O (only for opening `bsdsocket.library` once
at startup -- see below); libgit2's own `streams/socket.c` calls
plain `socket()`/`connect()`/`send()`/`recv()` (already proven fine on
AROS via `hello-socket.c`) and, critically, `p_getaddrinfo()` --
which is where DNS came back into scope.

### Why libgit2's own `NO_ADDRINFO` fallback wasn't used

libgit2 already ships a `getaddrinfo()`-free fallback for platforms
without it (`src/util/posix.c`/`posix.h`, enabled via
`-DNO_ADDRINFO`, used today for classic AmigaOS -- see
`if(AMIGA) add_definitions(-DNO_ADDRINFO ...)` in
`src/CMakeLists.txt`). It looked like a natural fit, but it defines
its **own** `struct addrinfo` (with `ai_hostent`/`ai_servent`/
`ai_addr_in` members) under that exact name -- and AROS's own
`<netdb.h>` *already* declares a real, POSIX-shaped `struct addrinfo`
(`ai_family`/`ai_socktype`/`ai_protocol`/`ai_addrlen`/`ai_addr`/
`ai_next`) unconditionally, with no guard around it. Defining
`NO_ADDRINFO` would redefine `struct addrinfo` with an incompatible
member layout -- a hard compile error, not a working alternative.

We also did **not** set `CMAKE_SYSTEM_NAME` to `AMIGA` to pick up that
whole code path automatically: `src/util/CMakeLists.txt` has
`elseif(NOT AMIGA)` guarding the glob that pulls in `src/util/unix/*.c`
(where our already-working `p_mmap`/`p_munmap` -- see
`src/aros_mman.c` -- and other Unix-shaped POSIX glue live). Setting
`AMIGA` would silently drop that entire directory from the build.

### What we did instead: our own getaddrinfo(), backed by gethostbyname()

`gethostbyname()`/`getservbyname()` **are** available on AROS, via the
same classic library-base calling convention that `inet_pton()` and
`select()` needed (`<clib/bsdsocket_protos.h>`, reached through
`<proto/socket.h>`, functions #35/#39 in the `bsdsocket.library`
vector table -- confirmed by reading that header directly, not
guessed). So `src/aros_dns.c` implements `aros_getaddrinfo()` /
`aros_freeaddrinfo()` / `aros_gai_strerror()` using `gethostbyname()`,
and builds AROS's *real* native `struct addrinfo` directly -- avoiding
the `NO_ADDRINFO` collision entirely, and letting libgit2's generic
`socket_connect()` code (which already expects a real POSIX-shaped
`struct addrinfo`) run completely unmodified.
`src/aros-shims/aros_dns.h` `#define`s the plain POSIX names
(`getaddrinfo`/`freeaddrinfo`/`gai_strerror`) onto our
`aros_`-prefixed implementations -- safe because AROS's own headers
never declare those names at all (only the struct, never the
functions), so there's zero collision risk.

**Known limitation:** IPv4 only. `gethostbyname()` has no IPv6
equivalent in AROS's `bsdsocket.library` vector table (no
`gethostbyname2`/`getipnodebyname` either) -- if `hints->ai_family`
requests `AF_INET6` explicitly we return `EAI_FAMILY`; `AF_UNSPEC`
(what libgit2 actually passes) is treated as "give me IPv4." Good
enough for any real-world git host today.

**Also needed:** `git_socket_stream`/`aros_dns.c` both assume
`bsdsocket.library` is already open (`SocketBase` set) -- but unlike
`hello-socket.c`/`hello-tls.c`, `examples/lg2.c` is vendor code we
don't want to hand-edit outside the tracked-patch mechanism. Rather
than a `__attribute__((constructor))` (untested on AROS's hosted
x86_64 ABI, and this project doesn't guess at things it can't verify
-- see the execution boundary note at the top of this doc's revision
history / session notes), `patches/libgit2-aros-lg2-init.patch` adds
one explicit `aros_net_init()` call right before `git_libgit2_init()`
and `aros_net_shutdown()` right after `git_libgit2_shutdown()` --
reusing `aros_net.c`'s already-proven-working
open/close-bsdsocket.library functions, even though `lg2` never calls
`aros_net_connect()`/`_send()`/`_recv()` itself (those three go
unused for `lg2` specifically -- only `aros_net_init()`/`_shutdown()`
and the shared `SocketBase` global are actually exercised).

## AROS's broken `realpath()` (why `lg2 init`/`lg2 clone` failed with "failed to make directory './.'")

AROS's native `realpath()` *is* correctly declared -- libgit2's own
`cmake/DefaultCFlags.cmake` unconditionally sets `-D_GNU_SOURCE`
(confirmed by grepping the actual verbose build log's compiler
invocation), which satisfies the `#if defined(_GNU_SOURCE) ||
(_POSIX_C_SOURCE >= 200112L)` guard around the declaration in AROS's
`<stdlib.h>`. The *implementation* is a different story: an isolated
test program (`tests/test_realpath.c`, built with
`x86_64-aros-gcc -D_GNU_SOURCE`) run natively on AROS hardware/VM
showed `realpath(".")`, `realpath("RAM:")`, and every other input
tried returning `NULL`/`ENOENT`. `realpath(".")` failing is the
damning data point -- the current directory always exists on any real
filesystem, so a correct implementation can never fail on it. This
isn't an edge case in AROS's colon/volume path syntax; the underlying
implementation just doesn't work, for any input.

This explains the `./.`  failure: `git_fs_path_prettify()`
(`src/util/fs_path.c`) calls `p_realpath()`
(`src/util/unix/realpath.c`), a near-trivial wrapper around the system
`realpath()` -- which returned `NULL` for essentially any input,
producing a garbled path that then propagated into a bogus `mkdir()`
call.

### What we did instead: `Lock()`/`NameFromLock()`, not a repaired `realpath()`

Same reasoning as DNS above: rather than repair an implementation of
unknown depth of breakage (and POSIX path-walking semantics don't map
cleanly onto AROS's volume/assign-based filesystem model anyway),
`src/aros_realpath.c` implements `aros_realpath()` on top of
AmigaDOS's own native path-resolution mechanism, available since OS
1.0: `Lock()` a target, then `NameFromLock()` asks the owning
filesystem handler for its fully qualified, canonical name.

**This is the first place this project uses `dos.library`** -- every
prior AROS glue function used `bsdsocket.library`
(`aros_net.c`'s `SocketBase`, opened explicitly via `OpenLibrary()`).
`dos.library` turns out to work differently: reading `proto/dos.h`/
`inline/dos.h` in `$SYSROOT` directly (not assumed) shows
`__DOS_LIBBASE` expands to `__aros_getbase_DOSBase()`, an accessor
AROS's C runtime already provides for every hosted process --
`dos.library` is a core library every AROS process needs from the
moment argv/envp exist, unlike the optional `bsdsocket.library`. No
`OpenLibrary("dos.library", ...)`/`CloseLibrary()` pair is needed;
`Lock()`/`UnLock()`/`NameFromLock()` are usable directly, same as any
other `<proto/dos.h>` call.

**The "." problem:** the input that mattered most (real code, via
`git_repository_discover()`, starts every walk from the current
directory) is also the one AmigaDOS has no native token for --
`Lock()`'s path grammar has no `.` meaning "here"; parsing of
`ACTION_LOCATE_OBJECT` names is up to each filesystem handler
individually, so `Lock(".", ...)` isn't guaranteed to work everywhere.
Confirmed by reading `AddPart()`'s autodoc (`rom/dos/addpart.c`): `/`
is a plain component separator (or, used leading, a "go up one level"
operator), but a component that is literally `.` is just an ordinary,
almost-always-nonexistent filename to any AmigaDOS-compliant
filesystem handler -- never a "stay here" marker the way Unix treats
it. `aros_realpath()` strips every leading `./` from its input, then
treats whatever remains -- `""` or a lone `.` -- as "current
directory, unchanged" by substituting an empty string: the portable,
handler-independent AmigaDOS idiom for that (see `RootDir()` in
AROS's own `rom/dos/lock.c`, which uses exactly this idiom to lock a
volume's root relative to a `DevProc` lock).

The leading-`./`-stripping (not just an exact-match on `"."`) matters
because `lg2 init .` -- initializing the repo in the current directory
-- genuinely produces compound forms at the `p_realpath()` call sites,
not just a bare `.`. Traced byte-for-byte through `git_str_join()` and
`git_fs_path_dirname_r()` (not assumed): `repo_init_directories()`
computes `repo_path = "./.git"` (`git_str_joinpath(".", ".git")`) and,
via `git_fs_path_dirname_r("./.git")` returning `"."` followed by
`git_fs_path_to_dir()`, `wd_path = "./"`. Both reach
`git_fs_path_prettify_dir()` → `p_realpath()` → `aros_realpath()`
directly. Stripping reduces `"./"` to `""` (locks cwd, correct) and
`"./.git"` to `".git"` (an ordinary, by-then-existing directory name,
left unmodified, correct).

**A related but out-of-scope finding:** the same `lg2 init .` trace
surfaces a *different* function constructing an even odder compound
path -- `git_futils_mkdir_relative()` (`src/util/futils.c`), when
walking up to create the not-yet-existing `.git` directory, rejoins
its `relative="./.git"` against `base="."` via
`git_fs_path_join_unrooted()`, producing `"././.git"`, which then goes
straight to `p_mkdir()` (`src/util/futils.c:677`,
`"failed to make directory '%s'"` -- the exact error format originally
reported). This is architecturally unrelated to `aros_realpath()`:
`p_mkdir()`/`p_lstat()` are AROS's *posixc* compatibility layer, a
different, more mature code path than the native AmigaDOS `Lock()`
parser proven broken above -- every other relative-path operation in
this build already depends on `p_lstat(".")`-style calls working
correctly, so a POSIX-conformant `mkdir("././.git")` collapsing
redundant `./` segments is the expected behavior, not a novel risk.
Not fixed here (nothing to fix without evidence it's broken), but
flagged: if `lg2 init .` still fails on-device with a `failed to make
directory` message after this fix, the failing path string in that
message will point at whether this guess was right.

**Call-ordering / GNU-extension check (verified, not assumed):** real
glibc `realpath()` can resolve a path whose final component doesn't
exist yet, as long as the parent does -- a GNU extension. Reading
`git_repository_init_ext()`'s `repo_init_directories()` in
`src/libgit2/repository.c` shows the target directory tree is always
created (`git_futils_mkdir()`, gated on
`GIT_REPOSITORY_INIT_MKPATH`, which `git_repository_init()` -- used by
both `lg2 init` and, transitively, `lg2 clone` -- always sets) *before*
`git_fs_path_prettify_dir()` runs (see the "prettify both directories
now that they are created" comment directly above that call). So
`aros_realpath()` only ever needs to resolve paths that already exist
on disk for these two commands specifically -- it does not replicate
the not-yet-existing-leaf GNU extension. The other `p_realpath()` call
sites in libgit2 were checked individually, not just assumed to follow
the same pattern:

- `src/libgit2/clone.c`'s `create_and_configure_origin()` (local-clone
  source detection, line 342) only reaches `p_realpath()` inside
  `if (!git_net_str_is_url(url) && git_fs_path_root(url) < 0 &&
  git_fs_path_exists(url))` -- `git_fs_path_exists(url)` being true is
  a hard precondition, so this call site can never hand
  `aros_realpath()` a not-yet-existing path. It's also the one place
  `url` could plausibly be `.` (`lg2 clone . dest`), which the `.` ->
  `""` substitution above already handles correctly.
- `src/libgit2/repository.c`'s ceiling-directory resolution
  (`find_ceiling_dir_offset()`, line 496) only ever runs against
  directories named in `GIT_CONFIG_CEILING_DIRECTORIES`-style config,
  and already tolerates `p_realpath()` failing (`continue`s past a
  `NULL` result rather than erroring) -- existing-or-skipped either
  way.
- `src/util/fs_path.c`'s `git_fs_path_find_dir()` (line 809) falls back
  to `git_fs_path_dirname_r()` when `p_realpath()` fails, so it
  doesn't depend on the not-yet-existing-leaf extension either.

All four call sites, not just the two `init`/`clone`-directory ones,
confirmed safe under an `aros_realpath()` that only resolves paths
that already exist.

**Known limitation:** buffer size. `p_realpath()`'s signature carries
no length (matching real POSIX `realpath()`), so `aros_realpath()`
assumes the caller-supplied buffer is at least `GIT_PATH_MAX` (4096,
`include/git2/common.h`) bytes -- true of every current call site, and
the same implicit contract any platform's `realpath()` relies on.

Integration: `src/aros-shims/aros_realpath.h` declares
`aros_realpath()` and `#define`s the plain `realpath` name onto it,
included into `src/util/unix/realpath.c` via
`patches/libgit2-aros-realpath.patch` (patch #6) -- same
macro-redirection pattern as `aros_dns.h`'s `getaddrinfo()` shim, and
deliberately *not* a direct edit of `p_realpath()`'s call site: that
function already implements every bit of the NULL-buffer/allocator
contract generically on top of a plain `realpath(pathname, resolved)`
call, so redirecting just the name means none of that logic needs
duplicating.

## AROS's POSIX calls don't understand `./` either (why `lg2 init peo` -- a fresh, named directory, not `.` -- still failed)

The `aros_realpath()` fix above was necessary but not sufficient. On
real hardware, `lg2 init peo` (a plain, non-existent, non-`.` directory
name) still failed with `failed to make directory './.'`. Tracing that
exact command byte-for-byte through both libgit2's `git_str_join()`/
`git_fs_path_dirname_r()` and AROS's own `mkdir()`-internals Unix->
AmigaDOS path translator (`compiler/crt/posixc/__upath.c`'s
`__path_normalstuff_u2a()`, which -- read directly, not assumed --
*does* correctly collapse a leading `./`) found no construction site
for a literal `"./."` for this input. That translator only runs when
`PosixCBase->doupath` is true, which is only ever set by
`__posixc_nixmain()` -- an opt-in startup path, not a language default.

Whether that startup path is actually linked for a normal `lg2`
invocation can't be determined by cross-compiling or reading source:
`nm` on our own binaries shows every posixc call (`__mkdir_CrtBase_libreq`,
`__realpath_CrtBase_libreq`, etc.) resolved through a runtime `CrtBase`
indirection -- none of `compiler/crt/posixc/*.c` is statically linked
into anything we build. The answer depends entirely on whichever
C-runtime library is installed on the target AROS system, invisible
from this checkout.

**Empirically confirmed on real AROS hardware** (AROS One, VirtualBox):
a minimal, standalone `mkdir("./someprobe", 0755)` -- plain libc,
nothing of libgit2's or aros_realpath.c's in the way -- fails with
`ENOENT`. `doupath` translation is *not* active for this project's
binaries in this environment. Every AROS POSIX call that takes a path
is reaching AmigaDOS's native parser completely unmodified, and (per
the "." problem above) that parser has no `./` concept at all -- this
was never a `realpath()`-specific quirk, it's true of `mkdir()`,
`stat()`, `open()`, and everything else that takes a path.

**The fix:** rather than special-case `./`-handling in each function
that needs it (fragile against future libgit2 bumps, and anything
agit's own frontend adds later would have to remember the same rule
by hand), the stripping happens once, at the narrowest common choke
point for the vast majority of path-taking POSIX calls:
`deps/libgit2/src/util/unix/posix.h`, where `p_lstat`, `p_stat`,
`p_mkdir`, `p_unlink`, `p_rmdir`, `p_access`, `p_chdir`, `p_readlink`,
`p_symlink`, `p_link`, `p_chmod`, and `p_utimes` are already all thin
macros to raw libc calls. `p_open`/`p_creat`/`p_rename` live in the
cross-platform `src/util/posix.c` as real functions instead of macros,
so they get the same treatment individually in their function bodies.
Both use one shared helper, `aros_strip_dotslash()`
(`src/aros_path_shims.c`/`src/aros-shims/aros_path_shims.h`) -- a
pure pointer-advance, safe to use directly as a macro argument, that
`aros_realpath.c` now also calls instead of duplicating its own copy
of the same loop.

This is *not* quite a universal choke point -- three direct libc calls
in libgit2 bypass the `p_*` layer entirely, found by grepping for raw
POSIX calls outside `unix/posix.[ch]`:

- `src/util/fs_path.c`'s two `opendir()` calls (directory iteration,
  e.g. for `git status`/tree-walking) -- a real, likely-reachable gap,
  so these were changed to call a newly-added `p_opendir()` macro
  instead (added to `unix/posix.h`, and to `win32/posix.h` too, purely
  so a hypothetical Windows build doesn't silently break -- Windows
  already macro-redirects bare `opendir` to its own `git__opendir()`
  via `win32/dir.h`, so `p_opendir(p)` there just resolves through
  that existing indirection).
- `src/util/unix/process.c`'s `chdir()` in a forked-child pre-exec
  path (subprocess spawning for credential helpers) -- this project is
  SSH/subprocess-free by design (`USE_SSH=OFF`, no credential helper
  subprocesses), so very likely dead code for `lg2`/agit, but it
  exists in the vendor source and isn't wrapped.
- `src/libgit2/streams/mbedtls.c`'s two `stat()` calls on the CA cert
  path -- not a practical concern here, since this project always
  hardcodes `PROGDIR:cacert.pem` (already AmigaDOS-native, never
  `./`-prefixed) via `patches/libgit2-aros-lg2-init.patch`.

**Deliberately strip-only, no collapse:** unlike `aros_realpath()`,
none of these wrapped macros substitute a fully-stripped result (`""`
or a lone `.`) with the Lock()-specific `""` idiom -- that idiom is
verified correct only for `Lock()` (via `RootDir()`'s usage in AROS's
own `rom/dos/lock.c`), and whether `CreateDir()`/`Examine()`/`Open()`
etc. treat an empty name the same way is a separate, unverified
question. A fully-stripped path is passed through as a plain `.` for
these calls instead.

Integration: `patches/libgit2-aros-path-normalize.patch` (patch #7)
touches `src/util/unix/posix.h`, `src/util/posix.c`,
`src/util/fs_path.c`, and `src/util/win32/posix.h`. Despite two of
those files (`posix.c`, and `posix.h` indirectly via `select.patch`)
already being touched by `patches/libgit2-aros-select.patch`, patch
order is still verified not to matter (empirically re-tested: applying
`path-normalize.patch` before `select.patch` and vice versa both
succeed) -- the two patches' hunks land in different, non-overlapping
regions of the same files.

### `SOCK_CLOEXEC` stripping (patch #8)

`streams/socket.c` passes the BSD extension flag `SOCK_CLOEXEC`
(`SOCK_STREAM | 0x10000000`) to `socket()` -- a common pattern on
Linux and modern BSDs. AROS's `bsdsocket.library` does not support
`SOCK_CLOEXEC` in the type argument and returns -1 with no errno set,
silently failing socket creation. The fix strips `SOCK_CLOEXEC` from
the type argument under `#ifdef __AROS__`, restoring the plain
`SOCK_STREAM` that the AROS library expects.

**Confirmed on real AROS hardware**: `socket(SOCK_STREAM | SOCK_CLOEXEC)`
fails immediately; `socket(SOCK_STREAM)` succeeds. See
`tests/test_aros_sock_cloexec.c` for the isolated reproducer.

### PAT credential callback for `lg2 push` (patch #9)

`examples/push.c` already exists and already calls `cred_acquire_cb`
(the interactive-prompt callback from `examples/common.c`). For
AROS's headless/VM environment there is no interactive terminal, and
the project's auth model is PAT-only by design. Patch #9 swaps in
`aros_cred_acquire_cb` (defined in `src/aros_cred.c`, linked via the
cmake patch) which reads the token from the `AGIT_PAT` environment
variable or `PROGDIR:agit.config` and calls
`git_credential_userpass_plaintext_new()` -- no interactive fallback.

See `src/aros_cred.c`, `src/aros_cred.h`, and `agit.config.example`
for the implementation and config-file format.

### AROS path-root detection (patch #10)

`git_fs_path_root()` (`src/util/fs_path.c`) recognizes only POSIX
`/`-prefixed paths and DOS `<letter>:` drive letters. AROS paths like
`RAM Disk:repo/assets/foo.txt` have a volume-name colon at a variable
position (not just [0]-[1]), so the function returned -1 (not rooted),
causing `git_fs_path_join_unrooted()` to prepend the workdir base a
second time. Patch #10 adds an `#elif defined(__AROS__)` branch that
scans for the first colon before any `/` and treats it as the volume
root, fixing the path duplication in checkout subdirectory creation.

### Dynamic branch refspec from HEAD (patch #11)

`examples/push.c` had `refs/heads/master` hardcoded as the refspec.
Modern GitHub uses `main` as the default branch. Patch #11 replaces
the hardcoded string with `git_repository_head()` to derive the
branch name from the current HEAD reference — it works for any branch
name, not just `master`/`main`.

**All six of the above, confirmed fixed on real AROS hardware**: paths
now resolve correctly (e.g. to `"RAM Disk:bengt"`), and `lg2 init`/
`lg2 clone` get past directory creation and path resolution entirely.

## AROS's ownership check always fails (`repository path '...' is not owned by current user`)

The next error hit on-device, past both fixes above: `git_repository_open_ext()`
(reached by both `lg2 init` and `lg2 clone`) validates that the repo
and worktree directories are "owned by the current user" --
`git_fs_path_owner_is()` (`src/util/fs_path.c`) compares `stat()`'s
`st_uid` against `geteuid()`. This check was added upstream for a real
multi-user threat (CVE-2022-24765): a shared or admin-writable
directory containing a repo with hostile hooks/config, opened
unknowingly by a different user on the same machine. AROS has no such
threat model -- it's a single-user OS, the same reasoning already used
to disable `MBEDTLS_TIMING_C` (see `cmake/mbedtls-user-config.h`).

**Root cause, read from AROS's actual source, not assumed:** AROS's
`stat()`/`lstat()` (`compiler/crt/posixc/__stat.c`'s `__id_a2u()`)
deliberately maps AmigaDOS's `fib_OwnerUID == 0` -- the default for
any file that was never given an explicit multi-user owner, true of
virtually everything on a typical AROS filesystem, including
`RAM Disk:` -- to Unix uid `65534` ("nobody"), specifically so an
unassigned AmigaDOS owner is never mistaken for Unix root. Meanwhile
`getuid()`/`geteuid()` (`compiler/crt/posixc/getuid.c`/`geteuid.c`)
only ever change via explicit `setuid()`/`seteuid()` calls
(`compiler/crt/posixc/setuid.c`/`seteuid.c`), which `lg2` never makes,
so they stay at their zero-initialized default, `0`. `0 != 65534`:
this comparison can never succeed on AROS as configured here, for any
file, regardless of who "owns" it in any meaningful sense. Not a bug
in AROS or in this build -- AROS's mapping is a deliberate, sensible
choice on its own terms -- just an assumption (comparable, meaningful
per-file ownership) that doesn't hold on a single-user OS with no real
ownership model at all.

**The fix:** `git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0)`,
called in `examples/lg2.c` right after the CA-cert-path call (same
`#ifdef __AROS__` block, via `patches/libgit2-aros-lg2-init.patch`).
This is libgit2's own first-class, maintained toggle for exactly this
check -- `GIT_OPT_SET_OWNER_VALIDATION` sets
`git_repository__validate_ownership` (`src/libgit2/settings.c`), the
same flag `git_repository_open_ext()` checks before ever calling
`validate_ownership()` (`src/libgit2/repository.c`) -- not a bypass or
a suppressed error, the documented, intended way to disable this
specific check on platforms where its threat model doesn't apply
(`include/git2/common.h`'s `opts()` documentation lists it explicitly).

## Why we patch this way (submodules + patches/ + shims, never edit vendor code)

- `deps/libgit2` and `deps/mbedtls` are pinned git submodules. We
  never commit modifications inside them -- that would make bumping
  to a newer upstream tag a manual, error-prone re-patching exercise,
  and would make `git -C deps/libgit2 status` lie about being clean.
- Configuration-level AROS deviations for mbedTLS go in
  `cmake/mbedtls-user-config.h`, using mbedTLS's own officially
  documented `MBEDTLS_USER_CONFIG_FILE` mechanism -- zero source
  changes needed.
- Source-level changes to libgit2 (eleven so far: `posix.c` needing
  `<proto/socket.h>` for `select()`/`WaitSelect()`; `streams/socket.c`
  and `src/util/posix.h` each needing one `#include` line for our own
  `getaddrinfo()`/`getpwuid_r()`/`getsid()`/`pread()`/`pwrite()`
  shims; `src/util/unix/realpath.c` needing one `#include` line for
  our `realpath()` replacement (see "AROS's broken `realpath()`"
  below); `src/util/unix/posix.h`, `src/util/posix.c`,
  `src/util/fs_path.c`, and `src/util/win32/posix.h` needing the
  `./`-stripping wrapper described in "AROS's POSIX calls don't
  understand `./` either" below; `streams/socket.c` needing to strip
  `SOCK_CLOEXEC` from the `socket()` type argument (AROS's
  `bsdsocket.library` rejects the flag);   `examples/push.c` needing
  to replace the interactive-prompt credential callback with
  `aros_cred_acquire_cb` (reads PAT from `AGIT_PAT` or
  `PROGDIR:agit.config`) and replace the hardcoded `refs/heads/master`
  refspec with a dynamic lookup from HEAD; `src/util/fs_path.c` needing AROS
  volume-name root detection for paths like `RAM Disk:repo/foo`
  (colons not at a fixed position); `examples/lg2.c` needing
  three small `#ifdef __AROS__` blocks to open `bsdsocket.library` and
  set the CA cert path; and `examples/CMakeLists.txt` needing to link
  our glue code into `lg2`) are each captured as a tracked `.patch`
  file in `patches/`, applied explicitly as a build step (see step 2),
  never silently baked into the submodule checkout.
- Missing OS facilities entirely (no `sys/mman.h`, no
  `getaddrinfo()`/`getpwuid_r()`/`getsid()`/`pread()`/`pwrite()` at
  all) get a header shim under `src/aros-shims/` plus a real,
  documented, honestly-limited implementation in `src/aros_*.c` --
  linked in at final *application* link time (`lg2`, eventually
  `agit`), not needed to build `libgit2.a` itself.
- Broken-not-missing OS facilities (declared, present in the library,
  but demonstrably incorrect at runtime -- `realpath()` is the one
  found so far, see "AROS's broken `realpath()`" above) get the same
  shim-header-plus-`src/aros_*.c`-implementation treatment, just
  redirected via a macro of the *same* POSIX name instead of filling a
  missing declaration, since AROS's own (broken) declaration is
  already there.
- A behavioral gap that spans *every* path-taking POSIX call, not one
  function (AROS's Unix->AmigaDOS `./`-handling being inactive for
  this project's binaries -- see "AROS's POSIX calls don't understand
  `./` either" above) gets one shared helper
  (`src/aros_path_shims.c`/`src/aros-shims/aros_path_shims.h`) reused
  from as many `#ifdef __AROS__` redirection points as the vendor code
  actually needs (`unix/posix.h`'s macros, `posix.c`'s `p_open`/
  `p_creat`/`p_rename`), rather than one shim per function -- the
  point of the shared helper is exactly to avoid re-solving (or
  re-forgetting) the same problem at each call site.

## Known limitations (read before relying on any of this for anything serious)

- **`src/aros_mman.c`** is `malloc()` + `read()`, not a real memory
  mapping: no lazy paging, no shared-page benefit, `MAP_SHARED`
  writes are not written back. Fine for libgit2's actual use
  (read-only pack/idx file access on small hobby repos); would be a
  real problem for huge repositories or anything expecting genuine
  `mmap()` semantics.
- **`src/aros_entropy.c`** falls back to a weak clock+stack+counter
  mixer when the CPU lacks `RDRAND` (true on the primary AROS
  development machine, an i5-2400 -- Sandy Bridge, one generation
  too early for `RDRAND`). Sufficient to make TLS session keys
  unpredictable against passive network eavesdropping in a hobby
  project; not resistant to a determined adversary. See the comment
  in that file for details.
- **DNS resolution (`src/aros_dns.c`) is IPv4-only.** Real hostnames
  work for `lg2`/`libgit2` (via `gethostbyname()`), but there is no
  AROS `bsdsocket.library` equivalent for IPv6 resolution -- see "What
  we did instead" above. `aros_net_connect()` (used by `hello-tls.c`,
  not by `lg2`) still takes a dotted-quad IP string only; it was never
  extended to take hostnames since libgit2 has its own connection path
  that doesn't go through it.
- **`src/aros_posix_shims.c`**: `getpwuid_r()` always reports "no
  entry found" (AROS has no real passwd database to look up -- see the
  `NOTIMPL` comment already in the real `<pwd.h>`); `getsid()` always
  returns `-1` (no session-ID concept on AROS -- only used as one
  ingredient in libgit2's non-cryptographic RNG seed mixing, harmless);
  `pread()`/`pwrite()` are plain `lseek()`+`read()`/`write()`+restore
  (correct given `THREADSAFE=OFF` -- no concurrent thread can move the
  fd position between the seek and the read/write).
- **`src/aros_realpath.c`** assumes the caller's buffer is at least
  `GIT_PATH_MAX` (4096) bytes (no length is available to check against
  -- see "AROS's broken `realpath()`" above), and only resolves paths
  that already exist on disk (no GNU-extension not-yet-existing-leaf
  support) -- verified sufficient for every current libgit2 call site,
  but would need extending if a future libgit2 bump adds a
  `p_realpath()` call ahead of directory creation.
- **`src/aros_path_shims.c`'s `aros_strip_dotslash()` only strips a
  leading `./`** -- it does not handle embedded `/./` in the middle of
  a path, `../`, or collapse a fully-stripped result to anything
  special (that's each wrapped macro's own decision -- see "AROS's
  POSIX calls don't understand `./` either" above). Sufficient for
  every path libgit2's own construction logic has been traced
  producing so far; not a general-purpose path canonicalizer.
- **Three direct libc call sites bypass the `p_*` normalization layer
  entirely**: `src/util/unix/process.c`'s `chdir()` (subprocess
  spawning, dead code for this SSH-free project) and
  `src/libgit2/streams/mbedtls.c`'s two `stat()` calls on the CA cert
  path (not `./`-prefixed in this project's configuration either way)
  -- see "AROS's POSIX calls don't understand `./` either" above for
  the full list and reasoning. Not fixed, since neither is currently
  reachable with a `./`-prefixed path for `lg2`/agit's actual usage.
- **`p_poll()` `revents` operator-precedence bug** (`src/util/posix.c`,
  lines 363-366): the `select()`-based `p_poll()` implementation's
  `revents` computation:
  ```c
  fds[i].revents = 0 |
      FD_ISSET(fds[i].fd, &read_fds) ? POLLIN : 0 |
      FD_ISSET(fds[i].fd, &write_fds) ? POLLOUT : 0 |
      FD_ISSET(fds[i].fd, &except_fds) ? POLLPRI : 0;
  ```
  Due to `|` binding tighter than `?:`, this parses as a chained
  ternary (`(0|read) ? POLLIN : (0|write) ? POLLOUT : (0|except) ? POLLPRI : 0`)
  rather than three independent ORs. Consequence: at most one of
  POLLIN/POLLOUT/POLLPRI is ever set (the first matching in read-then-
  write-then-except order), POLLERR and POLLHUP are never set, and
  multi-fd polls compute incorrect results. Currently unreachable:
  `connect_with_timeout()` (the sole `p_poll()` call site in the
  blocking `lg2` code path) only polls a single fd for POLLOUT, which
  the broken expression happens to compute correctly for that specific
  case. Noted here so any future code that adds a non-blocking connect
  or multi-fd poll knows to fix this first.
- **No SSH.** PAT-over-HTTPS only, by design (see main README's
  "What this is NOT" section).

## Troubleshooting checklist

If a build fails partway through, check these in order -- they cover
every failure mode hit so far:

1. **Did you `rm -rf` the build directory before reconfiguring?**
   CMake caches are sticky; a stale cache from a previous
   `-DCMAKE_C_FLAGS` invocation can silently ignore your new flags.
   Always start clean when changing configure-time flags.
2. **Does the reported compiler command line actually contain
   `--sysroot`?** Grep the verbose build log
   (`cmake --build . -j1 -- VERBOSE=1` or `gmake VERBOSE=1`) for
   `sysroot`. If it's missing, you clobbered `CMAKE_C_FLAGS` (see
   above).
3. **Is the error a bare `#error` inside vendor code mentioning
   "Unix and Windows"?** That's the AROS-isn't-recognized pattern
   that hit mbedTLS four times tonight (entropy, timing, ms_time,
   net_sockets) and libgit2's poll detection. Check whether the
   symbol involved is actually needed for our use case before writing
   a real implementation -- several of these were safely disabled
   outright.
4. **Is it `error: expected ',' or ';' before 'int'` inside an AROS
   header (`ctype.h`, `stdio.h`, etc.)?** That's the C90-vs-`inline`
   problem -- check `CMAKE_C_STANDARD` is actually being honored
   (`grep std= <verbose log>`).
5. **Is it a macro collision** (e.g. `"shutdown" requires 2
   arguments`)? That means an AROS header defining a
   function-style macro under a common name
   (`shutdown`/`connect`/`close`/etc, via the classic AROS
   library-base calling convention) got pulled in somewhere it
   shouldn't have been. Prefer a surgical fix (patch one file) over a
   global `-include`, which is what caused this exact failure once
   tonight.
6. **Is it `error: 'asm' undeclared` inside `<inline/exec.h>`**
   (`AROS_LIBREQ` or similar)? That's `CMAKE_C_EXTENSIONS=OFF` making
   `asm` unavailable under strict `-std=c99` -- add
   `-DCMAKE_C_EXTENSIONS=ON` (see the flag table above). Only shows up
   once something includes `<proto/exec.h>` (e.g. `aros_net.c`'s
   `OpenLibrary()`/`CloseLibrary()`), so it can appear well into an
   otherwise-successful build.
7. **Is it `cannot find -lrt` at the final executable link** (not a
   `.a` build)? That's `NEED_LIBRT`'s `check_library_exists` false
   positive under `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` -- add
   `-DNEED_LIBRT=OFF` (see the flag table above). Building `libgit2.a`
   alone never surfaces this; it only appears when linking a real
   executable against it (`lg2`, `agit`).
8. **Compiling a new AROS glue `.c` file that libgit2 doesn't know
   about at all** (`error: aros_something.h: No such file or
   directory`, or the file never gets built)? Check it's both (a)
   reachable via one of the `-I` flags in `CMAKE_C_FLAGS` if it's a
   header, and (b) actually listed in
   `patches/libgit2-aros-lg2-cmake.patch`'s `SRC_EXAMPLES` append if
   it's a `.c` file meant to link into `lg2` -- CMake's own `file(GLOB
   *.c)` in `examples/CMakeLists.txt` only sees files that already
   exist in `examples/`, never our own `src/aros_*.c`.
9. **Does `lg2 init`/`lg2 clone` fail with `git_error_last()` reporting
   `failed to resolve path`?** That's AROS's broken native
   `realpath()` -- see "AROS's broken `realpath()`" above. Confirm
   with `tests/test_realpath.c` (already declared broken as of this
   writing; if it now succeeds, AROS's `realpath()` may have been
   fixed upstream and `aros_realpath.c` may no longer be needed) before
   assuming `aros_realpath.c`/`patches/libgit2-aros-realpath.patch`
   themselves regressed.
10. **Does it instead fail with `failed to make directory '%s'`**
    (any path, not just `./.`)? That's `p_mkdir()`/`p_open()`/etc.
    reaching AmigaDOS's native parser with an unstripped `./` prefix
    -- see "AROS's POSIX calls don't understand `./` either" above.
    Confirm with `tests/test_aros_mkdir.c` (a minimal, raw
    `mkdir("./name", ...)` probe, no libgit2 involved -- already
    confirmed failing with `ENOENT` on this project's AROS One/
    VirtualBox test environment) before assuming
    `patches/libgit2-aros-path-normalize.patch` regressed. If a
    *different* path than the ones covered by that patch shows up in
    the error message, it's evidence of one of the three known direct-
    call gaps (see that section) actually being reachable -- check
    which call site the failing path traces back to before extending
    the patch.

## Testing lg2 on AROS

Builds happen only on this Pop!_OS machine (cross-compilation only --
nothing here ever runs VirtualBox/QEMU or drives an AROS guest
directly). Testing on the actual target is a manual hand-off.

**Status as of this writing:** the `realpath()` fix and the `./`-prefix
path-normalization fix are both confirmed working on real AROS
hardware (paths resolve correctly, e.g. to `"RAM Disk:bengt"`, and
directory creation/path resolution no longer fail). The next error hit
was the ownership-validation check (see "AROS's ownership check always
fails" above), now also fixed -- **not yet re-confirmed on-device.**
Steps 0's `test_realpath`/`test_aros_realpath`/`test_aros_mkdir`
probes don't need re-running unless something in that layer regressed;
the open item is steps 1-5 below with the ownership fix included.

0. **Isolate both fixes first**, the same way the bugs themselves were
   isolated -- don't jump straight to the full `lg2` binary. Rebuild
   `test_realpath` (the original, showing AROS's native `realpath()`
   is still broken), `test_aros_realpath` (exercising `aros_realpath()`
   directly), and `test_aros_mkdir` (a minimal, raw
   `mkdir("./<name>", ...)` probe -- plain libc, nothing of libgit2's
   or `aros_realpath.c`'s in the way):
   ```bash
   x86_64-aros-gcc --sysroot=$SYSROOT -D_GNU_SOURCE tests/test_realpath.c -o test_realpath
   x86_64-aros-gcc --sysroot=$SYSROOT -I src -I src/aros-shims \
       tests/test_aros_realpath.c src/aros_realpath.c src/aros_path_shims.c -o test_aros_realpath
   x86_64-aros-gcc --sysroot=$SYSROOT tests/test_aros_mkdir.c -o test_aros_mkdir
   ```
   Copy all three onto the AROS machine and run them.

   **Important: `test_aros_mkdir` is expected to keep printing `FAILED`
   even with every fix in this doc applied.** It calls the raw system
   `mkdir()` directly -- the same call already empirically confirmed
   failing with `ENOENT` on this project's AROS One/VirtualBox test
   environment (see "AROS's POSIX calls don't understand `./` either"
   above). `patches/libgit2-aros-path-normalize.patch` does not, and
   cannot, fix AROS's own `mkdir()`; it only makes *libgit2's internal*
   `p_mkdir()` (and friends) strip the `./` themselves before ever
   calling that same broken `mkdir()`. So `test_aros_mkdir` failing is
   the expected, unfixable-by-us baseline, not a regression -- the
   real evidence for whether the libgit2-side fix works is `lg2 init`
   actually succeeding in step 3 below, not this probe.

   Expect `test_realpath` to still show every input failing (confirms
   the native `realpath()` bug is still present and `aros_realpath()`
   is genuinely a workaround, not a coincidental fix), and
   `test_aros_realpath` to resolve `.`, `./.`, `./`, `RAM:`,
   `RAM Disk:`, and the self-created `./<tmpdir>` compound case
   successfully (with `RAM:lennart`/`lennart` failing with `ENOENT`
   only if that file genuinely doesn't exist in the test environment).
   `test_aros_realpath` also prints a raw `Lock()`/`UnLock()`
   diagnostic (bypassing `aros_realpath()`'s own `./`-stripping
   entirely) for `.`, `./.`, and `./` -- expect these to FAIL too (same
   reasoning as `test_aros_mkdir`: it's the unfixable native behavior
   the workaround exists to route around). Report all of this back
   verbatim, not just the normalized `aros_realpath()` results --
   they're the direct evidence for what's actually happening on this
   target, not assumed.
1. Build `build-libgit2/examples/lg2` per steps 1-4 above.
2. Copy four files onto the AROS machine (VM or native), in the same
   directory: `build-libgit2/examples/lg2`, `cacert.pem`
   (project root), and (if not already there from step 0)
   `test_aros_realpath` and `test_aros_mkdir`. Use `genisoimage -R`
   (the `-R` Rock Ridge flag is mandatory -- without it, AROS sees
   mangled 8.3-style filenames) to build a transfer ISO if going
   through VirtualBox, e.g.:
   ```bash
   genisoimage -R -o transfer.iso build-libgit2/examples/lg2 cacert.pem test_aros_realpath test_aros_mkdir test_realpath
   ```
   then mount that ISO as a VirtualBox optical drive and copy the
   files to, e.g., `RAM:` or a Work: partition from AROS's Shell.
3. From an AROS Shell, in the directory holding all the files, in a
   fresh empty subdirectory (`lg2 init .` writes into the current
   directory):
   ```
   Protect lg2 +E
   lg2 init newrepo
   lg2 init .
   lg2 clone https://github.com/octocat/Hello-World Hello-World
   ```
   Both `lg2 init newrepo` (a fresh, single-level, `.`-free name) and
   `lg2 init .` (self-init, producing the compound `"./.git"`/`"./"`
   forms) are meaningful tests now, for different reasons: `newrepo`
   is the case that was *originally* reported broken (`lg2 init peo`,
   traced and reconstructed in "AROS's POSIX calls don't understand
   `./` either" above) -- its `p_mkdir("./newrepo", ...)`-equivalent
   call reaches AmigaDOS's native parser with a plain, unstripped `./`
   prefix that `patches/libgit2-aros-path-normalize.patch` now strips.
   `lg2 init .` exercises the *other*, `aros_realpath()`-specific fix
   (the compound dot forms reaching `p_realpath()`, not `p_mkdir()`)
   -- see "AROS's broken `realpath()`" above. Both need to work; they
   were two separate bugs found via two separate rounds of empirical
   testing, not one. Past directory creation and path resolution, both
   commands (and `lg2 clone`) also exercise the ownership-validation
   fix (see "AROS's ownership check always fails" above) -- watch
   specifically for `repository path '...' is not owned by current
   user` disappearing; that error means `GIT_OPT_SET_OWNER_VALIDATION`
   either didn't take effect or the reasoning behind it was wrong.
   `octocat/Hello-World` is a tiny, stable, public GitHub repo -- good
   for fast iteration; avoid cloning this repo or anything large for
   the same reason.
4. Verify with `lg2 log` inside the resulting `Hello-World` directory,
   and/or inspect `Hello-World/.git` actually contains real pack/ref
   data -- not just "the command didn't crash." For `lg2 init`, check
   both `newrepo/.git` and the `.git` created by `lg2 init .` were
   actually created with the expected structure (`HEAD`, `objects/`,
   `refs/`, etc), not a partial/corrupt directory left behind by a
   `p_mkdir()` failure partway through.
5. Report back exactly what printed (or the AROS equivalent of a
   crash/Guru meditation) -- paste it back rather than summarizing, so
   root-causing doesn't have to guess at what actually happened. If
   `lg2 init`/`lg2 clone` still fails, note exactly which path string
   appears in the error -- that's the fastest way to tell whether it's
   a known, already-covered call site regressing, or one of the three
   direct-call gaps (`opendir`'s two callers -- now covered --,
   `process.c`'s `chdir()`, or `mbedtls.c`'s `stat()`) turning out to
   be reachable after all.

**Before it can even attempt a connection**, AROS's TCP/IP stack
(Poseidon/Miami/Roadshow) needs a working nameserver configured -- NAT
alone (as used for `hello-tls.c`'s hardcoded-IP test) doesn't imply
DNS is set up. If `lg2 clone` fails immediately with a DNS-shaped
error (`aros_gai_strerror` reporting "name or service not known"),
check the network stack's nameserver configuration before assuming
the code is wrong.
