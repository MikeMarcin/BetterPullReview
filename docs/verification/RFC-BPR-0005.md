# RFC-BPR-0005 Verification Plan and Ledger

RFC base: `55d365f16847d2ffc2c8fab83a5ce62eb695e2a0`

Status: planning complete, implementation not started. RFC-BPR-0005 stays
`Proposed` and ends its available-target work at the BPR-0101 closeout card in
`backlog/`.

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

## Claim matrix

| Claim | Direct surface | Command or interaction | What proves it | Result |
|---|---|---|---|---|
| Primary actions are top-level | Real application | Invoke previous/next file and change by button and shortcut at supported widths | Actions are reachable and keyboard addressable at every supported width, or move into an explicit compact overflow without losing keyboard access | Planned, BPR-0092 |
| Drawers return space to the centre | Real application | Collapse and reopen each drawer, measure the central viewport, restart | Central viewport width is restored and persisted widths survive restart | Planned, BPR-0093 |
| Link map maps groups | Rendered fixture | Compare ribbon and wedge bounds and selected identity to the RFC-BPR-0004 model through scroll and resize | Every visible ribbon agrees with the canonical group model and stays monotonic and noncrossing | Planned, BPR-0094 |
| One shared scrollbar, no synthetic rows | Rendered composition | Inspect scrollbar policy on both panes and the document contents | Exactly one shared vertical scrollbar exists and neither document contains invented rows | Planned, BPR-0094 |
| Lower pane is full-width detail | Real application | Select a group and inspect paired rows and central width | The pane spans the central width and its rows agree with the canonical mapping | Planned, BPR-0095 |
| Renderer is virtualized | Instrumented application | Load a very large file and inspect widget count at 1.0x, 1.25x, and 2.0x | Widget count stays bounded by visible UI structure as file length grows by orders of magnitude, not merely that rendering completed | Planned, BPR-0096 |
| Qt is a presentation adapter | Qt adapter harness | Drive every primary action and bounded state change | Actions produce semantic intents, `QAction` state follows command projections, and bounded changes use incremental signals rather than full resets | Planned, BPR-0097 |
| Navigation and state are correct | UI automation | Exercise navigation, selection synchronisation, focus order, loading, failure, cancellation, and restart | Selection follows the filtered actionable order and every state is explicit in text as well as enabled state | Planned, BPR-0097 |
| Presentation is natively acceptable | Real macOS composition | Owner-attended inspection of the assembled workbench | Readable contrast and correct native control rendering at supported sizes and scales | Deferred, BPR-0100, owner-attended |

## Deferred gates

RFC-BPR-0005's own text names no owner-attended requirement. One is inherited:
the RFC-BPR-0003 and RFC-BPR-0004 ledgers both defer the integrated-workbench
presentation-polish and native visual-acceptance pass to this RFC. BPR-0100
owns it and is the only owner-attended card here.

No RFC-BPR-0005 requirement needs target-native Windows execution, a GitHub
mutation, a credential, or a network service. Non-goals exclude full GitHub
mutations, so no safe test target is required.

## Documentation ledger

| Scope | Required documents | Disposition |
|---|---|---|
| Core navigation and command policy | Domain and state model | Planned, BPR-0098 |
| Host projection and persistence | System architecture | Planned, BPR-0098 |
| Workbench layout, drawers, focus, accessibility, error states | Workbench specification and appearance/accessibility documents | Planned, BPR-0098 |
| Composed comparison surface | Diff pipeline and rendering, link-map and alignment-space documents | Planned, BPR-0098 |
| Tests and fixtures | Fixture catalog and build/test guide | Planned, BPR-0098 |

## Closeout blockers

RFC-BPR-0005 cannot become `Auto-Verified` until RFC-BPR-0004 is
`Auto-Verified`, which requires RFC-BPR-0003, which requires RFC-BPR-0002 and
therefore real Windows x64 evidence in BPR-0036 and BPR-0053. BPR-0101 stays in
`backlog/` and makes no closure claim.
