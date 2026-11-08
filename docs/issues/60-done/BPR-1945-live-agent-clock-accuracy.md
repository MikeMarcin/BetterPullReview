---
schema: "implement-rfc.card.v1"
id: "BPR-1945"
title: "Count the live agent clock's first second without an early coarse tick"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"bug","found_by":"release candidate41 native macOS focused course","related_cards":["BPR-1941","BPR-1944"],"milestone_plan":{"id":"bpr-1945-live-agent-clock","path":"docs/rfcs/plans/bpr-1945-live-agent-clock/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"cfe9b4d075c14e42e85a3b46be29a6f96a106ce7","evidence":{"kind":"observation","detail":"Mechanical source control: only PreciseTimer choice plus explanation;1000ms actual clock/format/start-stop/lifetime/shared use unchanged; no appearance change."}},{"criterion":"AC2","outcome":"pass","revision":"cfe9b4d075c14e42e85a3b46be29a6f96a106ce7","evidence":{"kind":"command","detail":"Original41failure retained; identical new registered test fails2.02s on coarse source then passes3.63s;32phase lines15misses→32pass;3Tour/Agent composed controls pass under original budgets.","path":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/candidate41/implementer1945/verification-results.json"}},{"criterion":"AC3","outcome":"pass","revision":"cfe9b4d075c14e42e85a3b46be29a6f96a106ce7","evidence":{"kind":"observation","detail":"Format22/tidy22/docs/board/self-review pass; latest accepted1944 integrated and exact source pushed authoritative main. Native PC and frozen sources untouched; final full-release qualification explicitly pending."}}]
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


## Verification and main integration

The production change selects Qt::PreciseTimer on the shared status line's
existing owned QTimer. Its interval remains1000ms. Mechanical self-review
proves every other production byte unchanged, including elapsed formatting,
start/stop and lifetime. The original composed test's Release1300ms and
Debug3000ms block, stage wording and redraw checks are byte-identical.
Eight first-second probes vary the start phase, check actual whole elapsed
seconds, and retain the same per-probe Release/Debug budgets. They run in the
existing registered qt_tour_side_chat course; no registration or isolation
policy changed. No owner desktop was driven or captured.

The new registered regression with the ORIGINAL coarse product source fails
at2.02s with0:00 (exit8). After the precision fix, the identical test source
passes in3.63s. The external32-line exact-source probe changes from15 misses
(first ticks950–983ms) to32/32 pass (first ticks1000–1001ms), at the same1300ms
observation. These are causal controls, not repeated runs until a pass.
Candidate41's original focused49/50 failure remains distinct and unchanged.

On macOS arm64, Qt6.12.0, Release, original configured offscreen courses:
qt_tour_agent_session_view PASS0.78s, qt_tour_chat_in_tab PASS1.19s,
qt_workbench_window_scale_100 PASS28.55s, plus the side-chat regression above.
The latter composed window exercises the Agent tab's shared run footer and
terminal transitions. The chat courses retain accepted BPR-1944 Copy/Wrap and
independent scrolling assertions. No budgets or assertions were removed.
Clang-format22, warnings-as-errors clang-tidy22 on both changed TUs,
documentation, board validation and git diff --check pass. One initial
new-test aggregate initialization compile error was corrected with an explicit
member initializer; its raw failed build is retained and is not acceptance.

The exact two-file source correction cfe9b4d075c14e42e85a3b46be29a6f96a106ce7
is intentionally integrated and pushed to authoritative main, including latest
accepted BPR-1944 efb3f424a. Final closeout changes only this card. Evidence,
source/binary identities and raw positive/negative logs are external at
candidate41/implementer1945/verification-results.json and self-review.json.
The explicit milestone-plan bind passes. A separate target-only checkpoint
helper scans a legacy release-v041 plan and refuses its existing amendments
field; the raw refusal is retained. No unrelated historic plan is changed.

This is focused engineering evidence. Windows/Linux and the next frozen
candidate's full native graphs, trust, packaging and release acceptance remain
coordinator obligations. BPR_BUILD_TOUR_CLI is OFF in this ordinary local cache;
no shipped CLI feature qualification is inferred. The native PC, protected
Windows B5 source and coordinator's frozen41 source are untouched by this task.
No task commands or review servers remain active at handback. Existing accepted
1941/1944 appearance is preserved; no new appearance decision needs attendance.

Qt's documented timer contract: [QTimer accuracy and timer resolution](https://doc.qt.io/qt-6/qtimer.html#accuracy-and-timer-resolution).
