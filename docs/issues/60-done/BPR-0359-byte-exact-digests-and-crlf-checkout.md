---
id: BPR-0359
title: Byte-exact digests do not survive a CRLF checkout
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# Byte-exact digests do not survive a CRLF checkout

`.gitattributes` said exactly one thing: `* text=auto`. Git therefore stores
every text file with LF and hands Windows a CRLF working tree. Five checked-in
files have their exact SHA-256 pinned by a constant computed over the LF blob,
and two gates read those files from the working tree. On Windows they read
different bytes, so the pin failed. On macOS and Linux the same pin held, which
is why nothing caught it until a Windows host existed.

This is not cosmetic. Four of the five files are the designated live-mutation
canary surface, and their combined digest is part of the approval identity that
[[BPR-0235]] built to make one live lane authorizable and nothing else. A digest
that depends on which operating system checked the repository out is not an
identity.

## Measured

| File | pinned (LF blob) | working tree (CRLF) |
|---|---|---|
| `tests/network_isolation/designated_live_mutation_canary.cpp` | `e0de0ae165…` | `39d64e5ea9…` |
| `tests/network_isolation/designated_live_mutation_canary.hpp` | `9eebcf6551…` | `8a23e01711…` |
| `tests/network_isolation/designated_live_mutation_child.cpp` | `2cab794cd7…` | `5f0fbfa6c9…` |
| `tests/network_isolation/designated_live_mutation_child.hpp` | `e7247b22e2…` | `a7f5cc4fd4…` |
| `packaging/icons/better_pull_review.svg` | `5aa4a6d31d…` | `d21193f7f4…` |

The two readers are `cmake/VerifyArchitectureBoundaries.cmake:1150`
(`file(SHA256)`, four individual pins plus the combined
`fb77b7ca2a80bea0…` surface digest) and
`tests/scripts/app_icon_assets_tests.py:96` (`hashlib.sha256` against
`CANONICAL_SVG_SHA256`).

A repo-wide sweep for `file(SHA256`, `file(MD5`, `hashlib.sha256`, and
`hashlib.md5` found no sixth case: the other hits digest downloaded
dependencies under the CPM cache, which checkout never rewrites.

## Resolution

`.gitattributes` now pins those five paths with `text eol=lf`, so the working
tree matches the blob on every platform and the existing constants stay
correct.

The alternative was to normalize line endings inside each checker before
hashing. That was rejected: it would have redefined the pin from "these exact
bytes" to "these bytes modulo a transformation", which is precisely the
property the canary surface digest exists to deny. Fixing the checkout keeps
the digest byte-exact and makes it platform-independent at the same time.

`git add --renormalize` plus a fresh checkout of the five paths brought the
working tree into line; no constant changed.

## Result

```
sha256(worktree) == sha256(blob) == pinned constant     5 of 5 files
architecture_boundaries                                 Passed  (was Failed)
application_icon_assets_contract                        Passed  (was Failed)
py -3 scripts/verify_documentation.py --root .          DOCUMENTATION_OK
```

`architecture_boundaries` had failed with `Designated live canary surface file
is not exact: designated_live_mutation_canary.cpp`, which is the honest message
for what happened and named the right file.

## Not in scope

Two further digest failures were seen on Windows and are **not** shown to be
this defect, so they are not claimed here. `architecture_boundary_contract`
fails at the same `VerifyArchitectureBoundaries.cmake:1152` message but against
fixture files the test itself writes, so its cause is whatever that test writes
rather than what git checked out. `github_contract_fixture_security` reports a
capability-manifest digest mismatch and an unexpected fixture-tree entry.
[[BPR-0347]] records both as open Windows failures with their measured
symptoms.
