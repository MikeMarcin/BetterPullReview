---
schema: "implement-rfc.card.v1"
id: "BPR-0101"
title: "Verify and close RFC-BPR-0005"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0083","BPR-0090","BPR-0091","BPR-0092","BPR-0093","BPR-0094","BPR-0095","BPR-0096","BPR-0097","BPR-0098","BPR-0099","BPR-0100","BPR-0189","BPR-0190","BPR-0191","BPR-0192","BPR-0226","BPR-0227","BPR-0234","BPR-0414"]
closeout_for: ["RFC-BPR-0005"]
---

# Verify and close RFC-BPR-0005

RFC base: `55d365f16847d2ffc2c8fab83a5ce62eb695e2a0`.

Replay the complete RFC-BPR-0005 claim matrix from clean state, resolve every documentation-ledger row, perform a skeptical final diff review, and obtain an independent review pass.

Closure is strictly ordered, and the prior-RFC chain is now satisfied: RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0004 all closed `Auto-Verified` by 2026-08-28. The owner accepted BPR-0100's exact v003 executable and its exact final-head v004 successor; BPR-0100 stayed in Backlog by its own terms until the RFC-BPR-0005 terminal identity replay recorded below.

## Final closeout replay — 2026-08-28/29

Every row above was replayed at the closeout head, and the evidence
composition is stated exactly rather than rounded up:

- **Clean state**: fresh `--fresh` dev configure and `--clean-first` build,
  then the full dev matrix. Two full 218-test runs bracket the closeout: the
  first (pre-repair head `936d130e`) and the final one after the
  [[BPR-0479]] hygiene slices. In each, the only failures were the
  [[BPR-0477]] app-source digest pins — test-infrastructure guards drifted
  by legitimate `src/app` edits, repinned to the committed tree's values and
  rerun green each time (final: 101.7 s + 28.1 s). Every product test in
  both full runs passed. Fresh `core-only` passed 98/98.
- **Row-exact selectors**: the checklist commands passed 12/12 as first
  written and, after the independent review corrected two rows' selectors
  and strengthened two more, the consolidated corrected set passed **27/27**
  at the repaired head.
- **Strict gates**: whole-tree `coding-tidy` passed with **zero errors** —
  the first clean whole-tree run under LLVM 23.1.0, after [[BPR-0479]]
  judged and repaired all 96 findings the tightened toolchain surfaced —
  plus the format gate, `git diff --check`, and both documentation gates.
- **Composition**: `nm` on the shipped binary shows `workbench_window`
  (1,373 symbols), `workbench_drawer` (910), `workbench_application_host`
  (1,737), `diff_workbench` (625), `github_conversation_panel` (160), and
  zero `github_window` — the exact check this card's own preamble demanded.
- **Independent review**: `rfc-reviewer` returned GAPS-FOUND with four
  blocking evidence defects; each was reproduced directly and repaired
  before any status change — the ledger's attended-gate overclaim, the
  stale virtualization figures (restated 424/29/6 with
  equality-not-absolutes named), two selector/count mismatches (one naming
  a test that matched nothing), and the ribbon row running none of its
  distinguishing assertions. Its findings also produced BPR-0475's portable
  guard test (proven red-without-fix by a negative build), [[BPR-0478]]'s
  orphan removal, and the recorded frame-time/memory narrowing deferral to
  RFC-BPR-0013.
- **Attended terminal gate**: the recorded
  `conversation-code-boundary-v001` verdict revalidates against its exact
  manifest, and the owner accepted
  [`native-visual-acceptance-v005`](../../verification/reviews/native-visual-acceptance-v005/operator-packet.md)
  on 2026-08-29 against the closeout executable
  `sha256:462896aa2c48a2a97215bc8595a2fca9c5d1a7f635d1ab0759d8733592652347`
  (55,045,344 bytes), byte-validated in
  [`validation-record.json`](../../verification/reviews/native-visual-acceptance-v005/validation-record.json).

RFC-BPR-0005 is `Auto-Verified`. [[BPR-0100]] closes in this same commit,
its terminal procedure consumed by this replay, exactly as the
closeout-procedure rule requires.

## Historical composition finding and current correction

BPR-0188 measured that the application did not contain `workbench_window`,
`workbench_drawer`, or `workbench_application_host`, and that neither
`workbench_settings_port` nor `workbench_observation_port` had an adapter. That
historical finding led to BPR-0189 through BPR-0192. The shell is now on screen,
but BPR-0225 found its callbacks empty, its accepted review unobserved, and its
host projections routed to a non-rendering sink. BPR-0226 and BPR-0227 own the
current public-path gap.

When this card is eventually prepared, replay against the **composed
application**. Replaying the component suites would reproduce exactly the gap
BPR-0188 found: they all pass today, against a window no user can open.

Ask the linker as part of the replay. `nm` on the shipped binary reporting
symbols for every component this RFC specifies is a second's work and is the
check that no amount of green component testing substitutes for.

## Closeout checklist

Historical evidence is provenance only; every row requires final replay at the final closeout head.

| State | Criterion | Direct verification surface | Command or attended interaction | Expected result | Durable evidence location | Gate class | Live blocker |
|---|---|---|---|---|---|---|---|
| [x] | The rendered workbench matches the normative layout rather than the non-normative mockup pixel-for-pixel. | Real macOS shipped workbench populated through BPR-0226/BPR-0227 and compared with the normative RFC layout | Run [BPR-0100](BPR-0100-rfc-0005-native-visual-acceptance.md#run-the-inherited-native-visual-acceptance-pass)'s digest-bound attended inspection after the public fixture shows real drawers, epochs, ribbons, shared scroll, and detail content. | The reviewer accepts the assembled normative hierarchy, native controls, readability, and supported-size behavior; mockup pixel parity is not used. | Presentation-is-natively-acceptable row and BPR-0100 verdict artifact in [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix); final replay record keyed by “The rendered workbench matches the normative layout rather than the non-normative mockup pixel-for-pixel.” | owner-attended | [BPR-0100](BPR-0100-rfc-0005-native-visual-acceptance.md) records the accepted native presentation. The owner waived an interim digest-only replay after BPR-0220's probe-expectation relink. The row remains unchecked until the RFC-BPR-0005 terminal identity replay; its BPR-0083 wait ended when RFC-BPR-0004 closed on 2026-08-28. |
| [x] | File and conversation drawers are independently collapsible and restore persisted widths. | Three-scale qt_workbench_window drawer/splitter persistence harness | First complete BPR-0226's accepted-PR -> workbench-host -> real-window action/projection trace; then run `ctest --preset dev -R '^(qt_workbench_window_scale_(100\|125\|200))$' --output-on-failure`. | Each drawer independently collapses, returns width to the centre, restores its persisted width after restart, and remains keyboard reachable. | Drawers-return-space-to-the-centre row in the claim matrix in [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix); final replay record keyed by “File and conversation drawers are independently collapsible and restore persisted widths.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Primary navigation is always visible at supported window sizes or moves into an explicit compact overflow without losing keyboard access. | Top-level primary QAction and compact-overflow projection at three scales | First complete BPR-0226's accepted-PR -> workbench-host -> real-window action/projection trace; then run `ctest --preset dev -R '^(qt_workbench_window_scale_(100\|125\|200))$' --output-on-failure`. | Previous/next file and change remain visible or enter explicit overflow while their shortcuts and accessible names remain active. | Primary-actions-are-top-level row in the claim matrix in [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix); final replay record keyed by “Primary navigation is always visible at supported window sizes or moves into an explicit compact overflow without losing keyboard access.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Left/right epoch selectors show labels and exact OIDs and can select arbitrary retained epochs. | Workbench epoch selectors combined with retained-history selector projection | First complete BPR-0226's accepted-PR -> workbench-host -> real-window action/projection trace; then run `ctest --preset dev -R '^(qt_workbench_window_scale_(100\|125\|200)\|qt_epoch_comparison_scale_(100\|125\|200))$' --output-on-failure`. | Both selectors show labels plus untruncated OIDs and select arbitrary retained epochs without losing current file/selection. | Navigation-and-state and selector evidence in the claim matrix in [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix); final replay record keyed by “Left/right epoch selectors show labels and exact OIDs and can select arbitrary retained epochs.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Connector ribbons correspond to the RFC-BPR-0004 group model, attach to measured pane intervals, render empty-span wedges, and keep ordinary correspondence monotonic and noncrossing. | Canonical group ribbons in qt_workbench_window (which holds the monotonic/noncrossing and empty-span-wedge assertions), qt_diff_workbench, and the selected-file public trace | `ctest --preset dev -R '^(qt_workbench_window_scale_(100\|125\|200)\|qt_diff_workbench_scale_(100\|125\|200)\|application_diff_workbench_public_path)$' --output-on-failure`; use BPR-0227's accepted-review -> file-selection gesture -> exact diff request -> adopted window-comparison trace. | Measured ribbons/wedges match RFC-BPR-0004 groups, keep ordinary mapping monotonic, and remain attached through public-path scroll/resize. | Link-map-maps-groups row in the claim matrix in [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix); final replay record keyed by “Connector ribbons correspond to the RFC-BPR-0004 group model, attach to measured pane intervals, render empty-span wedges, and keep ordinary correspondence monotonic and noncrossing.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | The default workbench exposes one shared vertical scrollbar; the two top documents contain no synthetic alignment rows. | Shared vertical-scrollbar policy and exact source documents in qt_workbench_window | `ctest --preset dev -R '^(qt_workbench_window_scale_(100\|125\|200))$' --output-on-failure`; use BPR-0227's accepted-review -> file-selection gesture -> exact diff request -> adopted window-comparison trace. | Exactly one central vertical scrollbar is exposed; both panes disable their own vertical bars and contain no invented alignment rows. | One-shared-scrollbar-no-synthetic-rows row in the claim matrix in [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix); final replay record keyed by “The default workbench exposes one shared vertical scrollbar; the two top documents contain no synthetic alignment rows.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | The lower pane spans the central width and shows the selected group line over line. | Full-centre-width lower detail pane driven by canonical selection | First complete BPR-0226's accepted-PR -> workbench-host -> real-window action/projection trace; then run `ctest --preset dev -R '^(qt_workbench_window_scale_(100\|125\|200)\|qt_diff_workbench_scale_(100\|125\|200))$' --output-on-failure`. | Across drawer stages the lower pane width equals the centre and its line-over-line rows match core detail_for_selection. | Lower-pane-is-full-width-detail row in the claim matrix in [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix); final replay record keyed by “The lower pane spans the central width and shows the selected group line over line.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | UI automation proves navigation, selection synchronization, focus order, loading, failure, cancellation, and restart persistence. | Public workbench action/projection trace, three-scale UI automation, conversation panel, and the accepted BPR-0414 attended batch `7b46cacd14d6dca40c2f153d`, whose verdict and validation record are in `docs/verification/reviews/conversation-code-boundary-v001/`. That capture is the offscreen `QFusionStyle` fixture path, not target-native evidence | Run `ctest --preset dev -R '^(application_diff_workbench_public_path\|application_workbench_composition_contract\|application_workbench_host\|sqlite_workbench_settings_storage\|qt_diff_workbench_scale_(100\|125\|200)\|qt_workbench_window_scale_(100\|125\|200)\|qt_github_conversation_panel)$' --output-on-failure` — the composition contract is the shipped-window trace the BPR-0188 trap demands, and the SQLite store plus host suite carry the durable-restart and cancellation halves the Qt harness cannot; validate the returned `conversation-code-boundary-v001` verdict before terminal replay. | Navigation, exact conversation/source synchronization in both directions, focus order, loading, failure, cancellation, and restart each produce their explicit projected state; unplaceable threads state why and receive no guessed marker. | Navigation-and-state and conversation-and-code-selection rows in the [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix), plus digest-bound `docs/verification/reviews/conversation-code-boundary-v001/` artifacts; final replay record keyed by “UI automation proves navigation, selection synchronization, focus order, loading, failure, cancellation, and restart persistence.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | A large-file harness proves widget count remains bounded by visible UI structure rather than source-line count. | Instrumented 60/6000/60000-line qt_workbench_window scale matrix | First complete BPR-0226's accepted-PR -> workbench-host -> real-window action/projection trace; then run `ctest --preset dev -R '^(qt_workbench_window_scale_(100\|125\|200))$' --output-on-failure`. | Window, comparison-surface, and pane widget counts remain constant as source lines grow by three orders of magnitude — the harness asserts cross-size equality, not absolutes; measured 424/29/6 at the 2026-08-28 closeout head, restated from 218/29/5 as the shell grew (thread map, View menu bar, gutter child). | Renderer-is-virtualized row in the claim matrix in [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix); final replay record keyed by “A large-file harness proves widget count remains bounded by visible UI structure rather than source-line count.” | automated | No live blocker; final-head focused replay remains required. |
| [x] | Qt adapter tests prove all primary actions translate to semantic intents, `QAction` state follows command projections, and bounded row/state changes use incremental model signals rather than full resets. | Core workbench intents and Qt QAction/model delta contracts | First complete BPR-0226's accepted-PR -> workbench-host -> real-window action/projection trace; then run `ctest --preset dev -R '^(core_workbench_interaction\|qt_workbench_window_scale_(100\|125\|200))$' --output-on-failure`. | Every primary QAction emits a semantic intent, enabled/checked state follows projections, and bounded drawer/row changes avoid model reset. | Qt-is-a-presentation-adapter row in the claim matrix in [RFC-BPR-0005 verification ledger](../../verification/RFC-BPR-0005.md#claim-matrix); final replay record keyed by “Qt adapter tests prove all primary actions translate to semantic intents, `QAction` state follows command projections, and bounded row/state changes use incremental model signals rather than full resets.” | automated | No live blocker; final-head focused replay remains required. |
