---
schema: "implement-rfc.card.v1"
id: "BPR-1945"
title: "Count the live agent clock's first second without an early coarse tick"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"bug","found_by":"release candidate41 native macOS focused course","related_cards":["BPR-1941","BPR-1944"],"milestone_plan":{"id":"bpr-1945-live-agent-clock","path":"docs/rfcs/plans/bpr-1945-live-agent-clock/milestone-plan.json"}}
---

# BPR-1945: Live agent clock accuracy

## Acceptance

- AC1. The shared run line uses a timer that cannot fire early, retaining its
  1000 ms interval, actual monotonic elapsed formatting, Agent/Tour reuse,
  start-stop and lifetime behavior. No visual design or wording changes.
- AC2. Retain candidate41's actual Release failure and causal negative control.
  A phase-varied semantic first-second regression and affected original Qt
  courses pass with the original Release 1300 ms bound, without an assertion
  or budget waiver. Preserve BPR-1944's accepted code-block behavior.
- AC3. Focused clang-format22, warnings-as-errors clang-tidy22, documentation,
  board validation and intentional latest-main integration pass. Hand back
  exact source, evidence and native ownership; frozen release qualification
  and publication remain coordinator work.

## Diagnosis and mandate

Candidate41 passes all three Mac build graphs and49/50 focused courses but
qt_tour_side_chat fails after1.51s: the live thinking line still says0:00.
The owning status timer originates in BPR-1941 and is byte-identical through
latest accepted BPR-1944. Default QTimer is CoarseTimer: it may fire5% early.
The line floors actual elapsed milliseconds to seconds, so an early first tick
prints0:00 and the next tick is near two seconds. BPR-1944 changes the test's
Debug wait and stops waiting when words change, retaining Release1300ms; its
code-block source changes are not needed to reproduce the failure.

One external probe compiles the exact owning source and starts32 status lines
at staggered phases.15/32 still say0:00 at1300ms; those first ticks occur at
950–983ms. The actual logs and hashes are external under candidate41/
implementer1945/accuracy-negative.*. The coordinator approves selecting
Qt::PreciseTimer, which cannot fire early, without changing any other timer
or presentation behavior. Official Qt6.12 QTimer accuracy documentation
supports the mechanism. frontend-design and desktop guidance are loaded;
this is a behavior correction with no new surface or design decision.
