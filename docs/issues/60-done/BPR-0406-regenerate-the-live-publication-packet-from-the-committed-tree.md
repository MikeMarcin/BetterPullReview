---
id: BPR-0406
title: Regenerate the live publication packet from the committed tree
status: Done
rfc: RFC-BPR-0008
depends_on: []
---

# Regenerate the live publication packet from the committed tree

[[BPR-0256]]'s capability-critical review returned BLOCKING, and the finding is
that `designated-live-review-publication-v002` does not bind this repository.
Six of its 31 manifest entries mismatch at HEAD, three of them mismatched at
`1842216` — the commit that introduced the packet — and the capsule was built
from `b04b43c`, which is not an ancestor of `main`.

The mechanism is not implicated. All three counterexamples that invalidated v001
were re-derived against live source and are genuinely repaired. This card
rebuilds the artifacts so the packet describes something real.

## Why this is doable here despite the dev-binary hold

The hold protects `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review`.
The capsule is a **separate** tests-disabled Release graph at
`build/bpr0256-release`, which [[BPR-0385]] unblocked and [[BPR-0388]] and
[[BPR-0389]] repaired. Building it does not relink the `dev` application, so the
three attended packets bound to those bytes are unaffected.

Verify that rather than trusting it: record the `dev` digest before and after,
and treat any change as a failure of this card.

```sh
shasum -a 256 build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
# expect 7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93
```

## Required final state

- A capsule built from the current committed tree, with `source-manifest.txt`
  regenerated so all 31 entries reproduce byte-exactly at the head it names.
  A utility computed the would-be HEAD manifest digest as
  `b2e2c2d8f005e62ff997ec4e4dd87f48af6da0decdcf64a48b54746131ef4a81`; that value
  is a starting expectation, not evidence, and must be reproduced rather than
  copied.
- `approval-subject.txt` re-rendered through the real
  `designated_review_approval_subject` renderer, binding the new supervisor and
  inventory digests and the existing replay namespace, loaded read-only.
- `docs/verification/reviews/designated-live-review-publication-v003/` created
  as the live packet. v002's status line stays `INVALIDATED BY
  CAPABILITY-CRITICAL REVIEW` and its bytes are not otherwise edited.
- The eleven-selector matrix re-run **at the regenerated digest**, not reused
  from v002's run. The point of the finding is that the tested binaries and the
  bound binary were different things.
- The `dev` application binary is byte-identical before and after.
- A fresh capability-critical review of v003. The one that produced this card
  does not transfer.

## Three defects to fix while regenerating

These are the review's non-blocking findings, and each would otherwise survive
regeneration unchanged.

1. **Disclose the 15-minute authority lifetime.**
   `src/application/src/designated_review_authority_bootstrap.cpp:46` sets it and
   `src/application/src/review_mutation_application_host.cpp:82-86,122-125`
   auto-revokes on it. The packet's timeout story is 1,800 s and its abort list
   omits expiry, so a slow attended session consumes the approval at startup and
   then loses authority mid-session with retry forbidden. It fails closed, which
   is why this is not blocking — but an operator being asked for authority should
   be told how long it lasts.
2. **Correct the launch-sequence sentence.** The packet says the supervisor
   verifies signing identity through the Mach handshake and revalidates capsule
   identities "and only then resumes it". In
   `src/app/src/designated_live_macos_supervisor.cpp:661-699` the pid-keyed
   signing check and capsule revalidation precede `SIGCONT`; the audit-token
   handshake necessarily follows resume, because a suspended child cannot send.
   The invariant is real and enforced; the description of it is wrong.
3. **Do not carry v002's matrix forward.** See the required final state above.

## Non-goals

- Do not execute the live lane, access a credential, or submit a mutation.
- Do not materialize an approval file. This card prepares a packet; it has no
  authority to approve one.
- Do not rebuild or reconfigure `build/dev`.
- Do not edit v002's bytes beyond the status line already committed, and do not
  delete it. An invalidated packet is the record of why v003 exists.
- Do not treat the review's `b2e2c2d8…` figure as the answer.
- Do not close [[BPR-0256]], [[BPR-0142]], or RFC-BPR-0008.

## Verification

```sh
shasum -a 256 build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

plus the eleven-selector matrix named in
`docs/verification/RFC-BPR-0008.md`, re-run at the regenerated digest, and the
capsule's own `stage-zero` verification.

## One thing worth carrying forward past this card

v002 was stale at birth and stayed that way through a regeneration, a status
line reading "pending capability-critical review", and a board that listed it as
an operator decision. Nothing detected it until a reviewer recomputed the
manifest. The check is four lines of Python over a file the repository already
keeps, and `scripts/verify_documentation.py` already re-hashes bound artifacts
for *answered* packets through `frozen_packets`. Extending that to verify a
**pending** packet's `source-manifest.txt` against the tree would have caught
this at the commit that introduced it. That is a separate card, not this one.

## Result — 2026-08-20

`designated-live-review-publication-v003` is written and the eleven-selector
matrix is green at the regenerated digest. The fresh capability-critical review
is the one remaining step before the operator may be asked.

### The manifest was reproduced, not copied

This card warned that the review's `b2e2c2d8…` figure was "a starting
expectation, not evidence". It was reproduced from the algorithm instead: the 31
paths read from `bpr_attended_review_source_surface` in
`src/app/CMakeLists.txt:96-128`, each file hashed, the pairs concatenated as
`path=sha256` lines **in list order**, and that string hashed — which is exactly
what `file(SHA256)` plus `string(SHA256 …)` do at configure time.

That produced `b2e2c2d8f005e62ff997ec4e4dd87f48af6da0decdcf64a48b54746131ef4a81`,
and the freshly built capsule binary carries the same value compiled in. Manifest
and binary agree, which is the check v002 failed.

### The approval subject was rendered by a validated renderer

`designated_review_approval_subject` is C++ and the capsule graph has
`BUILD_TESTING=OFF`, so it cannot be called directly here. Rather than
reimplement it and hope, the Python rendering follows the append sequence at
`src/application/src/designated_review_authority_bootstrap.cpp:1488-1542`
literally and was validated before use:

- fed v002's own inputs it reproduces v002's `approval-subject.txt`
  **byte-exactly** — 1,496 bytes, `sha256:929a013d44ee…`, identical;
- `git log 1842216..HEAD` shows **zero** commits to either
  `designated_review_authority_bootstrap.cpp` or
  `review_mutation_execution_authority.hpp`, so the renderer that produced v002
  is the renderer that would produce v003;
- the resulting subject differs from v002 in exactly four fields, all measured
  digests: `application_sha256`, `source_sha256`, `supervisor_sha256`,
  `inventory_sha256`.

A byte-exact reproduction of a known-good output, from an unchanged source, is
evidence. It is not the same as running the real function, and that limit is
recorded rather than glossed.

### Verification matrix

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| the graph builds tests-disabled Release | `build/bpr0256-release` | `cmake -S . -B … && cmake --build …` | `BUILD_TESTING=OFF`, `Release`, `BPR_BUILD_QT=ON` in its cache | 89/89, exit 0 | this commit |
| the binary carries the manifest digest | the built capsule app | `strings` for the 64-hex constant | a mismatch is the v002 defect exactly | `b2e2c2d8f005…` found | this commit |
| the capsule verifies | `designated_live_macos_capsule.py verify` | `verify --capsule … --inventory … --source-sha256 …` | it checks signatures, inventory, and the source binding | `designated-live-macos-capsule:pass` | this commit |
| the read-only image verifies from inside | `stage-zero` | `stage-zero --image … --expected-image-sha256 … --mount-parent …` | it mounts read-only, verifies the inventory from within the mount, detaches | `mounted:…` then `pass`, exit 0 | this commit |
| the subject renderer is faithful | v002's own bytes | reproduce v002 from v002's inputs | a byte-exact match of a known-good rendering | 1,496 bytes, `929a013d…` identical | this commit |
| the renderer has not moved | Git | `git log 1842216..HEAD -- <both files>` | a changed renderer would invalidate the reproduction | 0 commits each | this commit |
| the matrix is green at the regenerated digest | eleven registered selectors | the RFC-BPR-0008 ledger selector against `build/dev` | it exercises bootstrap, host, composition, round trip, capsule, provenance, identity, isolation, architecture | **11/11**, 231.30 s | this commit |
| the dev binary never moved | the bound executable | `shasum -a 256` before and after every build and run | a relink invalidates two pending attended packets | `7b9d6be9…a78fcd93` at all four checkpoints | this commit |

### Three defects fixed rather than carried forward

1. **The 15-minute authority lifetime is now disclosed** in the abort
   conditions, with the distinction that matters: the 1,800 second CTest timeout
   is the lane's bound, not the authority's, so a session that consumes the
   approval and then pauses can lose it mid-run with no retry, and the approval
   is spent either way.
2. **The launch sequence is corrected.** v002 said the supervisor verifies the
   child through the Mach handshake and "only then resumes it". A suspended
   child cannot answer, so the handshake necessarily follows resume; what
   precedes it is the pid-keyed signing check and the capsule revalidation. The
   invariant was real, the description was not.
3. **The manifest claim now states how it was checked** rather than that it was.

### Not done

Step (2), the fresh capability-critical review of v003, has not run. Step (3),
the attended operator decision, has not been requested and this card has no
authority to request it.

## The capability-critical review returned no blocking finding — 2026-08-20

Step (2) is done. The review re-derived every attack target against live source
and direct re-execution rather than against this card, and each came back sound.

### It closed the limitation this card recorded

The card said the Python rendering "is not the same as running the real
function, and that limit is recorded rather than glossed". The review answered
it two ways:

- it reconstructed the subject byte-by-byte from the C++ append sequence itself
  and got 1,496 bytes and `sha256:35200c89…`, identical — an independent path to
  the same bytes;
- and more decisively, **the application re-renders the subject through the real
  renderer at run time and requires byte equality**
  (`designated_review_authority_bootstrap.cpp:1598-1603`). A transcription
  divergence therefore yields `approval_mismatch` and refuses. The failure mode
  the limit was hedging against cannot produce a wrong grant, only a refusal.

That is a better answer than the hedge, and it was found by attacking the hedge.

### What it confirmed independently

v002's failure mode is affirmatively absent: all 31 manifest entries match the
tree, the aggregate equals both the manifest file's digest and the sole 64-hex
constant in **both** capsule binaries, and the capsule was built from `8ca12d9`,
which is an ancestor of `main`. One-time consumption holds under every ordering
the reviewer constructed — hardlink, second parent, crash between record and
consume, losing concurrent launch. Capsule bypass is closed in both directions.
The corrected launch-sequence claim is now right: the pid-keyed signing check and
the content-digest revalidation precede `SIGCONT` at
`designated_live_macos_supervisor.cpp:688`, and the handshake follows at `:696`.

The reviewer also ran selectors beyond this card's obligation — the provider,
mutation-contract, recovery-store, and transport families — 7/7, and reproduced
the eleven-selector matrix independently at 11/11 in 223.90 s.

### Three non-blocking findings, all fixed here

1. **v003 carried v002's preamble verbatim**, including "This v002 packet … binds
   a fresh tests-disabled Release capsule built at commit `b04b43c`" — the exact
   commit v002 was invalidated for, in a document whose predecessor died for
   binding it, and v003 never named its own source state. The signed decision
   anchors on the digest table so it could not have caused a wrong approval, but
   it is the provenance sentence an operator reads. Rewritten to state v003's
   own provenance first and mark v002's as history.
2. **The pending-packet manifest gate was promised and not filed.** This card
   said "that is a separate card, not this one" and then did not write it. Filed
   as [[BPR-0411]]. `frozen_packets` only re-hashes *answered* packets, so the
   window in which a packet is most dangerous — written, pending, being approved
   — is exactly the window nothing checks.
3. **The ledger row still named v002** as the regenerated packet.
   `docs/verification/RFC-BPR-0008.md` now names v003, its digests, its matrix,
   and this review.

A fourth thing the review raised is now disclosed rather than fixed: `approval_id`
is inherited from v002. Reusing it is correct — the namespace was never reset and
holds zero spent markers, so the id is unspent, and minting a fresh one would
require the reset this packet forbids. The packet now says so instead of leaving
an operator to notice.

### Status

Step (1) regenerated, step (2) clean. Step (3), the exact attended operator
decision, is the only thing left and this card has no authority to request it.
