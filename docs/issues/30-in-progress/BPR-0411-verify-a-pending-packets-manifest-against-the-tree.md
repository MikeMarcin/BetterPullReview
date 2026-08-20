---
id: BPR-0411
title: Verify a pending packet's source manifest against the tree
status: In Progress
rfc: RFC-BPR-0013
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
