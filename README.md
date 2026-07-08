# agit

A minimal git client (clone, pull, commit, push) for AROS x86_64
(ABIv11), built on top of [libgit2](https://libgit2.org/) with
[mbedTLS](https://www.trustedfirmware.org/projects/mbed-tls/) as the
TLS layer. Authentication is via Personal Access Token (PAT) over
HTTPS

**Status: pre-alpha.** mbedTLS builds cleanly for AROS (2026-07-08).
Raw TCP sockets proven working on AROS One/VirtualBox the same day.
TLS handshake, libgit2 integration, and the actual client are still
ahead.

This is deliberately **not** a full git implementation:

- No SSH (no modern client to build on for AROS)
- No `git log`/`blame`/`rebase`/merge tooling -- just enough to move
  commits between your AROS machine and a remote

## Architecture

```
libgit2 (v1.9.4, git logic, object model, pack files)
   |
   +-- mbedTLS (v3.6.6 LTS, TLS layer, replaces OpenSSL)
   |      |
   |      +-- aros_net.c (our own bsdsocket.library wrapper --
   |            mbedTLS's own net_sockets.c doesn't work on AROS,
   |            see "Known AROS deviations" below)
   |
   +-- src/ (agit-specific code: PAT handling, CLI)
```

Core principle: **no upstream library is patched in its own tree.**
All AROS deviations live in `cmake/mbedtls-user-config.h` (mbedTLS's
official mechanism for configuration overrides) or in our own glue
code such as `aros_net.c`. That means `deps/libgit2` and
`deps/mbedtls` can be bumped to new upstream tags without losing our
changes or needing manual re-patching.

## License
GPLv2 (matches libgit2's "GPLv2 with linking exception"; mbedTLS is
Apache 2.0-licensed and compatible).
