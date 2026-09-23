---
schema: "implement-rfc.card.v1"
id: "BPR-1019"
title: "Bind RFC-0024 AC-07 to a current passing window review restoration course"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0985"]
priority: "P2"
metadata: {"origin":"RFC-BPR-0024 owner amendment r4, 2026-09-23","closeout_card":"BPR-0595","criterion":"AC-07","course":"application_profile_window_review_restoration_public_path","related_cards":["BPR-0595","BPR-0709","BPR-0710","BPR-0985","BPR-0707","BPR-0977","BPR-0597","BPR-0733"],"status_at_filing":"course passes 598.25 s at 0ddf929b (build/dev from tree 459296c6); re-bind only, no repair needed"}
---

# BPR-1019: Bind RFC-0024 AC-07 to a current passing window review restoration course

## Why this card exists

On 2026-09-23 the owner directed, verbatim: "for RFC-0024, criterion AC-07
just make an amendment and file a follow-up to fix that test". The owner gave
this in the coordinating chat, and the coordinator relayed it verbatim. The
amendment is
[RFC-BPR-0024 owner amendment r4](../../rfcs/RFC-BPR-0024-bpr-profiles-multiple-windows.md#owner-amendment-r4--2026-09-23),
also appended to [[BPR-0595]]. This card is the follow-up.

"That test" is `application_profile_window_review_restoration_public_path`
(executable `tests/end_to_end/profile_window_review_restoration_public_path`).
It is the public course of [[BPR-0709]] and [[BPR-0710]], and AC-07's row on
BPR-0595 cites it as retained evidence. RFC-BPR-0024 closed at `eb73df8b`
with that course red. The owner kept AC-07 closed over that gap. This card
binds AC-07 to a current passing run.

**The test already passes, so this card only re-binds AC-07's evidence to
it.** [[BPR-0985]] repaired the product regression at `c9c48800`. At filing,
one `ctest --test-dir build/dev -R
'^application_profile_window_review_restoration_public_path$'
--output-on-failure` run passed in 598.25 s and printed
`PROFILE_WINDOW_REVIEW_RESTORATION_OK`. The run used `build/dev` built from
`0ddf929b`'s tree `459296c6`, and ninja had no work to do. The logs are
local and git-ignored: `build/rfc0024-ac07-restoration-course-20260923.log`,
with the full output in `….lasttest.log`. Nothing is left to fix unless a
later run fails. The fallback is in AC1.

## Failure and history

| Revision | Course result | Record |
|---|---|---|
| `ce63c67b`, `377f4352` (2026-09-08) | Pass: BPR-0709's course 15.70 s; BPR-0710's 31-phase run 790.66 s | [[BPR-0709]], [[BPR-0710]] |
| `edb90493` (2026-09-15) | Pass in the d3 integration selection | [[BPR-0775]] |
| `cc038748` (2026-09-19, RFC-BPR-0037) | Reading phases break. The conversations drawer's default thread is re-revealed after every published file diff, reports a reading gesture and cancels the deferred reading restoration | [[BPR-0985]] |
| `9c670d88` (2026-09-19) onward | `auth-home` fails first, on an assertion superseded by the owner-accepted [[BPR-0795]] loading Overview, and hides the reading phases | [[BPR-0985]] |
| `eb73df8b` (2026-09-22, RFC-BPR-0024 closed) | Not run; red. No `src/` or `tests/` change separates it from `e2f6eb1d`, where [[BPR-0597]] found it failing | [[BPR-0595]] |
| `5b14d7e8`, `6a897af8` (2026-09-22) | Fails at `auth-home`, then at `auth-reading-gesture`. The full-phase diagnosis adds `unavailable-semantic` and `auth-restore` | [[BPR-0595]], [[BPR-0985]] |
| `c9c48800` (2026-09-22, repair) | Pass 570.40 s; the negative control fails the phases it names | [[BPR-0985]] |
| `17e40dc8` (2026-09-22) | Pass 591.97 s | [[BPR-0987]] |
| `0d56cee5` (2026-09-23) | Pass 563.89 s | [[BPR-0990]], [[BPR-0599]] |
| `0ddf929b` (2026-09-23, at filing) | Pass 598.25 s, `PROFILE_WINDOW_REVIEW_RESTORATION_OK` | This card |

BPR-0985 and BPR-0595 say the course "last passed at `ce63c67b`". The
`edb90493` row shows that it passed later, and that the break came from
`cc038748`.

## What AC-07 rests on

AC-07: "Window restoration preserves independent sessions, distinguishes
explicit window close from application quit, and exposes the retained Resume
destination without fabricating unavailable content."

- `application_profile_window_review_restoration_public_path` (BPR-0709,
  BPR-0710, repaired by BPR-0985) covers each window's pull request,
  comparison, file and semantic selection across Close, Quit and restart, the
  Recently viewed Resume, and unavailable anchors, files, semantic readings
  and content. Each unavailable case is reported, and nothing is substituted.
- `application_profile_window_restart_public_path` ([[BPR-0707]]) covers
  durable window identities, MRU order, and Close versus Quit across real
  process restarts.
- `application_profile_window_layout_public_path` ([[BPR-0977]]) covers each
  window's layout and geometry across Close, Quit and restart.
- Beneath them are `core_window_review_restoration`,
  `application_window_review_restoration` and
  `sqlite_window_review_restoration_storage`.
- This card leaves the attended evidence unchanged: the owner accepts on
  `BPR-0977-native-geometry-v001` (decision `ec0c5993`) and
  `BPR-0715-signed-out-open-v004` (decision `b1eb8efb`).

Since [[BPR-0733]] (owner accept, 2026-09-11, decision `76d1917a`), the app
no longer reopens a window's last pull request by itself at startup. The
course asserts that Home stays until a Recently viewed row is chosen, and that
its Resume restores the exact reading. Describe AC-07's coverage in those
terms. Do not claim automatic reopening of other windows' reviews at startup,
which BPR-0599 records as superseded.

## Acceptance

- AC1: `application_profile_window_review_restoration_public_path` passes
  every phase on a build of a named `main` revision. Record the command,
  duration and log path. The filing run above counts for `0ddf929b`. If a
  phase fails, do not re-bind. Trace and classify the failure as BPR-0985 did: a
  product regression, or an assertion superseded by an owner-accepted
  behavior, with that decision named. Repair it, and show a negative control
  that fails the phase it names.
- AC2: At the same revision, these pass:
  `application_profile_window_restart_public_path`,
  `application_profile_window_layout_public_path`,
  `core_window_review_restoration`, `application_window_review_restoration`
  and `sqlite_window_review_restoration_storage`.
- AC3: A section appended to BPR-0595 binds AC-07's current evidence to that
  revision and those runs. BPR-0595's front-matter acceptance record, its
  AC-07 checklist row and the r4 amendment text stay unchanged. RFC-BPR-0024's
  r4 section names the result.
  `python3 scripts/verify_documentation.py --root .` and
  `issue_board.py validate --rfc RFC-BPR-0024` pass.

## Boundaries

Fixture-only: no live provider, credential or GitHub mutation. This card does
not change RFC-BPR-0024's Auto-Verified status, reopen it, or alter any other
criterion. It is not a dependency of BPR-0595, which stays Done.
