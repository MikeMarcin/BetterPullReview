# RFC-BPR-0005 Verification Plan and Ledger

RFC base: `55d365f16847d2ffc2c8fab83a5ce62eb695e2a0`

Status: **components complete, composition absent.** Everything this RFC
specifies is built and tested against `qt::workbench_window` — a window the
shipped application does not contain. BPR-0188 measured it and allocated
BPR-0189 through BPR-0191 to close it. Documentation reconciled by BPR-0098.
RFC-BPR-0005 stays `Proposed` and its BPR-0101 closeout card stays in
`backlog/`.

Read the Composition section before the claim matrix. Every `Verified` row is
true of the harness it names and true of nothing a user can open.

## Why this RFC can be implemented now

RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0004 are implemented and verified on
available targets but remain `Proposed`, because closure is strictly ordered
and RFC-BPR-0002 retains real Windows x64 evidence in BPR-0036. Done BPR-0088
records the product owner's direction to advance the implementation frontier
past RFC-BPR-0004 on available targets. Closure order is unchanged: this RFC
cannot become `Auto-Verified` before RFC-BPR-0004 does.

## Architecture allocation

| Region | Selected responsibility | Planned evidence |
|---|---|---|
| Functional core | File/change navigation order, selection identity, epoch selection legality, drawer semantic state, command projections and their disabled reasons | Qt-free transition tables and replay tests; state-size and forbidden-dependency scans |
| Application host | Bounded workbench observations, incremental projection assembly, settings-persistence requests for drawer and layout state, changed-domain publication, cancellation and generation identity | Fake-port and manual-executor tests covering stale, reordered, duplicate, and canceled terminals, plus restart persistence |
| Qt adapter | Main window, primary toolbar, drawers, epoch selectors, composed comparison surface, lower detail pane, virtualized rows, focus, accessibility, incremental model signals | Qt adapter tests, three-scale rendered cases, instrumented large-file harness, and UI automation |
| Composition root | Wiring the workbench into the real application | Selected public-path evidence through the real executable |

The core stays free of Qt, providers, filesystem, process, clock, thread,
timer, network, database, credential, and hidden-global dependencies. No widget
performs an effect.

## Reuse rather than reimplementation

RFC-BPR-0004 already supplies the pieces this RFC composes. They are reused,
not rebuilt:

- `src/adapters/qt/src/diff_workbench.cpp` — natural-layout panes and the
  applied-origin publication.
- `src/adapters/qt/src/comparison_scroll_controller.cpp` — the single retained
  comparison position and the one shared vertical scrollbar. Pane vertical
  scrollbars stay `AlwaysOff`.
- `src/adapters/qt/src/diff_link_map.cpp` — custom-painted ribbons and wedges,
  indexed visible-path selection, and stable path-derived interaction.
- `src/adapters/qt/src/measured_alignment_axis.cpp` — live pane-boundary
  measurement.
- `src/core/src/diff_semantics.cpp` — the one canonical mapping. The lower
  detail pane re-resolves it rather than retaining a second copy.

Introducing a second mapping, presentation-side regrouping, follower-scroll
coupling, or synthetic alignment rows is a defect, not an implementation
choice.

## Composition

The application shows `github_window`. It does not contain
`workbench_window`, and never has.

| Measurement | Result |
|---|---|
| `nm build/dev/src/app/better_pull_review \| c++filt \| grep -cF workbench_application_host` | `0` |
| the same for `workbench_window` | `0` |
| the same for `workbench_drawer` | `0` |
| implementations of `workbench_settings_port` in `src/` | none |
| implementations of `workbench_observation_port` in `src/` | none |

So the shell this RFC is named for — primary navigation, compact overflow,
drawers, the full-width detail pane, the declared focus order, the accessible
names BPR-0097 repaired — has never been on screen. What the application does
show is `github_window`, RFC-BPR-0002's window, which grew the RFC-BPR-0003 and
RFC-BPR-0004 surfaces directly.

Both windows compose `diff_workbench` and `epoch_comparison_control`. Two
windows own one review surface, and the product shows the one that predates
this RFC. BPR-0188 decided the shell becomes the application's window and
recorded why the cheaper alternative was refused.

Neither of this RFC's two ports has ever had an adapter. That is not an
oversight that was tracked and deferred — a port satisfied by a fake never asks
for one, so nothing ever reported it missing.

The architecture allocation above is worth re-reading with this in mind. It
names a fourth region, **Composition root**, whose selected responsibility is
"wiring the workbench into the real application" and whose planned evidence is
"selected public-path evidence through the real executable." That region was
allocated at planning time and never delivered, and no row of the claim matrix
covers it — the matrix has rows for the core, the host, and the adapter, and
none for the composition root. The plan named the gap before the work started;
the verification table simply had no line for it.

## Claim matrix

Every row's `Direct surface` now names the harness it was proven against.

| Claim | Direct surface | Command or interaction | What proves it | Result |
|---|---|---|---|---|
| Primary actions are top-level | `qt_workbench_window` harness | Invoke previous/next file and change by button and shortcut at supported widths | Actions are reachable and keyboard addressable at every supported width, or move into an explicit compact overflow without losing keyboard access | Verified. `qt_workbench_window` at three scales; compact overflow keeps window association so shortcuts survive both modes |
| Drawers return space to the centre | `qt_workbench_window` harness | Collapse and reopen each drawer, measure the central viewport, restart | Central viewport width is restored and persisted widths survive restart | Verified. Central viewport measured across five drawer stages; collapse hides rather than zero-sizes so the splitter returns the width |
| Link map maps groups | Rendered fixture | Compare ribbon and wedge bounds and selected identity to the RFC-BPR-0004 model through scroll and resize | Every visible ribbon agrees with the canonical group model and stays monotonic and noncrossing | Verified. `test_composed_ribbons_follow_the_group_model_through_scroll_and_resize` checks visible ribbon identity and bounds against the canonical model through scroll and resize |
| One shared scrollbar, no synthetic rows | `qt_workbench_window` harness | Inspect scrollbar policy on both panes and the document contents | Exactly one shared vertical scrollbar exists and neither document contains invented rows | Verified. Exactly one shared vertical scrollbar, both panes always-off, composed unchanged |
| Lower pane is full-width detail | `qt_workbench_window` harness | Select a group and inspect paired rows and central width | The pane spans the central width and its rows agree with the canonical mapping | Verified. Pane width equals the centre across five drawer stages; rows checked against `core::detail_for_selection` at canonically mapped lines |
| Renderer is virtualized | Instrumented `qt_workbench_window` harness | Load a very large file and inspect widget count at 1.0x, 1.25x, and 2.0x | Widget count stays bounded by visible UI structure as file length grows by orders of magnitude, not merely that rendering completed | Verified of the harness. 60/6,000/60,000 lines produce 68/68/68 window widgets, 29/29/29 surface, 5/5/5 pane — measured on a window the application does not contain. BPR-0191 re-measures on the composed window and restates the figure if it differs |
| Qt is a presentation adapter | Qt adapter harness | Drive every primary action and bounded state change | Actions produce semantic intents, `QAction` state follows command projections, and bounded changes use incremental signals rather than full resets | Verified. Toggles and navigation emit core intents only; enablement and disabled reasons come from projections; drawer contents use insert/remove/data-change rather than reset |
| Navigation and state are correct | UI automation | Exercise navigation, selection synchronisation, focus order, loading, failure, cancellation, and restart | Selection follows the filtered actionable order and every state is explicit in text as well as enabled state | Verified. Exact untruncated OIDs, four distinguishable textual states, declared focus order, unique accessible names and key sequences |
| Presentation is natively acceptable | Real macOS composition | Owner-attended inspection of the assembled workbench | Readable contrast and correct native control rendering at supported sizes and scales | Deferred, BPR-0100, owner-attended — and blocked before it is deferred. BPR-0186 records that no launch can be inspected by a person at all: a fixture launch schedules an automated run and exits, and a launch with no fixture takes the real credentialed path. BPR-0187 records the real public pull request to judge it against |

## Deferred gates

RFC-BPR-0005's own text names no owner-attended requirement. One is inherited:
the RFC-BPR-0003 and RFC-BPR-0004 ledgers both defer the integrated-workbench
presentation-polish and native visual-acceptance pass to this RFC. BPR-0100
owns it and is the only owner-attended card here.

Attendance turned out not to be the binding constraint. BPR-0186 and BPR-0187
were allocated by BPR-0180 after an attended sitting on 2026-08-06 could not
proceed: there is no way to launch this application such that a person can look
at it. `--github-fixture github-com` composes the fixture-backed application
and immediately schedules an automated run that drives itself to shutdown, and
a launch without a fixture takes the real credentialed path. Both cards are
unattended work and neither needs Windows, a credential, or a network service.

No RFC-BPR-0005 requirement needs target-native Windows execution, a GitHub
mutation, a credential, or a network service. Non-goals exclude full GitHub
mutations, so no safe test target is required.

## Documentation ledger

| Scope | Required documents | Disposition |
|---|---|---|
| Core navigation and command policy | Domain and state model | Updated. System architecture records the compact core state, the bounded cursor, and why a nonresident selection has no ordinal to step from |
| Host projection and persistence | System architecture | Updated. Host ownership of the ordered set, changed-domain-only publication, per-domain staleness, and limit-rather-than-truncate are recorded |
| Workbench layout, drawers, focus, accessibility, error states | Workbench specification and appearance/accessibility documents | Updated. Command map with shortcuts, the ambiguous-shortcut rationale, compact overflow, drawer ownership split, declared focus order, and the accessible-name uniqueness rule |
| Composed comparison surface | Diff pipeline and rendering, link-map and alignment-space documents | No-impact. The comparison surface is composed unchanged; RFC-BPR-0004's own documents remain correct and no second mapping or authority was introduced |
| Tests and fixtures | Fixture catalog and build/test guide | Updated. The three new suites, the manual-executor terminal algebra, and the virtualization figures with their assert-counts-not-timing rule |

## Closeout blockers

RFC-BPR-0005 cannot become `Auto-Verified` until RFC-BPR-0004 is
`Auto-Verified`, which requires RFC-BPR-0003, which requires RFC-BPR-0002 and
therefore real Windows x64 evidence in BPR-0036 and BPR-0053. BPR-0101 stays in
`backlog/` and makes no closure claim.
