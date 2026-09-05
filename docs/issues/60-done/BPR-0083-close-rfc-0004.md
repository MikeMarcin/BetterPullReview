---
schema: "implement-rfc.card.v1"
id: "BPR-0083"
title: "Verify and close RFC-BPR-0004"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0054","BPR-0072","BPR-0073","BPR-0074","BPR-0075","BPR-0076","BPR-0077","BPR-0078","BPR-0079","BPR-0080","BPR-0081","BPR-0082","BPR-0227","BPR-0234","BPR-0474"]
closeout_for: ["RFC-BPR-0004"]
---

# Verify and close RFC-BPR-0004

RFC base: `243295a70756b2e63a785ed11b14647b6b0a4c6d`.

## Final-state correction

BPR-0225 found that the final application requests a selected diff only
through `github_application_fixture_access`; no reviewer gesture calls the
composition's diff request. The earlier critical review's semantic and
component findings remain useful, but its no-helper-only public-path conclusion
is superseded. BPR-0227 is therefore a mandatory dependency of this closeout.

Re-read every acceptance criterion and run clean relevant developer,
core-only, sanitizer, tests-disabled Release, target-native available-platform,
architecture/dependency, documentation-impact, original replacement-pack,
tracker, workflow-absence, and diff-hygiene gates. Replay the complete direct
claim matrix after final documentation and simplification.

Rerun both selected public-composition tests on the final post-BPR-0082 source
with `QT_QPA_PLATFORM=offscreen` and `CTEST_PARALLEL_LEVEL=1`; allow only one
application process at a time. The earlier BPR-0079 public raster plus
BPR-0080 component renders cannot substitute for this final C3 replay.

Capture rendered two-direction interaction and machine-readable traces proving
shared authority, left/right/divider input, insertion/deletion pinning,
unequal replacement, reversal, start/end, one scrollbar, link-map attachment,
identity agreement, stale rejection, and visible-set performance through the
real composition or closest deterministic rendered harness.

Capture an owner-activated, target-native System-theme transition on the full
selected composition. Record the native style/plugin, inherited palette and
font before/after the OS appearance change, stable semantic selection, one
post-theme geometry rebuild, and visibly differing full-window pixels. An
offscreen or forced-palette render cannot satisfy this gate. This RFC-4 gate
does not substitute for RFC-BPR-0005's planned integrated presentation-polish
and native visual-acceptance pass.

Mark RFC-BPR-0004 Auto-Verified only when every required live card is done and
RFC-BPR-0003 is genuinely Auto-Verified.

By product-owner direction the implementation frontier now advances past
RFC-BPR-0004, so RFC-BPR-0005 onward may be implemented on available targets
while this card remains open. That direction changes nothing about this card:
its own closure still requires RFC-BPR-0003 to close first, which still
requires real Windows x64 evidence.

## Closeout state

BPR-0082 is committed at `f4784f6`. Deterministic available-platform replay is
implemented on the selected public composition. The frozen installed-Git
fixture now contains matching, 4:7 replacement, deletion, insertion, and
asymmetric-tail regions. One serial offscreen application process produces 18
full-window PNGs plus a valid 18-record JSONL trace covering both-direction
motion, all three vertical input receivers, pinning, proportional replacement,
reversal, ribbon selection, resize, and endpoints. Direct inspection confirms
attached replacement/insertion/deletion geometry, stable selection/detail
agreement, and coherent resized/start/end presentation.

That replay exposed and fixed a zero-origin composition bug between the native
text panes and link map. The workbench now reports the actual quantized origin
derived from the scrollbar value it applies; a focused regression covers the
late scrolled replacement. The test-only native appearance probe also records
the selected workbench's before/after palette and font, stable selection, and
an exact one-rebuild geometry delta. Release-seam forbidden-byte coverage
includes the new evidence option and marker.

The target-native Windows lane remains open and nonblocking. The owner-visible
macOS System-theme transition will use exactly one selected application
process and will not start without explicit desktop-owner authorization to
change and restore the global appearance. BPR-0054/RFC-BPR-0003 closed
Auto-Verified on 2026-08-28, satisfying the canonical closeout dependency;
[[BPR-0474]]'s Windows high-DPI course completed later the same day, and
this card closed with it.

## Available-platform verification

- The final rebuilt `application_diff_workbench_public_path` passes 1/1 in
  12.90 seconds with `QT_QPA_PLATFORM=offscreen` and
  `CTEST_PARALLEL_LEVEL=1`. Exactly one application process ran and exited;
  the post-run process scan was empty. The JSONL file parses as 19 records and
  the evidence directory contains 19 full-window PNGs. Step 16 records one
  actual selection callback, two visible ribbons, stable group/segment
  identities, geometry generation 2, and semantic-index generation 1. Step 17
  records geometry generation 3, semantic-index generation 1, and nonzero
  applied origins on both panes.
- The selected Qt component matrix passes 12/12 offscreen and serial. The
  focused final workbench executable passes after the applied-origin and exact
  theme-rebuild regressions. The refreshed ASan/UBSan core/model/semantics/
  host/link-map/workbench matrix passes 7/7.
- The read-only RFC utility lane launched no application or Qt process and
  changed no file. Its core matrix passes 5/5 and its selected fixture,
  release-composition, binary/runtime, dependency/platform, architecture, and
  worker matrix passes 11/11. It also independently confirms the trace records
  and PNGs, immutable RFC content, clean diff hygiene, and tracked/
  on-disk workflow counts of 0/0.
- The complete 181-file format gate passes. The first repository-wide strict
  tidy pass found one unnecessary widening conversion in the new test-only
  encoder; removing the unused reserve simplified the code, and the final
  five changed translation units pass with warnings as errors. Documentation
  validation reports `DOCUMENTATION_OK` after the blocked closeout card
  returned to backlog. The downloaded replacement pack's own validator reports
  22 valid files and the canonical RFC-BPR-0004 replacement path.
- Two independent `rfc-reviewer` attempts were bounded, interrupted, and
  replaced after neither returned a finding report. They made no writes or
  launches. The root closeout review found and fixed two evidence hazards that
  were not allowed to stand: the native geometry baseline had preceded the
  probe's own resize drain, and the already-selected ribbon click needed an
  exact callback-count assertion.
- An independent `rfc-critical-reviewer` pass has now completed and returned a
  finding report, so that requirement is satisfied. Its verdict is non-blocking
  findings only. It falsified no RFC-BPR-0004 semantic claim and found no stub,
  mockup, or helper-API-only proof presented as public-path evidence. It
  confirmed the composed replay routes through the production request path, the
  single retained scroll authority with pane scrollbars off, pinning and exact
  reversal and the 4:7 proportional advance, semantic generation and index
  counters held at one across every record and a 1,000-event wheel
  storm, and honest statement of the open gates. It reviewed the then-current
  eighteen-record trace; done BPR-0087 has since regenerated it as nineteen
  records with strengthened assertions. It re-inspected the rendered
  frames independently and confirmed the insertion wedge and attached selected
  replacement ribbon.
- Its one medium finding is closed. The recorded 00:48 evidence predated three
  later RFC-BPR-0003 commits touching the epoch host composed into the same
  process. The complete matrix was replayed at `ca424f7`: `dev` passes 88/88 in
  685.29 seconds and `core-only` passes 33/33 in 83.27 seconds, including
  `application_diff_workbench_public_path` at 12.36 seconds and the
  tests-disabled Release composition contract. Those counts supersede the
  earlier ones.
- Its remaining findings are evidence-strength improvements, not defects.
  BPR-0087 tracks the two actionable ones: the composed wheel steps never
  assert that the wheel moved anything, and the composed ribbon click targets
  the already auto-selected group so the replay never shows a click replacing a
  selection. Both behaviors are separately proven at component level. The
  inspection-dependent PNG gate and the derived applied origin are recorded as
  accepted on that card, the latter belonging to RFC-BPR-0005 polish.
- The owner-authorized native System-theme gate is now satisfied. On 2026-08-03
  at commit `1e581a6`, on macOS 15.7.7 build 24G720 arm64 with Qt 6.11.1, the
  desktop owner authorized changing and restoring the global appearance and
  supplied the activating click. The probe reported `PUBLIC_PATH_OK`,
  `GITHUB_FIXTURE_QT_PLATFORM:cocoa`, `GITHUB_FIXTURE_QT_STYLE:QMacStyle`,
  `NATIVE_APPEARANCE_ACTIVE_FOCUS_OK`, both local and global `Light->Dark`
  transitions, and every signal, state, pixel-difference, changed-render, and
  workbench `NATIVE_APPEARANCE_DIFF_*` marker. The workbench palette moved from
  `#ffececec:#ffffffff:#ff000000:#ff000000` to
  `#ff323232:#ff171717:#ffffffff:#ffffffff`, the font was identical before and
  after, and the geometry rebuild delta was exactly 1. Both 2560x2000 rasters
  were inspected directly and show the same pull request, epoch rows, complete
  OIDs, `Whole PR` selection, highlighted `old-a`, attached 4:7 ribbon, and
  `Change 1 of 3` detail, with readable native contrast in both appearances.
  The original System Dark `true` was restored. The rasters are transient and
  not checked in.
- Direct inspection also found that the two frames are not a palette-only
  delta: the outer window scroll offset differs between them while the inner
  diff panes are identical. The desktop owner confirms this was their own
  interaction during the attended run, not a post-theme relayout effect. The
  probe requires an owner-activated focused window, so the owner is free to
  interact between the Light capture and the transition; the probe therefore
  asserts semantic and geometry invariants rather than an outer scroll position
  an attended run cannot hold fixed. Every gate requirement is met.
- The final available-platform replay ran on 2026-08-28 at head `7160c283`
  through three read-only `rfc-utility` lanes plus direct integrator
  inspection, and every automated checklist row passed fresh: C1 5/5
  (core-only), the dev boundary/core group 12/12 plus
  `documentation_issue_graph_contract` 1/1, C2 12/12 and C3 2/2 serial
  offscreen with an empty post-run process scan, the tests-disabled Release
  composition contract in 120.06 s, the ASan/UBSan selected matrix 18/18
  with the sanitizer proven active, `coding-format-check` over 425 files,
  `DOCUMENTATION_OK`, `BINARY_DEPENDENCIES_OK`, `RUNTIME_SURFACE_OK`, clean
  diff hygiene, 0/0 workflow counts, the 22-file pack validator and 22/22
  pack checksums, and the strict-tidy delta re-check. The same-day clean
  `dev` 218/218 and `core-only` 98/98 matrices are consumed with a
  commit-exact delta analysis: the only code change since their head
  `27e50ded` is BPR-0473's one test line owning no RFC-BPR-0004 selector.
  The regenerated 19-record trace and 19 PNGs were inspected directly and
  the keyed per-criterion records are in the ledger's final
  available-platform closeout replay section.
- The target-native Windows x64 high-DPI course ran on the designated host
  on 2026-08-28 and PASSED at real 1.25x and 2.0x per-monitor scaling
  through the real application at `7160c283`: fusion full course green at
  both scales with 19 frames plus 19-record traces each, default-style
  `RENDER_OK` with the pane body painting the applied profile base
  `#0d1117` and readable `#e6edf3` text, real-DPR component assertions
  matching the macOS BPR-0080 dimensions, rasters directly inspected,
  display restored, and both host checkouts clean. The one divergence —
  the default QWindows11Style failing the course's exact viewport
  palette-identity check, reproducible at 1.0x offscreen and so neither a
  high-DPI nor a geometry failure — is deliberately carded as [[BPR-0475]]
  under RFC-BPR-0005's presentation-polish scope, independent follow-up
  that does not belong to this RFC's queue. The complete record is on
  [[BPR-0474]].
- Closed 2026-08-28: [[BPR-0474]]'s designated-host course completed later
  the same day, so the closing slice moved both cards to `60-done/`, ticked
  this checklist, and flipped RFC-BPR-0004 to Auto-Verified. Every commit
  between the final available-platform replay at `7160c283` and this close
  is documentation/tracker-only, so no row's evidence was invalidated.
  [[BPR-0475]] remains deliberately independent RFC-BPR-0005 follow-up.

## Closeout checklist

Historical evidence is provenance only; every row requires final replay at the final closeout head. The 2026-08-28 final available-platform replay at `7160c283` satisfies every automated row — its keyed records are in the ledger's final available-platform closeout replay section — and every commit after that head in this closeout is documentation/tracker-only. Rows were ticked at the 2026-08-28 close.

| State | Criterion | Direct verification surface | Command or attended interaction | Expected result | Durable evidence location | Gate class | Live blocker |
|---|---|---|---|---|---|---|---|
| [x] | Identical inputs and options produce identical canonical segments, groups, mappings, and stable IDs. | Canonical segment/group/mapping identity in core_diff_model and core_diff_compute | `ctest --preset dev -R '^(core_diff_model\|core_diff_compute\|core_diff_semantics)$' --output-on-failure` | Repeating identical blobs and options yields byte-identical segments, groups, mappings, and stable IDs. | BPR-0073 deterministic-computation evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Identical inputs and options produce identical canonical segments, groups, mappings, and stable IDs.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Segment coverage and monotonicity validators pass for representative and generated cases. | Generated segment coverage and monotonicity validators in core_diff_compute | `ctest --preset dev -R '^(core_diff_compute)$' --output-on-failure` | Representative and generated inputs cover each source exactly once and every primary mapping remains monotonic. | BPR-0073 deterministic-computation evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Segment coverage and monotonicity validators pass for representative and generated cases.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Equal, replacement, left-only, and right-only regions are explicit. | Explicit equal, replacement, left-only, and right-only canonical segment variants | `ctest --preset dev -R '^(core_diff_model\|core_diff_compute)$' --output-on-failure` | Fixtures produce the required variant for each region; no changed region is collapsed into an ambiguous kind. | BPR-0074 canonical-semantics evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Equal, replacement, left-only, and right-only regions are explicit.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Empty spans represent insertions/deletions without fake source lines. | Empty-span insertion/deletion representation in the canonical diff model | `ctest --preset dev -R '^(core_diff_model\|core_diff_semantics)$' --output-on-failure` | Insertions and deletions carry empty source spans on the absent side and no synthetic source line appears. | BPR-0074 canonical-semantics evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Empty spans represent insertions/deletions without fake source lines.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Secondary pairing is deterministic and monotonic. | Deterministic monotonic secondary line pairing in core_diff_semantics | `ctest --preset dev -R '^(core_diff_semantics)$' --output-on-failure` | Repeated replacement pairing returns the same accepted pairs in monotonic order. | BPR-0074 canonical-semantics evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Secondary pairing is deterministic and monotonic.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Weak/unpaired lines remain explicit rather than being falsely paired. | Weak and unpaired replacement-line classification in core_diff_semantics | `ctest --preset dev -R '^(core_diff_semantics)$' --output-on-failure` | Below-threshold candidates remain explicitly weak or unpaired and never acquire a false partner. | BPR-0074 canonical-semantics evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Weak/unpaired lines remain explicit rather than being falsely paired.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Intra-line ranges attach only to accepted pairs and remain in valid text bounds. | Accepted-pair intraline range bounds in core_diff_semantics | `ctest --preset dev -R '^(core_diff_semantics)$' --output-on-failure` | Only accepted pairs receive intraline ranges, and every range endpoint is within its line text. | BPR-0074 canonical-semantics evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Intra-line ranges attach only to accepted pairs and remain in valid text bounds.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Move relations preserve a monotonic primary path. | Moved-block relation overlay on the monotonic primary mapping | `ctest --preset dev -R '^(core_diff_semantics)$' --output-on-failure` | Move relations identify retained text without reordering or crossing the primary path. | BPR-0074 canonical-semantics evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Move relations preserve a monotonic primary path.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Previous/next change uses the same deterministic groups displayed in the link map. | Shared change-group identity used by navigation and DiffLinkMap | `ctest --preset dev -R '^(core_diff_transition\|qt_diff_link_map_scale_(100\|125\|200))$' --output-on-failure` | Previous/next change selects the same stable group IDs painted by the link map. | BPR-0079 selected-composition evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Previous/next change uses the same deterministic groups displayed in the link map.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | The detail pane reports exact, similar, span, boundary, and unavailable mapping honestly. | Exact/similar/span/boundary/unavailable detail projection | `ctest --preset dev -R '^(core_diff_transition\|qt_diff_workbench_scale_(100\|125\|200))$' --output-on-failure` | Each mapping confidence/kind produces its distinct detail state, including explicit boundary and unavailable results. | BPR-0079 selected-composition evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “The detail pane reports exact, similar, span, boundary, and unavailable mapping honestly.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Diff work uses exact blob/epoch/file/options/algorithm identity. | Exact diff request identity in the application diff host | `ctest --preset dev -R '^(core_diff_transition\|application_diff_host)$' --output-on-failure` | Blob OIDs, epochs, file, options, algorithm, request, and generation all participate in completion acceptance. | BPR-0075 bounded-host evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Diff work uses exact blob/epoch/file/options/algorithm identity.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | A completion for an old repository, PR, epoch, file, options set, or generation is rejected. | Stale diff-completion rejection across every authority dimension | `ctest --preset dev -R '^(core_diff_transition\|application_diff_host)$' --output-on-failure` | Changing repository, PR, epoch, file, options, or generation before completion leaves the active projection unchanged. | BPR-0075 bounded-host evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “A completion for an old repository, PR, epoch, file, options set, or generation is rejected.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Cancellation and shutdown cannot update destroyed Qt state. | Diff-host cancellation/shutdown lifetime and destroyed-Qt boundary | `ctest --preset dev -R '^(application_diff_host\|application_worker_executor\|architecture_boundaries)$' --output-on-failure` | Cancelled or post-shutdown work publishes no completion and no callback can target destroyed Qt state. | BPR-0075 bounded-host evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Cancellation and shutdown cannot update destroyed Qt state.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Cache selection is directly proven rather than inferred from target names. | Direct cache hit/miss instrumentation in application_diff_host | `ctest --preset dev -R '^(application_diff_host)$' --output-on-failure` | The suite observes exact cache selection counters/keys; renaming targets cannot satisfy the assertion. | BPR-0075 bounded-host evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Cache selection is directly proven rather than inferred from target names.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Workers do not include or touch Qt. | Worker target dependency scan and executor contract | `ctest --preset dev -R '^(architecture_boundaries\|application_worker_executor)$' --output-on-failure` | Worker sources link no Qt target and make no Qt call while performing/cancelling diff work. | BPR-0075 bounded-host evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Workers do not include or touch Qt.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | One retained comparison-space position drives both panes. | Single retained comparison-space position in core and Qt scroll controller | `ctest --preset dev -R '^(core_workbench_interaction\|qt_comparison_scroll_controller_scale_(100\|125\|200))$' --output-on-failure` | Both pane origins are derived from one retained comparison position after every input. | BPR-0077 shared-controller evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “One retained comparison-space position drives both panes.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Wheel/trackpad input over either pane advances the same position with no handoff jump. | Wheel/trackpad translation over left, divider, and right surfaces | `ctest --preset dev -R '^(core_workbench_interaction\|qt_comparison_scroll_controller_scale_(100\|125\|200))$' --output-on-failure` | The same wheel path advances the same comparison position with no first-event handoff jump. | BPR-0077 shared-controller evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Wheel/trackpad input over either pane advances the same position with no handoff jump.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | A pure right insertion pins the left pane at its boundary while the right content advances. | Right-insertion pinning in the shared scroll controller | `ctest --preset dev -R '^(core_workbench_interaction\|qt_comparison_scroll_controller_scale_(100\|125\|200))$' --output-on-failure` | Across a pure right insertion the left origin stays at its boundary while right content advances. | BPR-0077 shared-controller evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “A pure right insertion pins the left pane at its boundary while the right content advances.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | A pure left deletion pins the right pane while the left content advances. | Left-deletion pinning in the shared scroll controller | `ctest --preset dev -R '^(core_workbench_interaction\|qt_comparison_scroll_controller_scale_(100\|125\|200))$' --output-on-failure` | Across a pure left deletion the right origin stays at its boundary while left content advances. | BPR-0077 shared-controller evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “A pure left deletion pins the right pane while the left content advances.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Unequal replacements advance at proportional measured rates and meet at the same segment boundary. | Measured proportional replacement traversal | `ctest --preset dev -R '^(core_workbench_interaction\|qt_comparison_scroll_controller_scale_(100\|125\|200))$' --output-on-failure` | Unequal replacement panes advance at measured proportional rates and meet at the same segment endpoint. | BPR-0077 shared-controller evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Unequal replacements advance at proportional measured rates and meet at the same segment boundary.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Reversing the same input path retraces the same pane origins within defined numeric tolerance and without cumulative drift. | Forward/reverse retained-position replay | `ctest --preset dev -R '^(core_workbench_interaction\|qt_comparison_scroll_controller_scale_(100\|125\|200))$' --output-on-failure` | Reversing the identical input path restores both pane origins within tolerance with no accumulated drift. | BPR-0077 shared-controller evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Reversing the same input path retraces the same pane origins within defined numeric tolerance and without cumulative drift.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Beginning and end content on both sides are reachable with no dead scrollbar region. | Shared-scrollbar endpoint reachability | `ctest --preset dev -R '^(core_workbench_interaction\|qt_comparison_scroll_controller_scale_(100\|125\|200))$' --output-on-failure` | Beginning and end content in both panes are reachable and the scrollbar has no dead range. | BPR-0077 shared-controller evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Beginning and end content on both sides are reachable with no dead scrollbar region.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Page, Home, End, change navigation, and shared-scrollbar drag use the same controller. | Page/Home/End/change/drag command routing through one controller | `ctest --preset dev -R '^(core_workbench_interaction\|qt_comparison_scroll_controller_scale_(100\|125\|200))$' --output-on-failure` | Every named command updates the retained comparison position and then derives pane origins through the same path. | BPR-0077 shared-controller evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Page, Home, End, change navigation, and shared-scrollbar drag use the same controller.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Horizontal scroll remains independently usable in each pane. | Independent horizontal scrollbars in the composed diff workbench | `ctest --preset dev -R '^(qt_diff_workbench_scale_(100\|125\|200))$' --output-on-failure` | Horizontal motion in one pane leaves the other pane's horizontal origin unchanged. | BPR-0079 selected-composition evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Horizontal scroll remains independently usable in each pane.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Changed intervals render as filled cubic ribbons attached to actual pane line boundaries. | Measured cubic ribbon endpoints in the three-scale DiffLinkMap | `ctest --preset dev -R '^(core_diff_semantics\|qt_diff_link_map_scale_(100\|125\|200))$' --output-on-failure` | Every changed interval paints a filled cubic ribbon whose endpoints equal measured pane line boundaries. | BPR-0078 DiffLinkMap evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Changed intervals render as filled cubic ribbons attached to actual pane line boundaries.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Insertions and deletions render as wedges through empty spans. | Empty-span insertion/deletion wedge rendering | `ctest --preset dev -R '^(core_diff_semantics\|qt_diff_link_map_scale_(100\|125\|200))$' --output-on-failure` | Insertion and deletion groups paint wedges at their empty-side boundary rather than a fabricated row. | BPR-0078 DiffLinkMap evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Insertions and deletions render as wedges through empty spans.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | The top documents contain no synthetic alignment blanks. | Top-pane source-document contents in diff_workbench | `ctest --preset dev -R '^(qt_diff_workbench_scale_(100\|125\|200))$' --output-on-failure` | The displayed line sequence equals each source blob exactly and contains no alignment blanks. | BPR-0079 selected-composition evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “The top documents contain no synthetic alignment blanks.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Ribbons remain attached while scrolling, resizing, changing DPI, and changing font metrics. | Ribbon attachment under scroll, resize, DPI, and font metrics | `ctest --preset dev -R '^(qt_diff_link_map_scale_(100\|125\|200)\|qt_diff_workbench_scale_(100\|125\|200))$' --output-on-failure` | After each geometry change, ribbon endpoints still equal the corresponding measured text-block intervals. | BPR-0078 DiffLinkMap evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Ribbons remain attached while scrolling, resizing, changing DPI, and changing font metrics.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Hover and click select stable semantic identities. | Stable semantic hover/click identity in DiffLinkMap | `ctest --preset dev -R '^(qt_diff_link_map_scale_(100\|125\|200))$' --output-on-failure` | Hover and click report the same stable group ID before and after geometry changes. | BPR-0078 DiffLinkMap evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Hover and click select stable semantic identities.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Move relationships do not create a permanent crossing tangle. | Primary-path crossing bound with move overlays | `ctest --preset dev -R '^(core_diff_semantics\|qt_diff_link_map_scale_(100\|125\|200))$' --output-on-failure` | Ordinary correspondence stays monotonic and move overlays do not create a permanent crossing tangle. | BPR-0078 DiffLinkMap evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Move relationships do not create a permanent crossing tangle.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Painting is bounded to visible/near-visible runs and does not instantiate one widget per ribbon. | Visible/near-visible ribbon run instrumentation | `ctest --preset dev -R '^(qt_diff_link_map_scale_(100\|125\|200))$' --output-on-failure` | Painted runs remain bounded by the viewport neighborhood as total group count grows; no per-ribbon widget is created. | BPR-0080 scale and bounded-interaction evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Painting is bounded to visible/near-visible runs and does not instantiate one widget per ribbon.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Splitter resizing and ribbon selection are both usable. | Splitter hit testing and ribbon selection in diff_workbench | `ctest --preset dev -R '^(qt_diff_workbench_scale_(100\|125\|200))$' --output-on-failure` | Dragging the splitter changes pane geometry and clicking a ribbon still selects its exact semantic group. | BPR-0079 selected-composition evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Splitter resizing and ribbon selection are both usable.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | The real BetterPullReview composition or closest deterministic rendered integration harness demonstrates the behavior in both scroll directions. | Selected-file public executable diff trace in both scroll directions | `ctest --preset dev -R '^(application_diff_workbench_public_path)$' --output-on-failure`; use BPR-0227's accepted-review -> file-selection gesture -> exact diff request -> adopted window-comparison trace. | The public fixture selects a real comparison, scrolls forward and backward, and reports window projections/ribbons for both directions. | BPR-0227 selected-diff correction contract and BPR-0225 final-state correction in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “The real BetterPullReview composition or closest deterministic rendered integration harness demonstrates the behavior in both scroll directions.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | The lower detail pane, change navigation, selected line, and link map agree on identity. | Selected group agreement across detail pane, navigation, line selection, and link map | `ctest --preset dev -R '^(core_diff_transition\|qt_diff_workbench_scale_(100\|125\|200)\|application_diff_workbench_public_path)$' --output-on-failure`; use BPR-0227's accepted-review -> file-selection gesture -> exact diff request -> adopted window-comparison trace. | All four surfaces expose the same stable group and mapped source-line identities after selection/navigation. | BPR-0079 selected-composition evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “The lower detail pane, change navigation, selected line, and link map agree on identity.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | Large-file evidence proves no full-file semantic recomputation per wheel event. | Large-file semantic recomputation and widget-count instrumentation | `ctest --preset dev -R '^(core_diff_compute\|application_diff_host\|qt_diff_link_map_scale_(100\|125\|200))$' --output-on-failure` | Wheel events change only viewport projection; semantic-computation counters stay fixed and painted/widget work stays viewport-bounded. | BPR-0080 scale and bounded-interaction evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Large-file evidence proves no full-file semantic recomputation per wheel event.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | High-DPI and supported platform rendering checks pass. | Real Windows x64 high-DPI diff workbench and retained BPR-0053 platform procedure | Run [BPR-0474](../60-done/BPR-0474-rfc-0004-windows-high-dpi-verification.md#run-rfc-bpr-0004-windows-x64-target-native-high-dpi-verification)'s target-native high-DPI application/rendering procedure for the retained diff surface; its card records the closed BPR-0053 host procedure it inherits. | Supported Windows scaling renders attached readable ribbons and panes with no platform-specific geometry or dependency failure. | Target-native portion of BPR-0083 available-platform closeout evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “High-DPI and supported platform rendering checks pass.” | target-native | [BPR-0474](../60-done/BPR-0474-rfc-0004-windows-high-dpi-verification.md) closed with this card; its 2026-08-28 designated-host record and the ledger's target-native Windows evidence section carry the real 1.25x/2.0x proof. |
| [x] | Product, architecture, UX, agent, and verification documentation is current. | Product/architecture/UX/agent/verification Markdown plus documentation_structure | `python3 scripts/verify_documentation.py --root .` | The documentation validator resolves every link/status/schema and the reviewed documents describe the final public diff path without stale claims. | BPR-0081 documentation and conformance evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “Product, architecture, UX, agent, and verification documentation is current.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | A conservative simplification pass is complete. | RFC-BPR-0004 changed-path simplification, format, tidy, and diff gates | `cmake --build --preset dev --target coding-format-check coding-tidy && git diff --check` | The conservative pass leaves no removable duplication or stale adapter path; format/tidy and whitespace checks report no issue. | BPR-0082 simplification evidence in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “A conservative simplification pass is complete.” | automated | No live blocker; the final replay is recorded in the ledger's final available-platform closeout replay section. |
| [x] | All live RFC-BPR-0004 issue cards are in the terminal done lane before Auto-Verified closeout. | Canonical issue graph and predecessor RFC-BPR-0003 closeout card | Verify BPR-0054 is in `60-done/` with status `Done`, then run `python3 scripts/verify_documentation.py --root .`. | Every RFC-BPR-0004 issue card is in `60-done/` and the dependency graph permits the Auto-Verified transition only after RFC-BPR-0003. | BPR-0083 available-platform closeout evidence and canonical RFC ordering in [RFC-BPR-0004 verification ledger](../../verification/RFC-BPR-0004.md#direct-claim-matrix); final replay record keyed by “All live RFC-BPR-0004 issue cards are in the terminal done lane before Auto-Verified closeout.” | automated | Completed at this close: every RFC-BPR-0004 card including [BPR-0474](../60-done/BPR-0474-rfc-0004-windows-high-dpi-verification.md) is in `60-done/`, BPR-0054 is `Done`, and RFC-BPR-0003 closed Auto-Verified in canonical order. |
