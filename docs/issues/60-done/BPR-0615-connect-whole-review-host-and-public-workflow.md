---
schema: "implement-rfc.card.v1"
id: "BPR-0615"
title: "Connect whole-review host and public workflow"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0613","BPR-0614"]
---

# Connect whole-review host and public workflow

Whole-review policy and provider recovery now exist, but the application still
selects only the source-line Comment path. Connect the complete PR-level review
workflow so Comment, Approve and Request Changes are ordinary, explicit user
actions with the account and revision shown before submission. BPR-0144 retains
the separate designated live-account checks after this implementation.

## Ownership and implementation boundary

Application: whole-review async host, typed projections and per-request
execution authority; bounded pending-request discovery through the recovery
port/store where needed. Core: compact decided control state for the existing
whole-review draft policy. Qt: a distinct whole-review summary/disposition
editor and its workbench entry. App: real provider/store/host selection,
request lifecycle, presentation and shutdown. Own their focused unit,
provider/storage, Qt and composition fixtures and CMake registrations. The
integrator owns documentation, board state and commits. Establish exclusive
worker paths before delegation; no concurrent source writers.

Preserve source-line editing and its existing publication course. Keep the
whole-review summary independent of the selected file/line. Do not migrate
unrelated JSON, expand deferred logging/schema work, change accounts, mutate
GitHub or revise a previously answered approval packet in this implementation.

## Acceptance

- The async host owns provider effects, stop sources and pending request
  correlation. Effect callbacks post typed terminals on the serialized decision
  lane; they never mutate core/Qt state. Every accepted request reaches one
  terminal, including cancellation/shutdown. Reject stale session/generation,
  draft and attempt identity before processing its summary or replacing state.
  Keep the decision service alive through host quiescence.
- Preserve the complete BPR-0613 request and one idempotency key through its
  attempt and restart recovery. Discover or restore pending requests within
  the exact account/PR scope, bounded by existing store capacity. Reopening or
  refreshing must not turn indeterminate publication into a fresh POST. Keep
  storage effects off the decision/Qt lane and recover read-only until the
  original outcome is known. Published receipts must validate against the
  original request before completing that exact core attempt.
- Resolve BPR-0614's known moved-head recovery limitation: an already admitted
  possibly-transmitted review may be found at its original head after the PR
  moves, using exact target/account identity and the frozen request/baseline.
  This read-only recovery must never authorize a write. Fresh publication still
  requires re-observation of the exact current expected head immediately before
  POST. Exercise the concrete lost-response, moved-head, original-review-present
  counterexample and assert zero recovery POSTs.
- Keep whole-review execution authority distinct from source-line authority.
  An earlier source-line approval cannot authorize a whole review. Wire the
  whole-review authority required by ordinary composition, binding target,
  acting identity, summary and disposition to the admitted request. Exercise
  refusal without authority, exact one-use admission, revocation and replay.
  Existing default-deny automated-test policy remains in force; no UI model,
  flag or fabricated approval may widen a live test course.
- Present host, owner/repository, PR number, expected head, actual acting
  identity, summary and selected disposition before Submit review. The normal
  workbench exposes this workflow without a selected source line. Comment and
  Request Changes require nonblank text; empty Approve works. Discard and
  pending/terminal enablement follow core decisions. File navigation preserves
  the PR-scoped draft; target/account changes cannot silently rebind it.
- Qt emits typed gestures and applies decided projections only. Show useful,
  selectable/copyable outcomes for refusal, pending reconciliation and success;
  do not report a timeout as definite nonpublication. Never expose credentials
  or protocol internals. Preserve focus, keyboard access, accessibility labels
  and supported narrow layouts.
- Ordinary composition selects the whole-review host/provider/store and
  retains them through pending work and shutdown. A public fixture must open
  a PR, use the normal controls for each disposition, observe the exact
  correlated result, and prove one submitted review with the expected account,
  head and prose. Cover no-anchor use, stale terminal, duplicate gesture,
  identity/head drift, failure, cancellation and restarted pending recovery.
  Retain the source-line public fixture as a regression check.

## Verification and continuation

Use focused host/core, provider/storage, Qt and real composition selectors.
Render representative normal-control states and inspect identity, disposition,
summary, status and layout; exercise keyboard/accessible controls deterministically.
Use isolated fixtures and ordinary build roots. Run affected architecture and
isolation checks, scoped formatting, documentation structure and diff checks.
No full matrix, live credentials or publication are needed for this slice.

After implementation, prepare BPR-0144's concrete live Comment/Approve/Request
Changes course using a suitable designated target/account. MikeMarcin authored
BPR-Test#1 and cannot use it to prove own-PR approval. Do not switch accounts,
create a PR or assume extra remote mutation authority. BPR-0143's four approved
mixed-client actions remain separately parked on the Computer Use restriction.

## Recovery integration progress — 2026-09-05

The first implementation slice adds effect-lane pending discovery through the
provider and SQLite recovery port. It returns the original request/key, validates
stable target and actor identity, rejects corrupt records atomically, preserves
prepared attempts, and respects the existing 1,024-record scope capacity.
Historical read-only recovery now finds the exact original-head review after
head/base/ref movement; fresh publication retains the strict current-head check.
A separate private Glaze DTO validates historical PR identity without weakening
the source-line parser. Shared SQLite schema stays v3.

Verification: five affected dev targets built. The focused command below passed
8/8 in 79.86 seconds, including normal-exit fresh-process discovery/recovery,
prepared restart, moved-head recovery with zero POSTs, actor/PR refusal, storage
scope/capacity/integrity and existing source-line regressions:

```sh
ctest --test-dir build/dev --output-on-failure -R '^(github_mutation_contract|live_review_mutation_provider|sqlite_review_mutation_recovery_storage|live_review_submission_provider|live_review_submission_restart_recovery|sqlite_review_submission_recovery_storage|architecture_boundaries|architecture_boundary_contract)$'
```

After adding malformed/missing/overflow/fractional/duplicate historical identity
cases, the rebuilt `github_mutation_contract` passed 1/1 in 0.40 seconds. Scoped
formatting and code-simplifier review completed. No live effects, app launch,
credential/log access, new build root or retained temporary fixture occurred.
This is not abrupt-kill or power-loss evidence. A prepared attempt remains
unresolved after restart; discovery cannot prove another process never sent it.
The delegated worker hit its usage limit; the integrator completed and verified
the partial changes locally. No additional independent review ran for this
bounded correction; the earlier BPR-0614 counterexample is now directly covered.

Final diff inspection also corrected read-only refusal semantics: absent local
records or wrong recovery authority remain indeterminate, and a concurrent
admission winner receives the same stable actor/PR observation before historical
reconciliation. The rebuilt provider/restart checks passed 2/2 in 0.66 seconds.
Documentation structure, all 62 documentation tests and `git diff --check` passed.

Remaining in this card: async host and exact one-use whole-review authority,
normal PR-level editor, real composition/lifecycle and public-control fixtures.
The four BPR-0143 live actions remain approved and unexecuted.


## Async host progress — 2026-09-05

The second slice implements `review_submission_application_host`, a separate
one-use whole-review grant and a private user-submit issuer. The product
implementation proceeds with normal authenticated Submit review as the stated
recommended assumption, following the user's continuation; this is not recorded
as an owner verdict or live-publication authorization. Source-line attended
approval remains a distinct path. Production issuance refuses in test builds.

The host accepts one operation at a time, constructs and destroys each provider
on the effect worker, retains exact request/key context, and posts one terminal
on the decision lane even through cancellation/shutdown. Read-only discovery and
reconciliation need no new publication grant. Stale/invalid results remain
indeterminate for the original request. Fixture tests cover all dispositions,
no grant, exact scope, summary/disposition mismatch, replay, revocation, expiry,
session/generation mismatch, pending discovery, stale attempts, invalid receipts,
provider exceptions, enqueue failure, reentrant projection feedback and shutdown.
The fixture directly checks provider creation/destruction stays off the decision
lane. The core continues to own draft legality; no Qt or app selection is added
in this slice.

Verification:

- Dev host target and production `build/app-release` application-library target
  built successfully. Production application launch was not performed.
- The eight-selector command below passed the host, prior source-line host,
  architecture and isolation checks. Two pre-existing core/contract binaries
  were absent in `build/dev`; after building those targets, their focused rerun
  passed 2/2 in 0.60 seconds. No product assertion failed.

```sh
ctest --test-dir build/dev --output-on-failure -R '^(application_review_submission_application_host|application_review_mutation_application_host|application_review_submission_contract|core_review_submission|architecture_boundaries|architecture_boundary_contract|test_network_isolation_manifest|test_network_isolation_structure_contract)$'
ctest --test-dir build/dev --output-on-failure -R '^(core_review_submission|application_review_submission_contract)$'
```

Scoped format and code-simplifier inspection passed. Documentation and diff
gates pass. Existing ordinary build roots only; no retained temporary fixtures,
credentials, logs or remote mutations. Optional independent host/authority
review remains useful once real composition is connected; the available worker
runtime previously exhausted its usage limit and was not repeatedly retried.

Next: add core-decided controls and a separate `review_submission_editor` in the
workbench conversations drawer, then wire accepted PR handoff, provider factory,
pending restoration, core commands/terminals and shutdown in real composition.
Use a separate `review_submission_composition_public_path.cpp` fixture executable
through ordinary controls rather than extending the main CLI state machine.
Preserve PR drafts on file navigation and identical refresh; changed authority
must not silently rebind a draft. The app must retain the executor/projection
through host quiescence. Add production selection/compile-seal checks and source
manifest updates when these new components become selected. BPR-0615 remains
In Progress and no RFC acceptance or live course is closed by this host slice.

## Public workflow integration — 2026-09-05

The normal conversations drawer now exposes a separate whole-review editor.
Core controls decide its edit/submit/discard/reconcile/new-review actions. The
composition selects an effect-worker provider factory, which opens/closes SQLite
and the concrete provider off the Qt lane, and a separate exact user-submit grant.
The extracted `review_submission_workflow` owns PR-scoped draft/pending context,
original head/account/body, read-only restoration and lifecycle. Authentication
invalidation is independent of file-ledger availability. Retired results cannot
complete or rebind a current draft. No existing source-line grant is reused.

The real public-control fixture uses the normal PR open, file selection, review
choice/summary and actions. All three dispositions, empty Approve, duplicate
submission, definitive refusal, stale result, lost response, reconstructed app
with a moved head, original-key read-only recovery, fake-account invalidation
and shutdown with a held terminal are covered. The typed fake server is separate
from real SQLite; this is not a live-GitHub or abrupt-kill result. The rendered
window exposed a clipped whole-review identity in a manually narrowed drawer;
the editor now follows its visible width and the public fixture checks that bound.
The independent editor tests exercise literal/copyable text, normal keyboard
submission, focus/cursor preservation, pending controls and three scales.

Verification for this integration:

- Dev composition, application and focused test targets built. The production
  tests-disabled `build/app-release` composition target also built; no launch.
- Core `core_review_submission` passed in dev and core-only (the latter 1/1,
  0.35 seconds). Earlier provider, storage and host results above remain valid.
- Ten existing architecture, isolation, composition and source-line/Qt regression
  selectors passed 10/10 in 263.23 seconds. The source inventory and production
  manifest were reviewed and repinned for the selected workflow files.
- The new production compile-seal test passed (1 test, 3.257 seconds): separate
  source-line/whole-review grant types, fixture factory absent from production,
  and private whole-review grant construction. The test-build issuer's refusal
  remains covered by the host suite from the previous slice.
- Expanded narrow-layout verification initially used a malformed fixture OID;
  that test input was corrected. Final focused render/control results are
  recorded below after the affected rerun. No product failure was hidden.

Scoped code-simplifier review reduced dense command gating and kept the workflow
out of the already-large app implementation. No new independent review was
selected; direct host/provider/public-path evidence and source inspection cover
this slice. Existing worker usage-limit failures were not retried or reset.
Only ordinary build roots are used; disposable SQLite/server fixtures clean up
automatically. No GitHub Actions, pushes, remote mutations, production credentials
or remembered sign-in changes occurred. BPR-0143 remains separately parked;
BPR-0144 needs a suitable designated PR/account for real attribution.

The identity field uses a native read-only text view with visual wrapping, so a
long head is fully visible without changing the text copied by the reviewer.
The public fixture also refreshes a local unsent draft to a new server head:
Submit stays disabled, original summary/head remain frozen, and only explicit
Discard then New review binds a new empty draft to that revision. No submission
occurs in that course.

Commands for the final changed UI and selected-path verification:

```sh
ctest --test-dir build/dev --output-on-failure -R '^(application_review_submission_composition_public_path|application_review_round_trip_composition_contract|qt_review_submission_editor_scale_(100|125|200)|qt_workbench_window_scale_(100|125|200))$'
python3 tests/end_to_end/composition_contract_tests.py CompositionContractTests.test_whole_review_authority_is_distinct_and_test_factory_is_private
ctest --test-dir build/core-only --output-on-failure -R '^core_review_submission$'
```

Final eight-selector UI/public-path rerun passed 8/8 in 53.52 seconds, including
the existing source-line round-trip and the new fresh-head draft refusal course.
The production tests-disabled application executable linked successfully in
`build/app-release`; it was not launched. Scoped formatting, documentation
structure, all 62 documentation tests and `git diff --check` passed. Full matrices,
`coding-tidy`, native Windows/Linux, live publication and subjective owner visual
acceptance were not run. The inspected render is an offscreen layout result.

After the final identity word-wrapping refinement, the three scaled editor tests
and whole-review public course passed 4/4 in 2.72 seconds. The final 1280×900
full-window render and 300-pixel editor render were inspected: the entire head,
account, disposition, summary, controls and outcome are visible and copyable.
The production application executable was rebuilt successfully after that
refinement. The three task-created build logs were removed after inspection;
only ordinary build outputs and deterministic render outputs remain.

Implementation commits: `f79c3628` (historical pending recovery), `2e527a03`
(async host/authority), and `2bc5841a` (normal controls, composition and public
fixtures). All BPR-0615 acceptance is complete. Its live attribution successor
BPR-0144 is parked separately; this is not RFC-BPR-0008 closeout.
