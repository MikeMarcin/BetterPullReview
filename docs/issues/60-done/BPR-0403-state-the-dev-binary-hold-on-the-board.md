---
id: BPR-0403
title: State the dev-binary hold on the board, not just inside three cards
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# State the dev-binary hold on the board, not just inside three cards

Three operator packets are pending right now, and all three bind the exact bytes
of the same file:

```
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93
51,085,872 bytes
```

- `docs/verification/reviews/appearance-scheme-foreground-v007/` — [[BPR-0391]],
  and [[BPR-0220]]'s causal question rides the same run.
- `docs/verification/reviews/native-notification-delivery-v005/` — [[BPR-0392]],
  and [[BPR-0168]]'s attended pass rides the same run.
- `docs/verification/reviews/native-visual-acceptance-v003/` — [[BPR-0100]].

Verified at HEAD: the on-disk executable is byte-identical to the bound digest,
so all three are live and answerable today. That check also retires the only
residual doubt about them — each card asserted settlement, and none of the three
had been confirmed against the file since.

## The problem

Relinking that executable invalidates all three at once, and it has already
happened three times in a row. [[BPR-0391]] records the third:

> Third rebuild in a row to invalidate a pending packet; caught before an
> operator hit it rather than after.

Each time, the rebuild was a legitimate repair — [[BPR-0390]]'s blob-read fix,
[[BPR-0397]]'s bounded actor waits — made by a session that had no reason to know
three attended artifacts were downstream of the link step.

The hold that protects them is currently written in exactly two kinds of place:
inside the three cards that own the packets, and one cell of
`docs/verification/RFC-BPR-0008.md`. Neither is where a session looks before
editing `src/`. The board README, which is what a session actually reads to
orient, does not mention it.

## Why the existing safeguard is not enough

[[BPR-0391]]'s stated reason the bytes would hold this time was:

> every card that could change these bytes is in `60-done/`, and nothing in an
> active lane is executable without a Windows or Linux host, a product decision,
> or an owner verdict.

[[BPR-0402]] retired the second half of that sentence. Portable work does exist
on this board, and at least one identified piece of it — [[BPR-0334]]'s
extraction of the duplicated agent protocol loop — edits `src/` and would relink.
The bytes held through this session because the session checked the digest and
chose to stay out of `src/`, not because nothing could have touched it.

An argument that depends on the board having no available work is not a
safeguard. It is a coincidence that happened to hold.

## Required final state

- `docs/issues/README.md` states the hold where a session orienting on the board
  will see it, names the three packets it protects, and says what lifts it.
- The statement is falsifiable: it names the exact file and digest, so a session
  can check in one command whether the hold is still in force rather than
  trusting this card.
- It distinguishes what the hold forbids — a rebuild — from what it permits: a
  `ctest` run against the already-built tree, which [[BPR-0256]] used at
  `5245c12` to discharge a gate without moving a byte.
- No claim that the hold is permanent. It expires when the three verdicts are
  returned, or when an owner decides a repair outranks them.

## Non-goals

- Do not rebuild anything, and do not touch `src/`.
- Do not regenerate, retire, or alter any of the three packets. They are current.
- Do not add a mechanical enforcement gate. A verifier rule that reads a build
  directory would fail on any checkout that has not built, and this is guidance
  for a human or agent choosing what to edit, not a repository invariant.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
shasum -a 256 build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
git diff --check
```

## Result — 2026-08-19

The hold is stated in `docs/issues/README.md` under "Verification scope", which
is the section a session reads to decide what to run and what to touch.

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| the three packets are still live | the bound executable | `shasum -a 256 build/dev/.../better_pull_review` | a packet whose bound bytes moved is stale by construction | `7b9d6be9…a78fcd93`, byte-identical | this commit |
| the board stays internally consistent | `scripts/verify_documentation.py` | `python3 scripts/verify_documentation.py --root .` | it rejects lane/status disagreement and a frontier table that does not match the lanes | `DOCUMENTATION_OK` | this commit |
| the documentation contract suite still passes | `tests/scripts/verify_documentation_tests.py` | `python3 tests/scripts/verify_documentation_tests.py` | 94 cases | 94 tests, OK | this commit |
| no whitespace damage | Git | `git diff --check` | it fails on trailing whitespace and conflict markers | clean | this commit |

The digest check is the substantive one. Each of [[BPR-0100]], [[BPR-0391]], and
[[BPR-0392]] asserted its packet was settled, and none had been confirmed against
the file since. It is confirmed now, so all three are answerable today.

### What was deliberately not built

No mechanical enforcement. A verifier rule that reads `build/dev` would fail on
any checkout that has not built, and the hold is guidance for whoever chooses
what to edit — not a repository invariant. The falsifiable digest in the README
is the substitute: a session can check the claim in one command instead of
trusting it.
