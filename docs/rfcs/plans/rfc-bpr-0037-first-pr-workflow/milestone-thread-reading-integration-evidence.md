# Thread-reading integration evidence

## Current frontier

- Run/dispatch: `rfc0037-run-20260918` /
  `rfc0037-dispatch-003-resume-02`; producer `/root/rfc0037_m3`; mandate
  revision `1`. This renews the interrupted dispatch without changing its
  milestone binding, scope or authority.
- Binding: authored milestone `thread-reading-integration`, recorded in
  `assignment-rfc0037-dispatch-003.json` in this directory. The authored work
  atoms are BPR-0924 and BPR-0925. Exit 4 additionally binds the design
  milestone's allocated runtime follow-ups BPR-0928 and BPR-0929. All four
  cards are now Done.
- Prior reconciliation: `overview-conversation-decisions` remains accepted.
  Its expected-completion fingerprint drift is solely the accepted cards'
  move to Done; revision-bound records, evidence and the unchanged plan agree,
  so no prior engineering outcome was reopened.
- Plan disposition: the live plan remains unchanged. The installed amendment
  checker requires every obligation atom and exit criterion to be preserved,
  so adding new work atoms to this complete authored plan is not a valid
  refinement. BPR-0928/BPR-0929 are instead retained under the existing
  milestone exit, their canonical cards and BPR-0927 dependency ledger.
- Candidate and cards: implementation is `cc03874861abd658cce11812f15d887607919d1b`,
  analyzer cleanup is `8939de7195c7de6c2dc4299c3f5e1f0d342bf115`, and the
  post-attended Discuss correction is
  `e3258509dace4ea0b8cda7ff9e87cfa0ea183b56`, followed only by test formatting
  `bd3976750dd4db8cd070b6766413c32093c8cfeb`. BPR-0925 finished in
  `6c604a66`; BPR-0924, BPR-0928 and BPR-0929 finished from their accepted
  verdicts in `9d660a48`, `c4c5289a` and `2135a6b9`.
- Binding and claim commits: `2773f730` bound the milestone; `6f0030b9`,
  `802a6f47`, `8996109f` and `24c9e90c` claimed BPR-0924, BPR-0925,
  BPR-0928 and BPR-0929. Attended handoff is `06a2a915`; the three
  human-pending transitions before verdict import are `60c5166b`, `d772ed92`
  and `ee6d355e`.
- Owned paths: BPR-0924/BPR-0925/BPR-0928/BPR-0929 cards; this assignment,
  evidence, acceptance and return; necessary scoped source/tests/docs.
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
- Ownership: `/root/rfc0037_m3` owns final integration. All three scouts, all
  three implementation workers and the independent reviewer are complete;
  no leaf owns a writer or command. No test/build command remains active.
- Integrated behavior: the Overview body has a compact preview and retained
  central reader; whole-review and inline composition use dedicated retained
  pages; Discuss owns the center, defaults to the full PR and keeps explicit
  file/unread/unresolved filters; its compact index, graph and single full-body
  detail share identity; long detail owns pixel wheel, angle wheel and keyboard
  scrolling. The focus-only Show review threads action is absent. Accepted AI
  workshop proposals open the retained inline composition page rather than a
  hidden former drawer editor. Previous/Next change may move the retained code
  comparison beneath Discuss but now preserves the selected conversation and
  full detail.
- Attended decisions: session `c44d1fe1c8245aa53959afc9fc9f4b27` reached
  sequence `18` with explicit accepts for Compose
  (`7e3a3b36-e1d0-417a-ba8e-7343b5f6ffbd`), Discuss
  (`e5968d67-c5b5-49e9-b0f0-f869a73737d0`) and Overview
  (`1c899dcc-9d2c-4868-906b-aa9fb2b1dbf6`). The accepted revision was
  `8939de71-5b8b23dc...362a-2026-09-19.2`. All three candidates' protections
  were explicitly released at session sequence `21` after verdict import.
- Verdict adjudication: Compose's question and screenshot identify no criterion
  failure, so its explicit accept stands. Discuss's note that change navigation
  hid the conversation was a concrete contract defect; `e3258509` adds a
  Discuss-only comparison-selection guard and an actual Previous/Next toolbar
  regression. Overview's syntax-colour, resource-file highlighting and fixture
  Recent observations are outside the retained-description-reader criterion
  and this milestone's Markdown/authentication scope; they were preserved as
  engineering input and did not reopen the explicit accept.
- Integrated verification: the corrected 15-row focused batch passed panel,
  graph at three scales, Overview, review editor at three scales,
  agent-finding composition/workshop and review-submission public path.
  Exact off-file Reply, narrow activation and same-file Open-in-code regressions
  pass. After `e3258509`, `qt_workbench_window_scale_100`, `_125` and `_200`
  all pass with Previous/Next change preserving Discuss identity.
- Independent review: the same `rfc-reviewer` inspected the attended note and
  two-file correction and reported no blocking or non-blocking finding. It
  confirmed the guard is Discuss-only and leaves non-Discuss synchronization
  unchanged; reviewer state and commands are quiescent.
- Native objective evidence: the network-isolated Cocoa panel executable
  passed on a Retina display at DPR 2. Long-detail scroll ranges were nonzero
  at 900/1400 widths and 13/20-point fonts; pixel/angle wheel and
  Home/End/PageUp assertions passed. The accepted attended decisions supply
  the physical-trackpad, VoiceOver, focus and native appearance judgments.
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
- Attended evidence remains at
  `docs/verification/reviews/rfc-bpr-0037-thread-reading-v001/`; the reviewed
  dedicated bundle SHA-256 is
  `5b8b23dcd7ed735c544b7bd78d81f1404b910b4cd09e9e610a787dacd15a362a`.
- Unrelated protected-resource incident: the first preparation correctly
  refused the mutable `build/dev` app because prior candidate
  `bpr-0797-device-browser-opens` still protects that resource at expected
  version `abe15f40...d577`. This integrator had already relinked the dev app
  to `5b8b23dc...362a` before consulting the machine registry. The prior
  candidate and registry were not released or edited; its recorded native
  decision remains, but its protected executable path no longer matches its
  expected version and requires its owning workflow to reconcile. This RFC's
  queue uses the separate immutable copy above and is unaffected.
- Next objective: hand off a fresh integrator context for
  `combined-first-pr-closeout`; this milestone does not claim pack completion.
- Native handoff check: the supplied resume-02 assignment and return validate
  under quiescent execution with action `fresh-integrator`, reason
  `integrated-boundary`; the checker intentionally leaves engineering
  acceptance to this integrator.

Historical accepted milestone evidence remains in the earlier evidence files;
this milestone does not reopen those engineering outcomes.
