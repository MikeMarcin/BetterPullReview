---
id: BPR-0406
title: Regenerate the live publication packet from the committed tree
status: In Progress
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
