---
schema: "implement-rfc.card.v1"
id: "BPR-0058"
title: "Open the Linux epoch catalog through its retained directory"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0044","BPR-0047","BPR-0057"]
completed: "2026-08-01"
---

# Open the Linux epoch catalog through its retained directory

Repair the target-native Linux catalog startup path exposed by BPR-0049. The
catalog deliberately addresses SQLite through `/proc/self/fd/<directory>/…`
so a retained, privately opened directory—not a replaceable pathname—anchors
bootstrap and live database opens. SQLite's generic `SQLITE_OPEN_NOFOLLOW`
flag rejects that descriptor path because `/proc/self` and its file-descriptor
entries are symlinks, so the real application currently fails closed before
rendering on Ubuntu 26.04.

Keep `SQLITE_OPEN_NOFOLLOW` on every platform. On Linux, preserve the existing
descriptor-anchored catalog spelling through a private VFS, retain every
directory/file identity and permission check, and do not weaken the retained
ancestor-chain or one-link private-file contract.

## Completion evidence

- Fresh Linux SQLite/catalog tests cover bootstrap, reopen, failure cleanup,
  retained-directory identity, and hostile symlink substitution.
- The real GitHub.com fixture starts through XCB with no forced Qt platform,
  style, or platform theme, renders the epoch control, and exits cleanly.
- Runtime tracing shows SQLite opens through `/proc/self/fd/<retained-dir>`;
  no direct replaceable catalog path is substituted.
- Focused macOS catalog, composition, and public-path tests still pass;
  architecture/documentation/workflow-absence gates and `git diff --check`
  remain clean.

## Result

The Linux catalog now registers one private, randomly named SQLite VFS per
live catalog. Its full-path callback admits only one file name beneath an
exact `/proc/self/fd/<decimal-descriptor>/` prefix, while the underlying Unix
VFS retains `SQLITE_OPEN_NOFOLLOW`/`O_NOFOLLOW`. The root, state, and version
directory descriptors and VFS registration live until after the SQLite handle
closes. macOS, Windows, and noncatalog SQLite paths retain their prior rules.

## Evidence

- Ubuntu 26.04 aarch64, Linux 7.0.0-28, GCC 15.2, and Qt 6.10.2: fresh selected
  targets built cleanly; `sqlite_epoch_catalog_storage_contract` and
  `sqlite_epoch_catalog_behavior_contract` passed 2/2. The storage test now
  renames the live version directory, installs an owner-private replacement
  catalog canary, performs a real catalog operation, and proves the canary is
  byte-for-byte unchanged.
- Target-native `strace -f -e trace=%file` shows bootstrap, main, WAL, SHM, and
  journal `openat` calls using `/proc/self/fd/5/...` with `O_NOFOLLOW`; no
  canonical replaceable catalog path is opened by the production adapter.
- The exact real GitHub.com application fixture passed under unforced XCB and
  the target-selected `QFusionStyle`, reporting `PUBLIC_PATH_OK`,
  `GITHUB_FIXTURE_OK:github.com`, and `RENDER_OK`.
- On macOS, the focused architecture, nested boundary, SQLite storage,
  SQLite behavior, composition, and GitHub.com public-path group passed 6/6
  in 215.65 seconds. Documentation validation, the zero-workflow scan, and
  `git diff --check` pass.
