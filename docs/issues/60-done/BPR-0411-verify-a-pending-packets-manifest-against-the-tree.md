---
schema: "implement-rfc.card.v1"
id: "BPR-0411"
title: "Verify a pending packet's source manifest against the tree"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# Verify a pending packet's source manifest against the tree

`designated-live-review-publication-v002` bound 31 source files by digest. Six
did not match the tree, and three did not match at the commit that introduced
the packet — it was **stale at birth**. It then survived a regeneration, a status
line reading "pending capability-critical review", and a board that listed it as
an operator decision. Nothing detected it until a reviewer recomputed the
manifest by hand. Its successor, `designated-live-review-publication-v003`, was
regenerated from the committed tree and its manifest does match — verified twice,
once during preparation and once independently by its own review — but that is
two people being careful, not a gate.

[[BPR-0406]] said filing this check was "a separate card, not this one" and then
did not file it. The v003 review found the omission, which is the second time a
promised follow-up has been caught by a reviewer rather than by the board.

## What already exists, and what it does not cover

`scripts/verify_documentation.py` already re-hashes bound artifacts — but only
for **answered** packets. `frozen_packets` (`:1011-1042`) globs
`*/validation-record.json` and fails when a bound artifact's digest or byte
length no longer matches. A packet with no validation record is not frozen, and a
*pending* packet never has one.

So the window in which a packet is most dangerous — after it is written, while an
operator is being asked to approve it, before any verdict exists — is exactly the
window nothing checks.

## Required final state

- A pending packet that ships a `source-manifest.txt` has that manifest verified
  against the working tree by `scripts/verify_documentation.py`, and a mismatch
  fails the gate.
- The check is the recompute the reviewer ran by hand: read each `path=sha256`
  line, hash the file, compare. Roughly four lines of Python over a file the
  repository already keeps.
- A packet whose manifest is absent is not failed by this — only a manifest that
  disagrees with the tree it claims to describe.
- A missing file named by a manifest is a mismatch, not a crash.
- The documentation contract suite gains a case in both directions: a manifest
  that agrees passes, one that disagrees fails.

## Why this is worth a gate rather than diligence

The failure is invisible by construction. A stale manifest reads exactly like a
fresh one — same shape, same 64-hex values, same byte count — and the only way to
tell is to hash 31 files. Three sessions and one reviewer looked at v002 before
anyone did that.

It is also cheap to check and expensive to miss: the artifact it guards is an
approval that authorizes a real public mutation, and the whole point of binding
digests is that the operator is approving *those* bytes.

## Non-goals

- Do not verify anything for an answered packet. `frozen_packets` already owns
  that, and re-checking a frozen artifact against a moved tree would fail every
  packet the moment the tree advances, which is the opposite of what freezing
  means.
- Do not extend this to the capsule binaries or the inventory. Those are build
  outputs, are not present in a fresh checkout, and a gate that reads `build/`
  fails on any machine that has not built — the same reason [[BPR-0403]]
  deliberately left the dev-binary hold unenforced.
- Do not touch anything under `src/`. Nothing here needs to.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
```

The negative control is the one that matters: with the check in place, mutate
one byte of a file named by a pending packet's manifest and confirm the gate
fails. Without that, this card proves only that the check runs.

## Result — 2026-08-20

`verify_pending_packet_manifests` is in `scripts/verify_documentation.py` and
runs with the rest of the gate. It is the four-line recompute the v003 reviewer
ran by hand: read each `path=sha256` line, hash the file, compare.

### The first draft was wrong, and the gate caught it

It keyed on `frozen_packets` — "if the packet is not frozen, check it" — and
immediately failed `designated-live-review-publication-v001`, which is
**invalidated**, describes a past tree deliberately, and never earns a validation
record because nobody answers a retired packet.

So "not frozen" is not "pending". The check now keys on the status line through
the existing `PACKET_PENDING_RE`, and a test pins that distinction so the same
mistake cannot come back as a refactor.

Coverage today: v001 skipped, v002 skipped, **v003 checked**.

### Verification matrix

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| the gate accepts a matching manifest | the live tree | `python3 scripts/verify_documentation.py --root .` | v003's 31 entries are checked against the working tree | `DOCUMENTATION_OK` | this commit |
| **the gate fails a real mutation** | v003's own manifest | append one line to `src/app/CMakeLists.txt`, rerun bare | this is the card's stated negative control; without it the card proves only that the check runs | **exit 1**, naming the file and both digests | this commit |
| …and recovers | same | restore the file, rerun | a gate that stays red after repair is not usable | `DOCUMENTATION_OK` | this commit |
| a retired packet is not checked | `test_ignores_a_retired_packets_manifest` | a superseded packet whose named file has moved on | this is the first draft's bug, pinned | pass | this commit |
| a missing file is a mismatch, not a crash | `test_rejects_a_pending_manifest_naming_a_missing_file` | unlink a named file | the card required a refusal rather than a traceback | pass | this commit |
| the suite grew rather than shifted | `tests/scripts/verify_documentation_tests.py` | `python3 tests/scripts/verify_documentation_tests.py` | 98 cases | **98, OK**, was 94 | this commit |
| nothing under `src/` was touched | Git | `git status --short` | the dev binary must not move | `scripts/` and `tests/` only | this commit |

### What it deliberately does not do

It does not check answered packets — `frozen_packets` owns those, and re-checking
frozen evidence against a moved tree would fail every packet the moment the tree
advances, which is the opposite of freezing. It does not read `build/`, so it
works in a fresh checkout, for the same reason [[BPR-0403]] left the dev-binary
hold unenforced.

### The window this closes

v002 was written, marked pending, listed on the board as an operator decision,
and carried through a regeneration while six of its 31 entries did not match the
tree and three had never matched. Every one of those steps had a human or a gate
look at it. None of them hashed the files, because a stale manifest looks exactly
like a fresh one.
