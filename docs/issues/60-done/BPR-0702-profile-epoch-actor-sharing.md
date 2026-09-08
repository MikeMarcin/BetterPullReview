---
schema: "implement-rfc.card.v1"
id: "BPR-0702"
title: "Share profile cache actors with independent window epoch state"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0697", "BPR-0698"]
acceptance: [{"criterion":"Bounded profile cache sharing and window isolation criteria","outcome":"pass","revision":"HEAD plus explicit finish paths","evidence":{"kind":"observation","detail":"Core-only epochhost0.61s tests pass A/B/A, exact immutable FIFO switches/max1live session, Aclose/Bqueued survival, release/reopen/destructoroverlap. ThreeTUs tidy/format/diff pass. Mediumreview no blockers. Rebuilt public epochpersistencegithub-com seed/reopen52.01s pass after existing owner-private fixture helper; no live repository or actual multiwindow UI claim."}}]
---

# Share profile cache actors with independent window epoch state

## Scope

Extract a bounded profile-owned cache actor registry from epoch_application_host.
Share the exact-authority repository cache session and serialized FIFO, while
retaining selected-diff/blob/realignment warrants, PR catalog sessions, jobs,
projection and cancellation per window. Include access mode in actor identity.
Admit exact profile/window context; no competing live cache open for a sibling lease.
Last-consumer retirement releases the session only after admitted work drains.
Profile shutdown separately refuses admission and drains owned actors.

Keep the current facade using the same extracted implementation. Executors are
supplied by the profile owner and outlive quiescence. No Git/SQLite schema or
Qt changes; actual multiple-window construction remains later integration.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, app composition, CMake, bookkeeping and acceptance.
Workers own separate epoch or ledger paths and preserve parallel work.

## Acceptance

Two valid window consumers share one live cache session and retain independent
selected comparisons. Equal exact requests reuse it; distinct immutable session/PR
requests may close/reopen only through the serialized actor FIFO, never with two
live sessions or authority translation. Closing A preserves B work/projection; final detach/drain closes
once and later admission reopens once. Different host/account/revision/object
format/access mode never coalesce; foreign/stale contexts refuse before effects.
Existing epoch host contracts and a composed epoch path remain passing.

## Evidence

The profile registry binds one cache capability and bounded weak actor entries;
window consumers keep independent warrants, requests, cancellation and projection.
FIFO request switching preserves immutable authority with at most one live cache
session. A release marker prevents replacement while a session destructor drains.

Core-only epoch host tests pass (0.61s), including A/B/A exact requests, A shutdown
while B has queued work, final release/reopen and destructor-overlap admission.
All three changed TUs pass warnings-as-errors tidy, formatting and diff checks.
Independent medium review found no evidence-backed blocker. The rebuilt public
GitHub epoch persistence seed/reopen course passes (52.01s). Its fixture now uses
the existing owner-private directory helper; production root refusal is unchanged.
Native Git effects are exercised by that fixture course, not a live repository.
Actual same-profile Qt multiple-window construction remains later integration.
