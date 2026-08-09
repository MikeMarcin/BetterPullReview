---
id: BPR-0083
title: Verify and close RFC-BPR-0004
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0054, BPR-0072, BPR-0073, BPR-0074, BPR-0075, BPR-0076, BPR-0077, BPR-0078, BPR-0079, BPR-0080, BPR-0081, BPR-0082, BPR-0227]
---

# Verify and close RFC-BPR-0004

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
change and restore the global appearance. BPR-0054/RFC-BPR-0003 also remains a
hard canonical closeout dependency, so this card stays in backlog and
RFC-BPR-0005 does not begin.

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
- This card still cannot leave backlog. Its remaining blocker is BPR-0054,
  which needs real Windows x64 evidence.
