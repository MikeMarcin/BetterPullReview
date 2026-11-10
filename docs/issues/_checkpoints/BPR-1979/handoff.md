# Tour destination repairs checkpoint

- Assignment: import and implement the supplied card; mandate `tour-destination-repairs-r1`.
- Baseline/RFC_BASE_SHA: `cf9413b9f76dd34e299bc46518a52127b18b175c` on `main`.
- Owner: `/root/tour_repair`, nondelegating implementer; main/tracker authority from coordinator.
- Source: `docs/rfcs/assets/tour-destination-repairs-v1/card-tour-compatibility-forced-by-repairable-stops.md` (immutable).
- Live parent: BPR-1979. Core prerequisite: BPR-1980.
- Plan: repair-policy, editor-repairs, mixed-publication. All original AC1–AC7 remain on parent.
- Current frontier: repair-policy accepted and BPR-1980 Done at `21b108dbc25ba68af91d8d1c3fbf1d0273647f14`; BPR-1979 is Ready for a fresh editor-repairs implementer.
- Independent writers: none dispatched; foreign `.codex-attachments/` remains untouched.
- Commands: all task commands and descendants quiescent. No task worktrees/servers; no desktop or live GitHub effects. Foreign `.codex-attachments/` remains untouched.
- Required skills: implement-rfc loaded; frontend-design loaded from pinned MASK store, desktop reference read.
- Acceptance: no human or native criterion has been self-certified; parent remains incomplete.

## Current implementation frontier

- Import: `f1182ff3d`; BPR-1980 claim: `b4c397889`.
- Changed: new `review_tour_repair.hpp/.cpp`, core destination fallback, exact-byte revision mapping, shared nearest-hunk clamp, restore selection, focused core and real host regression tests, core/test target registration.
- Baseline public core course: `core_review_tour_destination` passed before edits.
- Focused core-only candidate: 4/4 passed in 1.45s (`core_review_tour_repair`, destination, tour_agent_session, diff_semantics). Mapping negatives include ignored whitespace and normalized endings.
- The first attempted focused build named a nonexistent diff-semantics target and exited before compilation; corrected target completed normally. No product failure inferred.
- Binding tool's unrelated-plan scan fails on existing legacy `amendments`; `/tmp/bpr-tour-repair-run/bind_target.py` selects the explicit adopted plan, verifies complete normal linkage, then calls pinned checkpoint.bind unchanged. Stored binding resolves all three obligations without diagnostics. No installed skill or unrelated plan was changed.
- Next milestone `editor-repairs` remains unimplemented; mixed publication follows it. No parent acceptance claimed.

## Verified source candidate and successor seam

- Source candidate: `9f0224f861b36ef3c58e63707b2cc85c846ae926` (main).
- Assignment bound to `repair-policy` / BPR-1980 only. Fresh successor must bind
  `editor-repairs`, using accepted repair-policy evidence; mixed-publication follows.
- Source API: `bpr/core/review_tour_repair.hpp`.
  `tour_destination_repairs(tour, observations, files)` returns blocker rows in
  route order; `plan_tour_stop_repair` includes the precise Destination plan,
  typed Problem and optional Repin/Relocation ranges. Tour ranges are zero-based;
  `tour_anchor_file` provider hunks and HeadLines are one-based.
- Obtain observations for the exact `publication_source(stop)`, with qualification
  facts for the exact operation/scope. Bind provider patch/HeadLines and mapped
  source observations to `ObservedHead.value_or(Head)`. Do not infer support from
  a painted diff, source text, a generic 422 or denied/unknown operations.
- Historical exact repair consumes `unchanged_material` + Mapped at the head and
  in a known hunk. The existing `map_tour_revision` supplies this, now checking
  retained bytes including line terminators. M2 must arrange the existing exact
  comparison/mapping read when it is not yet observed; source edits do not read Git.
- Preview relocation's `Relocation` range plus actual source text through the
  existing source host. Only after explicit Apply, call
  `prepare_tour_source_repair(currentTour, stopId, kind, observation, files)` and
  `TourHost->dispatch(*command)`. It returns one revision-bound restore command
  and sets Stop so the host keeps selection. Captured commands refuse stale edits.
  Re-plan after repair/Undo/Redo. Published tours/remote subjects are deliberately
  refused by this local source-edit API; maintenance has its separate plan.
- Existing actual-host regression is in `review_tour_host_tests.cpp`, proving
  selected identity, Undo, Redo and final saved repair on shutdown.
- No Qt/app publication composition was modified yet. In particular,
  `prepare_tour_plan` still uses the old generic compatibility sentence and must
  consume the new typed findings; no parent public-flow criterion is accepted.
- A qualified `file_permalink` may join a pending review in the destination
  planner, but existing aggregate readiness/publication still admits only native
  line batches. M2/M3 must reconcile actual file-operation support and staging;
  never label a standalone file comment as part of one pending review.
- `plan_tour_stop_repair` evaluates its supplied native-route observation: callers
  should pass CompatibilityRequested=false to describe the repairs before the
  explicit publication choice. The ordinary compatibility planner remains separate.
- Frontend-design desktop guidance was loaded. No owner desktop was driven or
  captured; M2 owns offscreen rendered/composed Qt evidence.

## Finished commands and evidence

- Baseline `ctest --preset core-only -R '^core_review_tour_destination$'`: pass.
- `cmake --build --preset core-only -j 6`: pass (full build, final relink also pass).
- Focused four core courses: 4/4 pass, 1.45s.
- Seven-course integration (`application_review_tour_host`, core repair/diff,
  core_dependency_boundary, core_boundary_contract, architecture_boundaries,
  architecture_boundary_contract): 7/7 pass, 198.84s. Contract negative cases
  consumed 174.92s; no hang or timing acceptance implied.
- LLVM22 format check for all ten touched C++ files: pass.
- LLVM22 tidy for all eight changed translation units, followed by changed repair
  source/test recheck after citation-range refinement: pass with warnings-as-errors.
- `python3 scripts/verify_documentation.py --root .`: pass.
- `python3 tests/scripts/verify_documentation_tests.py`: 67/67 pass. Printed ERRORs
  are expected negative fixture diagnostics; suite exit is zero.
- Scoped board validation for BPR-1979, BPR-1980, BPR-1737: pass; diff check pass.
- Optional broad core-only pass was deliberately interrupted after 58 completed
  courses, all passing, at `designated_live_macos_capsule_contract` (no result
  or coverage credit for that course). The exact completed rows are retained in
  `/tmp/bpr-tour-repair-run/optional-completed-tests.txt`. SIGINT was sent only
  to the owned CTest PID 73886; exit 130. All six captured descendants ended.
  No task image or mount remained according to hdiutil. The exact task-owned
  0700 isolation root `/private/tmp/bpr-net-u299qt9q` was removed. No full-suite
  pass is claimed. Required scoped acceptance is satisfied by the seven-course
  integration and focused negative controls. The broad command omitted dedicated
  Debug `core_diff_compute_benchmark` (no timing/functional coverage claimed) and
  reused the unchanged passing architecture contract. No further breadth is needed.
- Runtime logs: `/tmp/bpr-tour-repair-run/{core-build,final-core-build,integration-tests,core-functional-tests,tidy,final-tidy,format}.log`.
- Retained local failures: initial nonexistent target name (corrected); citation
  separator initializer-list deduction compilation error (corrected before final
  successful focused/full builds). No unresolved source failure.

## Milestone acceptance and next action

All BPR-1980 criteria and the repair-policy exits pass for source candidate
`9f0224f861b36ef3c58e63707b2cc85c846ae926`. Self-review checked source identity,
exact-byte mapping, zero/one-based conversion, integer bounds, capability refusal,
prose/readiness preservation and real host history/autosave. No independent or
human criterion was required for this foundation, and none is claimed.

The optional broad pass is incomplete, not an acceptance blocker or a failed
functional course. Required actual failures are corrected as recorded above.
No task-owned command, writer, worktree, server, review protection or mount
remains. The full source/test candidate and meaningful scoped gates are accepted.

Next: coordinator dispatches a fresh nondelegating implementer for
`editor-repairs` under this same mandate/plan, starting from main's committed
handoff and BPR-1980's recorded acceptance. Finish all BPR-1979 criteria through
its remaining two milestones; do not treat this foundation as parent completion.
The exact current-HEAD completion observation/check is external at
`/tmp/bpr-tour-repair-run/repair-policy-observation.json` and
`/tmp/bpr-tour-repair-run/repair-policy-check.json`; the tracked baseline,
source candidate, plan binding, card acceptance and this checkpoint are the
durable recovery basis.
