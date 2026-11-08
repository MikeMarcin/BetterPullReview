---
schema: "implement-rfc.card.v1"
id: "BPR-1932"
title: "Land release-discovered portability and presentation regressions on main"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"v0.4.2 native qualification","mandate":"Owner 2026-11-07: an implementer owns crash/fix implementation and lands it on main before release qualification resumes.","milestone_plan":{"id":"bpr-1932-release-regressions","path":"docs/rfcs/plans/bpr-1932-release-regressions/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"16959a628ca44f678ecb097b2690a8d76c174b52","evidence":{"kind":"observation","detail":"Reviewed source split ownership, native path and platform capture corrections landed main; exact matching Windows Release/Debug compiler and clone-course feedback retained in candidate34 Windows proof receipts. Fresh final full native qualification is separate."}},{"criterion":"AC2","outcome":"pass","revision":"16959a628ca44f678ecb097b2690a8d76c174b52","evidence":{"kind":"command","detail":"Overview completion unit/composed evidence passes; targeted accepted-behavior corrections retain negative controls. Own focused Qt/core and exact public6 pass, with33fileclang-format22 and network Python74 checks."}},{"criterion":"AC3","outcome":"pass","revision":"16959a628ca44f678ecb097b2690a8d76c174b52","evidence":{"kind":"command","detail":"Original Tour stack overflow retained/reproduced. Resize trace identifies recursive scrollbar/header wrapping. Old header fails new measurement regression; correction passes stable hints across60 sizes,25 consecutive original manual publication runs113.93sec, exact own-source manual course and drawer scales100/125/200."}},{"criterion":"AC4","outcome":"pass","revision":"16959a628ca44f678ecb097b2690a8d76c174b52","evidence":{"kind":"command","detail":"All source/test/tool fixes pushed authoritative main16959a628. Format22 passes33C++files, docs DOCUMENTATION_OK and67tests, architecture+contract pass, core-only3 pass. Configured Debug/Release watchdog values and manifest pass. Full release/security/native/source-artifact acceptance not claimed."}}]
---

# BPR-1932: Release-discovered regressions

## Outcome and acceptance

- AC1. Platform-specific declarations and fixture capture constants remain with
  the translation units that consume them after the source splits. Retain the
  native Windows Release/Debug compiler and clone-path evidence.
- AC2. Overview completion is announced once after the visible presentation
  handoff; superseded or still-hidden completions stay silent. Reconcile
  focused courses with already accepted header, setup, Tour wording and skill
  behavior without weakening their negative controls.
- AC3. Diagnose and prevent the Tour publication geometry recursion seen in the
  composed manual publication course. Exercise a focused regression and the
  public composition, retaining the original intermittent crash evidence.
- AC4. Format and documentation gates pass; fixes land on authoritative main.

## Bounded milestone

One engineering milestone owns the four acceptance criteria and integration. It does not accept
native full-release qualification, signing, security or publication. Source
baseline is `160427576`; prior release-only fixes are reviewed before adoption.

## Retained failure

The v0.4.2 build-34 investigation's manual Tour publication course crashed with
SIGSEGV and a macOS stack-overflow report: repeated `QScrollArea::eventFilter`
and `QWidget::resize`. A registered repeat reproduced it after nine passes.
External evidence is under the release run's `candidate34` directory, not in
this repository. The coordinator owns release artifacts and final qualification.


## Engineering (2026-11-07)

`frontend-design:frontend-design` was loaded. Source changes preserve the
accepted controls, palette, spacing and one/two-row header arrangement.

- Reviewed and adopted the release-only portability corrections: Windows
  menu/window declarations, title-row host ownership, caption capture control,
  New Window shortcut ownership, platform fixture height floors, native path
  comparisons, move-only expected result, and strict GCC test anchor lifetime.
- Kept new recent visits while the initial stored history loads; merge older
  entries behind them, fill only missing facts, and retain a removed entry's
  tombstone until that load finishes. Core checks cover the late read/removal.
- Reconciled the seeded refresh recording order with the accepted conversation-
  first refresh; request shapes and exact recording checks remain enforced.
- Accessible Overview completion waits until the presentation handoff ends,
  once, with a revision guard; superseded and still-hidden completions are
  silent. Unit and composed readiness courses cover it.
- Reconciled tests with accepted wrapping, Tour conversation wording, built-in
  skill version, clone selection, host-applied Ready/next and loaded progress.
  Negative wording controls remain. Source navigation dispatches real event
  turns through the already-shared helper.
- Replaced the Tour agent header's resize-time grid row mutation with a local
  Qt layout that measures its two rows at the requested width. Its minimum
  and height-for-width do not depend on its current width. Controls remain
  fixed at their own sizes and the trailing group remains right-aligned.
- Inventoried the repository skill reader's existing Windows uncontained
  registration. Its contained native measurement is explicitly **unmeasured**;
  no successful containment or native product claim follows from the entry.

### Exact source and retained evidence

Engineering source: `f1ee8ed41b0ec66a06df6a605b879582fc8eb4e8`, with the
inventory follow-up `0ab4159ef`. Evidence stays outside source, under
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/candidate34`.

- Original failure: `final-focused.xml` and `tour-publication-repeat.xml`.
  The repeat passed nine times then crashed with a stack overflow. The scoped
  resize trace (`implementer-trace.log`) shows the Tour holder recursively
  alternating 320×661 and 302×676 as the header wraps and the scrollbar appears.
- Negative control: `implementer-negative.xml` rejects the original header's
  missing requested-width sizing before geometry changes.
- Corrected regression: `implementer-regression-r4.xml` passes, including sixty
  sizes around the header wrap and scrollbar boundary with stable hints and a
  settled column. Existing reset, focus, proposal, transcript and header checks
  remain in the same course.
- The original manual publication course passes 25 consecutive quiet runs
  (`implementer-quiet-repeat.xml`, 113.93 seconds). Temporary tracing was removed.
  The scoped release source was restored exactly to `df00de349`, clean, with
  `implementer-release-source-restored.json`; release coordination owns it again.
- Own Release architecture and architecture-contract courses pass (18.58 and
  180.04 seconds). Own core-only restoration, Tour agent and architecture courses
  pass (three checks, 22.63 seconds). Own pending-veil and click-scroll courses
  pass. Exact own-source manifest, round-trip composition, review submission at 100%, 125% and 200%, and manual Tour publication all pass (six courses, 50.19 seconds; `implementer-exact-public.xml`).
- clang-format 22 passes all 33 changed C++ files. The documentation gate prints
  `DOCUMENTATION_OK`; its 67 tests and the network isolation verifier's 74 tests
  pass. Exact application composition pins were emitted by their own verifier.
- Initial partial-build failures remain in `implementer-final.xml`: the manifest
  identified the missing skill entry and absent Git helper, and the round-trip
  app binary had not yet been built. The targeted build and reruns retain that
  distinction; they do not rewrite the failed run.

### Separate native release obligations

Windows focused compiler/clone proofs are in the coordinator's build-34 records.
The longer Debug watchdog passed composed hosts, default review submission and
125% review submission. The 200% course failed an own-approval-settlement
predicate after its internal twenty-second wait (R4, 114.98 seconds). That raw
failure remains retained and its intermittent cause is **unexplained**. R5
passed the original course in 99.86 seconds. R6 then ran four serial diagnostic
repetitions, all passing the original twenty-second predicate. Every snapshot
showed the expected draft/body/control prerequisites, exactly one attempted
submission (21 to 22), and the expected own-approval refusal. None required the
diagnostic's extra observation window. R6 is diagnostic evidence, not final
native acceptance and not proof that the R4 cause was repaired. Receipts, four
XML results and four raw outputs are under
`windows-approval-diagnostic-proof-r6`; source and generated settings were
restored exactly and the native host is idle.

The coordinator must run the unchanged approval checks in final exact-main
native qualification. Any functional failure returns to an implementer before
stable publication. Full native release qualification, Cocoa observation,
artifact/source/security, signing and publication remain coordinator-owned and
are not accepted here.

### Workflow tooling observation

The adopted plan's explicit binding resolves all four criteria. The generic
checkpoint helper's unrelated-plan recovery scan refuses an existing plan's
`amendments` field; its failure is retained in the external continuation.
No historical plan was altered or acceptance inferred from that helper.


### Measured Debug watchdogs

The native Windows Debug courses passed with outer watchdogs of 600 seconds:
composed hosts in 30.34 seconds, submission at 100% in 87.52 seconds, at 125%
in 92.61 seconds, and a diagnostic 200% run in 99.86 seconds. The former 30/90
second registrations could cut off their functional checks. Their CMake test
properties now give Debug 120 seconds for composed hosts and 600 seconds for
the three submission courses. Release retains 30/90 seconds. Every functional
predicate remains, including the own-approval internal twenty-second wait.
Configured CTest JSON from both graphs proves the exact values
(`implementer/watchdog-configured-values.json`); the manifest check passes.
These are bounded watchdogs and give no Debug timing/performance credit.


## Engineering handback

All reviewed source, test, containment-inventory and bounded Debug-watchdog
corrections are pushed to authoritative main through
`16959a628ca44f678ecb097b2690a8d76c174b52`. The completed card's move and acceptance
record are bookkeeping on that integrated source. All task-owned Mac builds and
checks are finished; the scoped release tree and native producer were restored
clean. The isolated implementer worktree and external evidence remain available
for inspection. The selected engineering criteria are complete; this closes no
RFC-BPR-0038 or release-distribution criterion.
