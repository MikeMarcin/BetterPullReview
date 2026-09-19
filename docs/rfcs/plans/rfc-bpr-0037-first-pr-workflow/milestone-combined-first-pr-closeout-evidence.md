# Combined first-PR closeout evidence

## Current frontier

- Run/dispatch: `rfc0037-run-20260918` / `rfc0037-dispatch-004`; producer
  `/root/rfc0037_closeout`; mandate revision `1`.
- Binding: authored milestone `combined-first-pr-closeout`, recorded in
  `assignment-rfc0037-dispatch-004.json` in this directory. The bound work atom
  is BPR-0927 and all four authored exits remain live.
- Reconciliation: starting revision
  `75e94c28fc7874aee99b76d4e62bb616365e8d85` and registration baseline
  `9bd2002b859ce4c5599d86dcc51771aa365fa90b` match the dispatch. The prior
  `thread-reading-integration` return reports quiescent ownership and points to
  accepted revision-bound evidence; live fingerprint drift is expected from
  accepted card transitions and will not reopen engineering without a concrete
  contradiction.
- Card ownership: BPR-0927 was dependency-ready, promoted in `e8004fd8`,
  claimed by this integrator in `41d123ff`, and moved to Done in `b8ff4ed7`
  after acceptance. No RFC-BPR-0037 card remains active.
- Canonical status constraint: RFC-BPR-0037 is still `Proposed`; its listed
  dependencies RFC-BPR-0008, RFC-BPR-0013, RFC-BPR-0015, RFC-BPR-0028 and
  RFC-BPR-0029 must be reconciled before any `Auto-Verified` transition. This
  milestone may accept the technical closeout while truthfully preserving the
  RFC status if those dependencies remain nonterminal.
- Owned paths: BPR-0927, this assignment/evidence/acceptance/return, and the
  RFC/index only if their actual status may truthfully change. Scoped source or
  tests may be repaired only if a closeout check exposes an in-scope regression.
- Dispatch ledger: native `spawn_agent` is available; `followup_task` resumes
  an idle child and `send_message` delivers to an active child. Two read-only
  scouts were dispatched with fresh non-history contexts and no per-call
  model/effort overrides: `/root/rfc0037_closeout/rfc_scout_closeout_evidence`
  owns the criterion/evidence inventory, and
  `/root/rfc0037_closeout/rfc_scout_combined_course` owns composed-test surface
  discovery. Their generated `rfc-scout` configuration is
  `gpt-5.6-sol` / `low`; both returned that configured banner. Independent
  serving telemetry is unavailable and is not inferred from those banners.
  `/root/rfc0037_closeout/rfc_worker_combined_course` was subsequently
  dispatched with generated role `rfc-worker`, a fresh non-history context,
  no per-call model/effort override, and exclusive ownership of
  `tests/end_to_end/setup_destination_public_path_tests.cpp`. Its generated
  configuration is `gpt-6-astra` / `low`; it returned that configured banner.
  Independent serving telemetry is unavailable. The integrator then dispatched
  `/root/rfc0037_closeout/rfc_test_runner_closeout_batch` with generated role
  `rfc-test-runner`, a fresh non-history context and no per-call model/effort
  override to run the exact five-selector integration batch at `e4096e05`.
  Its generated configuration is `gpt-5.6-luna` / `xhigh`; it returned that
  configured banner. Independent serving telemetry is unavailable. The final
  `/root/rfc0037_closeout/rfc_reviewer_final_closeout` dispatch used generated
  role `rfc-reviewer`, a fresh non-history context and no per-call model/effort
  override to inspect exact candidate `35195547`, including the test-producing
  revision `e4096e05`. Its generated configuration is `gpt-6-astra` / `medium`;
  it returned that configured banner. Independent serving telemetry is
  unavailable.
- Commands and writers: both scouts, the combined-course worker, the test
  runner and the final reviewer are complete and quiescent. They own no
  remaining edit or command. The promotion/claim and integrator verification
  commands are also complete.
- Pending obligation: no engineering, human or target-native obligation remains
  for this technical closeout. The RFC status transition remains separately
  gated by its nonterminal direct dependencies.
- Next discriminating step: none inside this technical milestone. A later
  status-only reconciliation becomes eligible after the five named direct
  dependencies are `Auto-Verified`.

## Evidence audit before the combined course

- AC-01 through AC-09 each have accepted revision-bound evidence in
  `acceptance-BPR-0918.json` through `acceptance-BPR-0929.json`; every cited
  producing/correction commit resolves locally. No prior milestone has a
  concrete unresolved engineering contradiction.
- Exit 1 is supported across the accepted source and thread milestones, not by
  one monolithic test. Initial load, update history, two files, head change,
  unavailable/failure and stale replacement are bound to `5b95485c`; empty,
  filtered-empty, refresh/failure, authority replacement and retained drafts
  are bound to `8939de71`; Discuss selection under comparison movement is
  corrected by `e3258509`. Successful in-process Refresh recovery was not
  observed; healthy relaunch over the same durable store is the demonstrated
  recovery and remains an explicit limitation.
- Exit 2 has one real gap: no existing selector continuously loads the recorded
  PR, reaches retained history and both exact source files, enters Review and
  then reaches PR-wide Discuss/Compose without a file-selection dependency.
  `application_setup_destination_public_path` is the narrowest existing public
  course to extend; the other selectors prove only partitions of that journey.
- Exit 3 reuses the three explicit macOS attended accepts from session
  `c44d1fe1c8245aa53959afc9fc9f4b27` at reviewed source `8939de71` and app
  SHA-256 `5b8b23dc...362a`, plus the native Cocoa DPR-2 course. Windows, Linux,
  live GitHub/GHES, a literal `gh` subprocess, and private-service behavior are
  not claimed.
- Exit 4 still requires current-candidate documentation, board and combined
  focused gates. BPR-0927's old body is documentary drift and must be replaced
  with the final evidence rather than treated as an engineering invalidation.

## Combined recorded-fixture candidate

- Candidate `e4096e050b856eed1f103c215701d3cf289d159f` extends only
  `application_setup_destination_public_path`. Its single GHES/CLI course now
  begins with the existing refused acquisition and explicit unavailable state,
  recovers through the same durable store, loads two retained updates, enters
  Review through the natural Overview **Start review** gesture, and proves the
  exact base/head first lines for `review.cpp` and `secondary.cpp`.
- Before any file or comparison selection, the same course opens PR-wide
  Discuss and observes exact pathless issue-comment/review identities and an
  inline `review.cpp` comment with their recorded bodies. Discuss leaves the
  file and comparison unselected. After the two exact sources are exercised,
  the course opens the retained Compose review page, types a local draft,
  traverses Discuss and both files, returns to the same draft and exact selected
  source, and verifies zero review submissions, inline publications, replies or
  resolutions.
- Continue review is not redundantly re-proven inside this new combined tail.
  Its natural seed/reuse behavior and zero-publication result remain bound to
  the unchanged passing `application_review_behaviour_contract` at `5b95485c`.
  An exploratory attempt to drive its Settings preference from this fixture did
  not reach accepted behavior and was removed without a production change; it
  is not represented as evidence or a new defect.
- The worker's focused build and selector passed. After a behavior-preserving
  code-simplifier pass extracted the repeated PR-wide discussion check into a
  named local helper, the integrator rebuilt the target and reran
  `application_setup_destination_public_path`: 1/1 passed in 32.16 seconds;
  `clang-format --dry-run --Werror` and `git diff --check` also passed.

## Closeout verification

- At candidate `e4096e05`, the independently executed integration batch selected
  `application_setup_destination_public_path`,
  `application_review_behaviour_contract`,
  `application_review_submission_composition_public_path`,
  `qt_github_conversation_panel` and `qt_workbench_window_scale_100`: 5/5 passed
  in 145.31 seconds. The command exited zero and is quiescent.
- `python3 scripts/verify_documentation.py --root .` returned
  `DOCUMENTATION_OK`; all 64 documentation verifier tests passed; RFC-scoped
  board validation reported 921 cards, `valid: true`, zero errors; and
  `git diff --check` passed.
- Independent review of exact documentation candidate `35195547` and the test
  diff from the dispatched starting revision through `e4096e05` found no
  blocking or non-blocking issue. The reviewer independently ran diff checking,
  inspected the retained five-test result, and confirmed that technical card
  completion while the RFC/index remain `Proposed` respects the explicit status
  gate. All reviewer commands are quiescent and no files were changed.
- The reviewer also confirmed the recorded material limits: exact source
  identity and first-line content are asserted rather than whole-file byte
  equality; target-native evidence is macOS-only; and live services, literal
  `gh`, private repositories and successful in-process Refresh recovery remain
  unclaimed.
- The historical BPR-0915 shared-sign-in expectation drift is not present in
  the selected current boundary and was not used as passing closeout evidence.
  BPR-0797's protected executable digest mismatch remains owned by that separate
  workflow; this closeout neither touches its candidate/registry nor interprets
  an automated fixture as unrestricted browser or live-CLI capability.
- RFC-BPR-0037 remains `Proposed`: RFC-BPR-0003 is `Auto-Verified`, while
  RFC-BPR-0008, RFC-BPR-0013, RFC-BPR-0015 and RFC-BPR-0028 are `Proposed`, and
  RFC-BPR-0029 is `Accepted`. Finishing the technical closeout without changing
  the RFC/index status is the canonical-order pass condition.
- `issue_board.py finish` accepted `acceptance-BPR-0927.json`, moved BPR-0927
  from `30-in-progress` to `60-done`, and committed the transition as
  `b8ff4ed70444ec7d2d419a77f367ec736f326b70`.
- The final handoff checker validated the return structure, exact dispatch and
  quiescent execution, then requested reconciliation because the live BPR-0927
  fingerprint changed from assigned
  `sha256:843272d0c547449edb1d363496e11144eab148fc41166fb0672002af1130e7c0`
  to
  `sha256:480f380af23290a260afe8ca63c01f0e73c63f8ccfd70f38cdac5866bb13cb0c`.
  The milestone definition,
  exit criteria and prerequisite fingerprint are unchanged; the sole affected
  requirement change is the accepted card's documented In Progress-to-Done
  transition. The designated integrator therefore reconciles that expected
  board drift without reopening accepted engineering.
