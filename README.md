# agit

A minimal git client (clone, pull, commit, push) for AROS x86_64
(ABIv11), built on top of [libgit2](https://libgit2.org/) with
[mbedTLS](https://www.trustedfirmware.org/projects/mbed-tls/) as the
TLS layer. Authentication is via Personal Access Token (PAT) over
HTTPS
# agit

A minimal git client for AROS x86_64 (ABIv11) -- `clone`, `add`,
`commit`, and `push`, straight from your AROS Shell.

```
1.RAM Disk:> agit clone https://github.com/octocat/Hello-World Hello-World
Cloning into 'Hello-World'...
remote: Enumerating objects: 13, done.
...
Clone complete.
```

## Status

**0.1 -- early, but genuinely working.** `clone`, `add`, `commit`, and
`push` have all been tested end-to-end against real GitHub
repositories, running on real AROS x86_64 hardware and in VirtualBox.


## Installation

1. Download the latest release zip and extract it -- you should end
   up with three files: `agit`, `cacert.pem`, and
   `agit.config.example`.
2. Copy all three into a directory of your choice (a fresh drawer on
   `Work:` or wherever you keep your tools works fine).
3. Rename `agit.config.example` to `agit.config` and fill it in --
   see the next two sections for exactly what goes in it.

## Getting a GitHub Personal Access Token (PAT)

This is the "password" agit uses to talk to GitHub on your behalf.
Takes about a minute:

1. On any computer, log into GitHub and go to **Settings** ->
   **Developer settings** -> **Personal access tokens** ->
   **Fine-grained tokens**.
2. Click **Generate new token**.
3. Under **Repository access**, choose **Only select repositories**
   and pick the repo(s) you want agit to be able to touch. (Avoid
   granting access to everything -- there's no reason agit needs more
   than it's actually going to use.)
4. Under **Permissions** -> **Repository permissions**, find
   **Contents** and set it to **Read and write** (this is the only
   permission agit needs).
5. Set an expiration date that makes sense for you -- tokens can
   always be regenerated later.
6. Click **Generate token**, and **copy it immediately** -- GitHub only
   shows you the value once. It'll look something like:
   ```
   github_pat_11AADI...a very long string...
   ```

Treat this token like a password. Anyone who has it can write to
whatever repositories you granted it access to.

## Getting the certificate file (cacert.pem)

agit needs to know which certificate authorities to trust when it
connects to GitHub over HTTPS. A `cacert.pem` is included in the
release zip, sourced from
[curl's official CA bundle](https://curl.se/docs/caextract.html) (the
same trusted bundle used by countless other tools).

Certificate authorities change over time, so if agit ever starts
failing to connect with a certificate-related error, download a fresh
copy from <https://curl.se/ca/cacert.pem> and replace the file
alongside `agit`.

## Setting up agit.config

One small text file holds your identity and your token. Create
`agit.config` in the same directory as the `agit` binary, containing:

```
GIT_USER_NAME=Your Name
GIT_USER_EMAIL=you@example.com
GITHUB_PAT=github_pat_11AADI...your-actual-token-here
```

- `GIT_USER_NAME` / `GIT_USER_EMAIL` are what shows up as the author
  on commits you make with agit.
- `GITHUB_PAT` is the token from the previous step.

One key per line, no quotes, no extra spaces around the `=`. That's
the whole format.

**This file contains your token in plain text.** Don't share it, don't
commit it into a git repository, don't post it anywhere. If you ever
suspect it's leaked, go back to GitHub and revoke/regenerate the
token -- it takes seconds.

## Using agit

All four commands are run from an AROS Shell, in whatever directory
makes sense for the operation:

**Clone a repository:**
```
1.Work:Projects> agit clone https://github.com/someuser/somerepo somerepo
```

**Stage a changed or new file:**
```
1.Work:Projects/somerepo> agit add path/to/file.txt
```

**Commit staged changes:**
```
1.Work:Projects/somerepo> agit commit -m "Describe what changed"
```

**Push your commits back to GitHub:**
```
1.Work:Projects/somerepo> agit push
```

That's the whole workflow: clone once, then add/commit/push as many
times as you like.

## Troubleshooting

- **"failed to connect" during clone/push:** check that DNS actually
  works on your AROS system (`Ping github.com` is a quick sanity
  check). If that works but agit still fails, your `cacert.pem` might
  be stale or missing -- see above.
- **Commit succeeds but push fails with an authentication error:**
  double check `agit.config` -- no stray spaces, the token hasn't
  expired, and the token's repository permissions actually cover the
  repo you're pushing to.
- **Nothing in `agit.config` seems to be read at all:** make sure the
  file is named exactly `agit.config` (not `agit.config.txt` or
  similar) and sits in the same directory as the `agit` binary itself.

## For developers

Building agit from source, the full list of AROS-specific
compatibility fixes this project required, and the reasoning behind
each one, are documented in [`docs/BUILDING.md`](docs/BUILDING.md).
[`ROADMAP.md`](ROADMAP.md) covers what's planned next.

## License

GPLv2 (matches libgit2's "GPLv2 with linking exception"; mbedTLS is
Apache 2.0-licensed and compatible).
