# agit

A minimal git client (clone, pull, commit, push) for AROS x86_64
(ABIv11), built on top of [libgit2](https://libgit2.org/) with
[mbedTLS](https://www.trustedfirmware.org/projects/mbed-tls/) as the
TLS layer. Authentication is via Personal Access Token (PAT) over
HTTPS

**Status: pre-alpha.** mbedTLS and libgit2 both build cleanly for
AROS x86_64. Raw TCP sockets and a full TLS 1.3 handshake (real
handshake + HTTP request against github.com, decrypted response) are
proven working on AROS One/VirtualBox. `lg2` (libgit2's own example
CLI) cross-compiles and links into a real AROS ELF executable against
our `libgit2.a`, with real hostname-based DNS (`gethostbyname()`-backed
`getaddrinfo()`, IPv4-only) wired in -- see `doc/Building.md` for the
full architecture writeup. **Not yet verified**: an actual `lg2 clone`
run on AROS hardware/VM (this project cross-compiles only; on-device
runs are a manual hand-off step -- see `doc/Building.md`'s "Testing
lg2 on AROS"). agit's own polished frontend hasn't been started yet.

This is deliberately **not** a full git implementation:

- No SSH (no modern client to build on for AROS)
- No `git log`/`blame`/`rebase`/merge tooling -- just enough to move
  commits between your AROS machine and a remote

## Architecture

```
libgit2 (v1.9.4, git logic, object model, pack files)
   |
   +-- its own git_socket_stream (streams/socket.c) -- plain
   |     socket()/connect()/send()/recv(), wrapped by
   |     mbedtls_ssl_set_bio() for TLS. NOT mbedTLS's own
   |     net_sockets.c (MBEDTLS_NET_C stays disabled -- see
   |     doc/Building.md's "Does libgit2 use mbedTLS's own
   |     sockets, or its own?").
   |
   +-- mbedTLS (v3.6.6 LTS, TLS layer, replaces OpenSSL)
   |
   +-- agit's AROS glue (src/, linked into the final binary only,
   |     never into libgit2.a/libmbedtls.a themselves):
   |       aros_net.c    -- opens bsdsocket.library once at startup
   |                        (also a standalone TCP wrapper, used by
   |                        hello-socket.c/hello-tls.c, not by lg2's
   |                        own socket I/O)
   |       aros_dns.c    -- getaddrinfo() via gethostbyname(), IPv4 only
   |       aros_entropy.c -- mbedtls_hardware_poll() (RDRAND or weak fallback)
   |       aros_time.c   -- mbedtls_ms_time() (second resolution)
   |       aros_mman.c   -- honest malloc+read mmap()/munmap() emulation
   |       aros_posix_shims.c -- getpwuid_r()/getsid()/pread()/pwrite()
   |
   +-- src/ (agit-specific code: PAT handling, CLI -- not started yet;
         examples/lg2 from the libgit2 submodule is today's integration
         test, see doc/Building.md)
```

Core principle: **no upstream library is patched in its own tree.**
All AROS deviations live in `cmake/mbedtls-user-config.h` (mbedTLS's
official mechanism for configuration overrides) or in our own glue
code such as `aros_net.c`. That means `deps/libgit2` and
`deps/mbedtls` can be bumped to new upstream tags without losing our
changes or needing manual re-patching.

AROS-specific libgit2 patches live in `patches/` (submodule content is
never committed dirty). See `doc/Building.md` step 2 for the exact,
ordered list of `git apply` commands and step-by-step build
instructions.

## TODO

- Verify `lg2 clone` actually completes end-to-end on real AROS
  hardware or VirtualBox (build/link is done and cross-verified; the
  on-device run itself is the next open item -- see
  `doc/Building.md`'s "Testing lg2 on AROS")
- Confirm DNS resolution (`src/aros_dns.c`, `gethostbyname()`-backed)
  actually succeeds against a real nameserver in the target AROS
  network config, not just at the link level
- Write agit's own frontend (PAT handling, a real CLI or GUI) on top
  of the now-working libgit2 integration -- `lg2` was only ever the
  integration test
- No SSH, ever (by design -- see "What this is NOT" above)
- `src/aros_entropy.c`'s fallback entropy source is intentionally weak
  on non-RDRAND CPUs (see the honesty warning in that file) --
  revisit if this project ever needs stronger guarantees than "hobby
  project passive-eavesdropping resistance"

## License
GPLv2 (matches libgit2's "GPLv2 with linking exception"; mbedTLS is
Apache 2.0-licensed and compatible).
