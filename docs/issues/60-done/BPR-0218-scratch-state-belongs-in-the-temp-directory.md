---
schema: "implement-rfc.card.v1"
id: "BPR-0218"
title: "Scratch state belongs in the temp directory, not the repository"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# Scratch state belongs in the temp directory, not the repository

Fourteen directories of live application state — 198 files — were committed to
the repository by four separate commits. AGENTS.md and CONTRIBUTING.md both
forbid committing transient inspection artifacts, and nobody did it
deliberately.

## Measured

```
tracked directories   14
tracked files        198
  .sqlite3            54      .sqlite3-wal  54      .sqlite3-shm  54
  .lock                6      pack .idx      3      pack .pack     3
  plain text          24
```

| Commit | Subject | Files added |
|---|---|---|
| `1b38cb5` | an attended launch that holds the window open (BPR-0186) | 120 |
| `66f8f47` | compose the workbench host, and gate composition permanently (BPR-0190) | 42 |
| `a9ecaf3` | one liveness token for every poller, not one per crash (BPR-0217) | 24 |
| `d4d9e61` | close BPR-0186, a person can inspect the composition | 12 |

Nothing outside those directories names them: `git grep bpr-public-path` over
the tracked tree, excluding the directories themselves, returns no match. No
fixture, test, CMake `WORKING_DIRECTORY`, manifest, or document depends on the
path, so untracking them cannot break anything.

## The mechanism

`github_public_path_fixture.cpp:1028` constructed its scratch root from the
**relative** template `"bpr-public-path-XXXXXX"`. A relative template resolves
against the process working directory, so an application launched from the
repository root wrote its application data — SQLite databases with their WAL and
SHM sidecars, a bare Git cache, process-boundary locks — into the repository.

`QTemporaryDir` removes itself on clean destruction, which is why this was
invisible for so long: every ordinary run cleaned up after itself. What survived
was exactly the runs that died abruptly. Three of the four commits above are
crash runs, including `a9ecaf3` — the BPR-0217 fix, which leaked twenty-four
files of its own while investigating the segfault it was fixing.

## Fixed, 2026-08-08

The template is now absolute:
`QDir::temp().filePath(QStringLiteral("bpr-public-path-XXXXXX"))`. That matches
the shape the repository already uses for an identifiable absolute temp prefix
(`macos_keychain_native_tests.cpp:150-152`,
`linux_secret_service_native_tests.cpp:186`); every other `QTemporaryDir` in the
tree is a bare default construction, which Qt already resolves under
`QDir::tempPath()`.

The fourteen directories are untracked (`git rm -r --cached`), deleted from the
working tree as the runtime litter they are, and `/bpr-public-path-*/` is
ignored.

### Both directions, measured

The defect only shows on an abrupt death, so a clean run proves nothing. Both
runs below are the same command from the repository root, killed with `SIGKILL`
after four seconds — the same shape as the BPR-0215/0217 segfaults.

| | repository root | system temp dir |
|---|---|---|
| Before the fix | **one new directory** (`bpr-public-path-ikgRsM`) | nothing |
| After the fix | nothing | **one new directory** (`bpr-public-path-xzqjGs`) |

And on a clean exit after the fix, the run completing normally through
`APPEARANCE_APPLIED:#7fb0e8:#ffffff:from-profile`: zero directories in the temp
dir, zero new ones in the repository. `QTemporaryDir` removes its own directory
when it is destroyed properly, which was always true and was never the problem.

### What a full suite run leaves behind, and where

After the fix, `ctest --preset dev` (140/140) leaves **zero** `bpr-public-path-*`
directories in the repository and **one** in the system temp directory. That one
is attributable: `attended_launch_contract.py:143-148` deliberately `terminate()`s
and then `kill()`s the held-open application, and a killed process cannot run
`~QTemporaryDir`. Its contents are a bare `state/` directory, consistent with a
run ended before the Git cache was built.

This is the intended end state, not a leftover defect. Abrupt deaths still strand
scratch state — nothing can stop that — but it is now stranded in the directory
the operating system reaps instead of in the repository. Before the fix, that
same directory is the one that would have been committed.

### The ignore rule, shown non-vacuous

```
$ git check-ignore -v bpr-public-path-PROBE/state/v1/x.sqlite3
.gitignore:18:/bpr-public-path-*/	bpr-public-path-PROBE/state/v1/x.sqlite3
```

A file named `bpr-public-path-notadir` is still reported by `git status`, so the
trailing slash keeps the rule to directories and the leading slash keeps it at
the repository root. The rule is deliberately redundant with the fix: the fix
stops the directory being created here at all, and the rule stops a future
relative-template regression from being committed a fifth time.

## Invariants checked before moving the path

The root is consumed as `StorageRoot` by `make_installed_git_cache` and the
SQLite epoch catalog, and `installed_git_cache.cpp`'s `canonical_existing()`
requires it to stay absolute and already canonical for the life of the cache.
That invariant is unaffected: the fixture stores the result of
`QFileInfo::canonicalFilePath()` (`:1035`), not the raw template, so it was
satisfied by construction before this change and still is.

The multi-phase epoch tests do not exercise this branch at all — they pass
`--epoch-fixture-root`, which routes to the explicit-root branch at `:1004-1026`
and never constructs a `QTemporaryDir`.

## What must not be done instead

Adding the directories to `.gitignore` and leaving the relative template in
place. That hides the litter in this checkout while the application keeps
writing live SQLite state into whatever directory a person happened to launch it
from.

Deleting the directories without untracking them. They are in history; the point
is that they stop being part of the working tree.
