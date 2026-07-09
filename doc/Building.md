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
`aros_posix_shims.c` -- wired in via
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
| `-I.../src/aros-shims` | Provides `sys/mman.h`, `aros_dns.h`, and `aros_posix_shims.h` -- see the deviation table entries below for what each covers. All are pure declaration headers; the real implementations (`src/aros_*.c`) are linked in later, only at final application (lg2/agit) link time, never needed to build `libgit2.a` itself. |
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

## Why we patch this way (submodules + patches/ + shims, never edit vendor code)

- `deps/libgit2` and `deps/mbedtls` are pinned git submodules. We
  never commit modifications inside them -- that would make bumping
  to a newer upstream tag a manual, error-prone re-patching exercise,
  and would make `git -C deps/libgit2 status` lie about being clean.
- Configuration-level AROS deviations for mbedTLS go in
  `cmake/mbedtls-user-config.h`, using mbedTLS's own officially
  documented `MBEDTLS_USER_CONFIG_FILE` mechanism -- zero source
  changes needed.
- Source-level changes to libgit2 (five so far: `posix.c` needing
  `<proto/socket.h>` for `select()`/`WaitSelect()`; `streams/socket.c`
  and `src/util/posix.h` each needing one `#include` line for our own
  `getaddrinfo()`/`getpwuid_r()`/`getsid()`/`pread()`/`pwrite()`
  shims; `examples/lg2.c` needing three small `#ifdef __AROS__` blocks
  to open `bsdsocket.library` and set the CA cert path; and
  `examples/CMakeLists.txt` needing to link our glue code into `lg2`)
  are each captured as a tracked `.patch` file in `patches/`, applied
  explicitly as a build step (see step 2), never silently baked into
  the submodule checkout.
- Missing OS facilities entirely (no `sys/mman.h`, no
  `getaddrinfo()`/`getpwuid_r()`/`getsid()`/`pread()`/`pwrite()` at
  all) get a header shim under `src/aros-shims/` plus a real,
  documented, honestly-limited implementation in `src/aros_*.c` --
  linked in at final *application* link time (`lg2`, eventually
  `agit`), not needed to build `libgit2.a` itself.

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

## Testing lg2 on AROS

Builds happen only on this Pop!_OS machine (cross-compilation only --
nothing here ever runs VirtualBox/QEMU or drives an AROS guest
directly). Testing on the actual target is a manual hand-off:

1. Build `build-libgit2/examples/lg2` per steps 1-4 above.
2. Copy two files onto the AROS machine (VM or native), in the same
   directory: `build-libgit2/examples/lg2` and `cacert.pem`
   (project root). Use `genisoimage -R` (the `-R` Rock Ridge flag is
   mandatory -- without it, AROS sees mangled 8.3-style filenames) to
   build a transfer ISO if going through VirtualBox, e.g.:
   ```bash
   genisoimage -R -o transfer.iso build-libgit2/examples/lg2 cacert.pem
   ```
   then mount that ISO as a VirtualBox optical drive and copy both
   files to, e.g., `RAM:` or a Work: partition from AROS's Shell.
3. From an AROS Shell, in the directory holding both files:
   ```
   Protect lg2 +E
   lg2 clone https://github.com/octocat/Hello-World Hello-World
   ```
   (`octocat/Hello-World` is a tiny, stable, public GitHub repo --
   good for fast iteration; avoid cloning this repo or anything large
   for the same reason.)
4. Verify with `lg2 log` inside the resulting `Hello-World` directory,
   and/or inspect `Hello-World/.git` actually contains real pack/ref
   data -- not just "the command didn't crash."
5. Report back exactly what printed (or the AROS equivalent of a
   crash/Guru meditation) -- paste it back rather than summarizing, so
   root-causing doesn't have to guess at what actually happened.

**Before it can even attempt a connection**, AROS's TCP/IP stack
(Poseidon/Miami/Roadshow) needs a working nameserver configured -- NAT
alone (as used for `hello-tls.c`'s hardcoded-IP test) doesn't imply
DNS is set up. If `lg2 clone` fails immediately with a DNS-shaped
error (`aros_gai_strerror` reporting "name or service not known"),
check the network stack's nameserver configuration before assuming
the code is wrong.
