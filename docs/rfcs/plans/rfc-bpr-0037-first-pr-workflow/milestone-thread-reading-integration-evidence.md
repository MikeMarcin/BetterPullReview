# Thread-reading integration evidence

## Current frontier

- Run/dispatch: `rfc0037-run-20260918` /
  `rfc0037-dispatch-003-resume-01`; producer `/root/rfc0037_m3`; mandate
  revision `1`. This renews the interrupted `rfc0037-dispatch-003` without
  changing its milestone binding or authority.
- Binding: authored milestone `thread-reading-integration`, recorded in
  `assignment-rfc0037-dispatch-003.json` in this directory. The authored work
  atoms are BPR-0924 and BPR-0925. Exit 4 additionally binds the design
  milestone's allocated runtime follow-ups BPR-0928 and BPR-0929; the current
  mandate requires implementing them before acceptance.
- Prior reconciliation: `overview-conversation-decisions` remains accepted.
  Its return checker requests reconciliation only because moving the four
  accepted cards to Done changed their source fingerprints. The Done cards,
  revision-bound acceptance records, final evidence, current plan definition
  and quiescent ownership agree; no engineering contradiction was found.
- Plan disposition: the live plan remains unchanged. The installed amendment
  checker requires every obligation atom and exit criterion to be preserved,
  so adding new work atoms to this complete authored plan is not a valid
  refinement. BPR-0928/BPR-0929 are instead retained under the existing
  milestone exit, their canonical cards and BPR-0927 dependency ledger.
- Candidate: reviewed implementation revision
  `cc03874861abd658cce11812f15d887607919d1b`, followed only by analyzer naming
  cleanup `8939de7195c7de6c2dc4299c3f5e1f0d342bf115`. Binding was committed as
  `2773f730`; claim commits are `6f0030b9`, `802a6f47`, `8996109f` and
  `24c9e90c` for BPR-0924, BPR-0925, BPR-0928 and BPR-0929 respectively.
  BPR-0925's fully mechanical graph/detail acceptance is Done in `6c604a66`;
  the three cards with required native judgment retain human-pending
  acceptance records and will be parked against the attended queue.
- Owned paths: BPR-0924/BPR-0925/BPR-0928/BPR-0929 cards; this assignment,
  evidence, acceptance and eventual return; necessary scoped source/tests/docs.
- Completed checks: consumer configuration, governing instructions, RFC,
  four cards, authored plan, prior return/evidence and both prior accepted
  milestone bindings inspected. Branch is `main`; prior owners and commands
  are quiescent. The pre-change focused Qt/application batch passed 10 of 11:
  Overview, panel/cards, graph at three scales, review editor at three scales
  and review-submission composition passed. `qt_composed_github_scale_100`
  reproduced the existing pre-RFC-0037 BPR-0915 CLI-warning assertion before
  reaching the changed surfaces; it remains negative integration context.
- Delegation route ledger: scouts
  `/root/rfc0037_m3/rfc_scout_overview_reader`,
  `/root/rfc0037_m3/rfc_scout_compose_discuss` and
  `/root/rfc0037_m3/rfc_scout_thread_graph` were dispatched with generated
  role `rfc-scout`, fresh non-history contexts and no model/effort override.
  Each returned the configured banner `gpt-5.6-sol` / `low`; independent
  serving telemetry is unavailable. Writers
  `/root/rfc0037_m3/rfc_worker_overview_reader`,
  `/root/rfc0037_m3/rfc_worker_compose_pages` and
  `/root/rfc0037_m3/rfc_worker_conversation_detail` were dispatched with
  generated role `rfc-worker`, fresh non-history contexts and no model/effort
  override. Each returned the configured `gpt-6-astra` / `low` banner and a
  quiescent working state; independent serving telemetry is unavailable.
- Active owners/commands: `/root/rfc0037_m3` owns integration and all board,
  commit and acceptance work. All three read-only scouts, all three standard
  implementation owners and the independent reviewer completed. Their
  commands are quiescent; no leaf owns a live writer or build. The reviewed
  source and focused tests are committed as `cc038748`; `coding-tidy` is the
  sole active command while the integrator prepares the attended handoff.
- Integrated behavior: the Overview body has a compact preview and retained
  central reader; whole-review and inline composition use dedicated retained
  pages; Discuss owns the center, defaults to the full PR and keeps explicit
  file/unread/unresolved filters; its compact index, graph and single full-body
  detail share identity; long detail owns pixel wheel, angle wheel and keyboard
  scrolling. The focus-only Show review threads action is absent. Accepted AI
  workshop proposals open the retained inline composition page rather than a
  hidden former drawer editor.
- Integrated verification after reconciliation: the full dev build passed.
  The corrected 15-row focused batch passed panel, graph at three scales, Overview,
  review editor at three scales, agent-finding composition/workshop and the
  review-submission public path. Its first run exposed two new regression-test
  failures: arrow navigation's test assumed a filtered-out identity, and the
  new off-file fixture exposed loss of the explicitly selected identity across
  Open-in-code. Both were corrected; cards and all three workbench scale rows
  then passed. The independent reviewer had previously found two blockers:
  off-file Reply could dispatch before exact path validation and Up/Down could
  open narrow detail. Exact path/endpoint gates, reordered dispatch, explicit
  activation signaling and regressions now pass. The required correction
  re-review initially encountered a service usage-limit failure, then resumed
  in the renewed dispatch. It confirmed those two fixes and found one remaining
  AC-09 defect: Open in code retained identity but did not explicitly reveal a
  same-file line. The corrected path now leaves Discuss, validates the selected
  thread path against the current comparison, and explicitly synchronizes the
  exact guarded line. Its regression asserts the right-side zero-based line 21
  anchor for the line-22 fixture; all three workbench scale rows pass. The same
  reviewer accepted the correction with no remaining blocking or non-blocking
  finding. Reviewer commands are quiescent; independent serving telemetry was
  unavailable.
- Native objective evidence: the network-isolated Cocoa panel executable
  passed on a Retina display at DPR 2. Long-detail scroll ranges were nonzero
  at 900/1400 widths and 13/20-point fonts; pixel/angle wheel and
  Home/End/PageUp assertions passed. Direct native Overview/workbench focus
  assertions could not acquire foreground focus from the sandboxed terminal,
  while the equivalent offscreen tests pass. Physical trackpad, VoiceOver and
  subjective appearance remain explicit attended criteria.
- Static/documentation gates: `git diff --check`, `coding-format-check`, the
  documentation gate and all 64 documentation self-tests pass. The full-tree
  `coding-tidy` completed and exposed existing baseline diagnostics outside
  this milestone, including the agent supervisor, Git/GitHub transports,
  `home_view_state.hpp`, `dif_runtime.hpp` and older fixture/test naming. Its
  scoped pass also found three naming/static-access diagnostics introduced in
  this slice; those mechanical corrections are committed as `8939de71`, their
  three strict translation units now pass with warnings-as-errors, and the
  affected Overview plus three editor-scale tests pass. The final dev build
  relinked successfully.
- Attended handoff: session `c44d1fe1c8245aa53959afc9fc9f4b27`
  sequence `15` has three ready/protected candidates. Their dedicated bundle
  is `build/attended/rfc-bpr-0037-thread-reading-v001/better_pull_review.app`,
  whose executable SHA-256 is
  `5b8b23dcd7ed735c544b7bd78d81f1404b910b4cd09e9e610a787dacd15a362a`.
  The offline queue and operator packet are under
  `docs/verification/reviews/rfc-bpr-0037-thread-reading-v001/`.
- Unrelated protected-resource incident: the first preparation correctly
  refused the mutable `build/dev` app because prior candidate
  `bpr-0797-device-browser-opens` still protects that resource at expected
  version `abe15f40...d577`. This integrator had already relinked the dev app
  to `5b8b23dc...362a` before consulting the machine registry. The prior
  candidate and registry were not released or edited; its recorded native
  decision remains, but its protected executable path no longer matches its
  expected version and requires its owning workflow to reconcile. This RFC's
  queue uses the separate immutable copy above and is unaffected.
- Worker verification: Overview reader passed its focused target at 1x/1.25x/
  2x; conversation panel/cards/graph passed five focused rows plus long-body
  900/1400-wide, 13/20-point evidence at 1.25x/2x; Compose/workbench and review
  editor passed all three scale rows. Changed files passed owned formatting and
  whitespace checks. Actual native trackpad/appearance/accessibility verdicts
  remain pending an integrated attended candidate.
- Next discriminating step: capture and import explicit human verdicts for the
  three protected candidates, then reconcile the parked card acceptance
  records. Do not begin combined-first-pr-closeout while they remain pending.

Historical accepted milestone evidence remains in the earlier evidence files;
this milestone does not reopen those engineering outcomes.
