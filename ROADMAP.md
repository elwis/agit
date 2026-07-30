# Roadmap: from lg2 to agit 0.1

## Where we are

`lg2` (libgit2's own minimal example CLI) has proven the entire
underlying stack works on real AROS x86_64 hardware: DNS, TCP, TLS 1.3,
path handling, commit creation, and PAT-authenticated push. Eleven
patches got us here (see `docs/BUILDING.md`).

**Critical fact for scoping this work: almost all of the hard-won
fixes live in the CORE library (`libgit2.a`), not in `lg2` itself.**
Patches to `src/util/posix.h`, `src/libgit2/streams/socket.c`,
`src/util/fs_path.c` etc. are compiled into `libgit2.a` and will
apply automatically to ANY binary linked against it -- including a
brand new `agit` binary that has never heard of `lg2`. Only a handful
of patches specifically touched `examples/*.c` (lg2's own source,
`deps/libgit2/examples/`) -- those need their *logic* reimplemented in
our own code, not the patches themselves reapplied.

Specifically:
- `patches/libgit2-aros-cred.patch` (PAT credential callback) and
  `patches/libgit2-aros-lg2-init.patch` (branch-detection in push,
  `aros_net_init()` call, cert path setup) touched `examples/lg2.c` /
  `examples/push.c`. These behaviors need to exist in agit's own code
  -- but the actual implementations already exist as reusable,
  already-working `src/aros_*.c` files. This is mostly wiring, not new
  logic.
- Everything else (path normalization, ownership validation,
  SOCK_CLOEXEC, realpath, DNS) is already inside `libgit2.a` itself
  and needs zero extra work to benefit agit.

## Scope for 0.1 -- deliberately minimal

**In scope:**
- `agit clone <url> <dest>`
- `agit add <path>`
- `agit commit -m "<message>"`
- `agit push`
- Identity (`user.name`/`user.email`) and PAT read from the **same**
  `PROGDIR:agit.config` file already established -- add two new keys
  (`GIT_USER_NAME=`, `GIT_USER_EMAIL=`) alongside the existing
  `GITHUB_PAT=`. No new config mechanism, no `agit config` subcommand
  yet. One file, one format, documented in `agit.config.example`
  (already exists -- just extend it).
- Distribution: a zip containing the binary, `cacert.pem`, and a
  README with manual copy/`Protect +E` instructions. No installer
  script. No icon/Archives packaging polish yet.

**Explicitly OUT of scope for 0.1** (do not implement, do not design
around -- keep the architecture simple, not extensible-for-later):
- Installer script
- Zune GUI frontend
- `agit config` as an interactive subcommand
- Any command beyond the four listed above (no `log`, `status`,
  `pull`, `branch`, etc. -- those can reuse `lg2` for now if needed at
  all)
- Auto-updating `cacert.pem`
- Multi-repo config, credential storage beyond the single flat file

The goal of 0.1 is proving "agit, not lg2, is what a user actually
runs" -- a real, minimal, own-branded binary, not a feature-complete
client.

## Milestones

### M1: `agit.c` skeleton + config loading
- New `src/agit.c` (or `src/main.c` -- pick one, be consistent with
  existing naming), a from-scratch CLI entry point. Does NOT live
  under `deps/libgit2/examples/` -- this is agit's own source, no
  patch mechanism needed, builds via its own small `CMakeLists.txt`
  or is added to the existing build setup (propose which before
  implementing).
- Command dispatch: `agit <clone|add|commit|push> [args...]`
- Config loading: read `PROGDIR:agit.config`, parse `GITHUB_PAT=`,
  `GIT_USER_NAME=`, `GIT_USER_EMAIL=`. Reuse/extend `aros_cred.c`'s
  existing parser rather than writing a second one -- it already
  reads this exact file format.
- No git operations yet -- just prove the binary builds, links
  against `libgit2.a` + our glue, and can print parsed config values.

### M2: `agit clone`
- Wire `git_clone()` with the credential callback from `aros_cred.c`
  (already proven working, just called directly instead of via
  `examples/push.c`'s indirection).
- Test: clone a real repo with nested directories, same test used to
  verify the fs_path_root fix.

### M3: `agit add` + `agit commit`
- `git_index_add_bypath()` / `git_index_write()` for add.
- Signature built directly from the config-loaded `GIT_USER_NAME`/
  `GIT_USER_EMAIL` (not from `git_signature_default()`, which reads
  system config we don't have set up -- this is why `lg2 commit`
  needed manual `config` calls every session; agit should never need
  that once config loading works).
- Test: commit creation, verify via re-reading the commit object, not
  just "the command didn't crash."

### M4: `agit push`
- Reuse the HEAD-branch-detection logic already fixed in
  `patches/libgit2-aros-lg2-init.patch` -- port that logic into
  `agit.c` directly (it's a small, already-debugged piece of code,
  copy the *logic*, not the patch mechanism).
- Test: full clone → add → commit → push cycle with the new `agit`
  binary, end to end, same test repo as the `lg2` milestone.

### M5: Packaging for 0.1
- `README.md` (or a dedicated `PACKAGING.md`) with manual install
  instructions: copy `agit`, `cacert.pem` to a directory, create
  `agit.config` from the example, `Protect agit +E`.
- Zip the deliverable. No AROS Archives submission yet -- that's
  post-0.1, once the installer/GUI work (explicitly deferred above)
  is also done.

## What to tell DeepSeek to start with

Give it M1 only, as a single focused task. Do not hand over the whole
roadmap as one task -- same discipline as every prior session in this
project: small, verifiable steps, propose-before-implement, one
milestone at a time. See the accompanying task brief.
