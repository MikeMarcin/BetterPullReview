# RFC-BPR-0005 Verification Plan and Ledger

RFC base: `55d365f16847d2ffc2c8fab83a5ce62eb695e2a0`

Status: **workbench host and selected-file diff publicly connected.** Since
BPR-0191, `qt::workbench_window` is the window the shipped
application opens. BPR-0226 now routes its primary intents and drawer widths to
the workbench host, publishes host projections and failures to the real window,
observes exact accepted pull-request authority, clears invalidated authority,
and retains a session/default settings path when SQLite cannot open. BPR-0227
now resolves a real file-row gesture through the retained epoch cache actor and
publishes the exact comparison through the production diff host.

Rows below remain direct evidence for their named component, public composition,
or fixture-driven surface. They now prove the shipped user-selected file-diff
path. The exact final-head BPR-0100 attended gate is accepted. RFC-BPR-0005
stays `Proposed`; BPR-0101 waits for the ordered closure chain.

## Why this RFC can be implemented now

RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0004 remain incomplete and
`Proposed`. Their live cards include target-native Windows and attended work.
Done BPR-0088 records the
product owner's direction to advance the implementation frontier despite that
open work. Closure order is unchanged: this RFC cannot become `Auto-Verified`
before RFC-BPR-0004 does.

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

## Historical composition investigation

This section preserves the pre-BPR-0191 investigation that led to replacing
`github_window`. It is not the current product state. At final HEAD the binary
contains `workbench_application_host`, `workbench_window`, and
`workbench_drawer`, contains no `github_window`, and includes the selected-file
diff wire named in the status correction above.

The application showed `github_window` and did not contain `workbench_window`.
**BPR-0191 reversed both.** The shipped binary now contains 591
`workbench_window` symbols and *zero* `github_window` symbols — the old window
is not merely unreachable, nothing links it in at all.

`github_window` is **deleted**. Its 2,074 lines of assertions were not deleted
with it: they moved to `composed_github_tests.cpp` and now run against
`workbench_window`. Each was forced by a real shipped defect, so moving them was
the condition on removing the window — three were restated where the shell
genuinely replaced the mechanism they measured (the content splitter and the
whole-window scroll), each to the guarantee the original bought.

| Measurement | Result |
|---|---|
| `nm build/dev/src/app/better_pull_review \| c++filt \| grep -cF workbench_application_host` | `0` at the time of measurement; **present after BPR-0190** |
| the same for `workbench_window` | `0` at the time of measurement; **present after BPR-0191** |
| the same for `github_window` | non-zero at the time of measurement; **0 after BPR-0191** |
| the same for `workbench_drawer` | `0` at the time of measurement; **present after BPR-0191** |
| implementations of `workbench_settings_port` in `src/` | none at the time of measurement; **supplied by BPR-0189** |
| implementations of `workbench_observation_port` in `src/` | none at the time of measurement; **supplied by BPR-0192** |

At that measurement point, the shell this RFC is named for — primary
navigation, compact overflow, drawers, the full-width detail pane, the declared
focus order, and the accessible names BPR-0097 repaired — had never been on
screen. The application then showed `github_window`, which grew the RFC-0003
and RFC-0004 surfaces directly. BPR-0188 decided the shell would replace it,
and BPR-0191 performed that replacement.

The later final-state defect was different: the shell was on screen, but its
workbench callbacks were empty, its accepted review was not observed, and host
projections terminated in a non-rendering sink. BPR-0226 resolved those wires;
BPR-0227 resolved the distinct selected-file diff path.

At the historical measurement point neither of this RFC's two ports had an
adapter. The later adapters and host remain valid component work; final-HEAD
reachability is stated separately above.

### Staged actionability, stated rather than implied

BPR-0192's observation adapter reports **every file in the review**, unfiltered.
`workbench_file_observation::Actionable` is named for a filter that does not
exist yet: actionability is decided from receipts, and RFC-BPR-0007's review
ledger host is not composed. The filter arrives when it is.

This is recorded here, and in the adapter itself, rather than left for a reader
to infer from an unfiltered list. An "actionable" set that quietly means "all
files" is the same class of half-truth as a ledger row naming a surface it was
not proven against.

### What a real adapter found that a fake could not

BPR-0189 supplied the settings adapter and found a defect that only a real
implementation could surface. `progress_scope` installs a SQLite progress
handler every 256 virtual-machine steps; a small statement finishes in fewer,
so a pre-cancelled call is never interrupted by it. With only a post-step
check, a cancelled write **is performed and then reported as cancelled** — the
caller is told it did not happen while the bytes on disk say it did. A fake
port answering `cancelled` from a flag would never have shown this, because a
fake has no bytes on disk to disagree with it.

The architecture allocation above is worth re-reading with this in mind. It
names a fourth region, **Composition root**, whose selected responsibility is
"wiring the workbench into the real application" and whose planned evidence is
"selected public-path evidence through the real executable." BPR-0226 now
delivers that region and the claim-matrix row below records its exact real-window
surface. BPR-0227 completes that composition by resolving the selected file's
material identities through the already-retained cache actor.

## Claim matrix

Every row's `Direct surface` now names the harness it was proven against.

| Claim | Direct surface | Command or interaction | What proves it | Result |
|---|---|---|---|---|
| Primary actions are top-level | `qt_workbench_window` harness | Invoke previous/next file and change by button and shortcut at supported widths | Actions are reachable and keyboard addressable at every supported width, or move into an explicit compact overflow without losing keyboard access | Verified. `qt_workbench_window` at three scales; compact overflow keeps window association so shortcuts survive both modes |
| Drawers return space to the centre | `qt_workbench_window` harness | Collapse and reopen each drawer, measure the central viewport, restart | Central viewport width is restored and persisted widths survive restart | Verified. Central viewport measured across five drawer stages; collapse hides rather than zero-sizes so the splitter returns the width |
| Link map maps groups | Rendered fixture | Compare ribbon and wedge bounds and selected identity to the RFC-BPR-0004 model through scroll and resize | Every visible ribbon agrees with the canonical group model and stays monotonic and noncrossing | Verified. `test_composed_ribbons_follow_the_group_model_through_scroll_and_resize` checks visible ribbon identity and bounds against the canonical model through scroll and resize |
| One shared scrollbar, no synthetic rows | `qt_workbench_window` harness | Inspect scrollbar policy on both panes and the document contents | Exactly one shared vertical scrollbar exists and neither document contains invented rows | Verified. Exactly one shared vertical scrollbar, both panes always-off, composed unchanged |
| Lower pane is full-width detail | `qt_workbench_window` harness | Select a group and inspect paired rows and central width | The pane spans the central width and its rows agree with the canonical mapping | Verified. Pane width equals the centre across five drawer stages; rows checked against `core::detail_for_selection` at canonically mapped lines |
| Renderer is virtualized | Instrumented `qt_workbench_window` harness | Load a very large file and inspect widget count at 1.0x, 1.25x, and 2.0x | Widget count stays bounded by visible UI structure as file length grows by orders of magnitude, not merely that rendering completed | Verified of the composed window. 60/6,000/60,000 lines produce **218/218/218** window widgets, 29/29/29 surface, 5/5/5 pane, at all three scales. Restated by BPR-0191: the earlier 68/68/68 was measured before the shell composed the session chrome, pull-request metadata, description and appearance surface, which is why the window figure rose. The surface and pane figures did not move, which is the substantive claim — the comparison surface itself builds no widget per line |
| Qt is a presentation adapter | Qt adapter harness | Drive every primary action and bounded state change | Actions produce semantic intents, `QAction` state follows command projections, and bounded changes use incremental signals rather than full resets | Verified. Toggles and navigation emit core intents only; enablement and disabled reasons come from projections; drawer contents use insert/remove/data-change rather than reset |
| Navigation and state are correct | UI automation | Exercise navigation, selection synchronisation, focus order, loading, failure, cancellation, and restart | Selection follows the filtered actionable order and every state is explicit in text as well as enabled state | Verified. Exact untruncated OIDs, four distinguishable textual states, declared focus order, unique accessible names and key sequences |
| Conversation and code selection synchronize without changing source layout | Real `application_diff_workbench_public_path` executable plus three-scale Qt harnesses | Select the current right-side fixture thread, select exact source lines, inspect outdated/file-level roots, and reverse the selected comparison | A root or reply reveals an exact identity-proven endpoint and source line through the existing semantic transition; a source gesture reverse-filters by exact file/side/line; a narrow margin marker is rendered; unavailable endpoints receive no guessed marker; comment prose remains in the drawer | Verified mechanically by BPR-0414. The public course emits `CONVERSATION_ANCHOR_EVIDENCE_OK` only after asserting right-side zero-based line 41, one exact drawer root, and a non-empty marker rectangle, then saves the composed capture. Focused Qt tests exercise the endpoint-agnostic adapter, reply-to-root resolution, reverse filtering, and defensive unavailable projections. The production path places only a right endpoint whose selected commit equals the accepted GitHub `headRefOid`; left fails closed without the merge-base identity. Reachable outdated/file roots explain non-placement, while malformed missing-side or missing-line provider payloads fail before projection. The marker-and-drawer product boundary is packaged at `reviews/conversation-code-boundary-v001/` for pending owner direction; no inline-comment authorization or RFC closeout is claimed |
| Deep-thread rendering has deterministic content without false recording provenance | Constructed `deep-thread-v1` projection fixture and independent contract test | `python3 tests/scripts/bpr0417_deep_conversation_fixture_contract_tests.py`; `ctest --preset dev --output-on-failure -R 'github_conversation|github_fixture'` | The valid tree has a null-reply root comment, a five-reply-edge chain, and siblings at two depths; exact schema, stable identities/order, and constructed provenance are enforced; absent-parent and cycle cases remain separately labelled non-provider-reachable Qt-defense inputs | **Verified.** The fixture lives outside `contracts/v1`, changes no shared public-path bytes or production loader, and cannot claim recorded GitHub evidence. Ten direct contract tests pass and include hostile mutations that fail if provenance, depth, sibling breadth, parent ordering, provider reachability, absent-parent/cycle shape, canonical bytes, or valid/hostile separation weakens. The ordinary network-isolated `github_conversation_fixture_contract` registration is restored in the default `dev` plan and passes under CTest alongside the conversation panel/model suites, the network-isolation gates, and the formatting, documentation, and hygiene gates. No BPR-0256 artifact or generated live-plan byte was regenerated. BPR-0418 still owns rendered map consumption and the evidence that the chosen view is usable. |
| One deep thread is readable as a map, with the clicked branch visible and on screen | Composed `qt::workbench_window` conversation drawer plus the `qt_github_thread_map_scale_{100,125,200}` rendered suite | `ctest --preset dev --output-on-failure -R 'github_conversation|workbench|thread_map'` | The map lays out every projected node and one edge per non-root; the complete root-to-selection path is marked and no node off that branch is; the selected node and its parent are inside the map's **visible** content rect after a drawer selection; ancestor and on-path-edge pixels change between rasters while off-branch pixels are identical; a press lands on a node asserted visible first and moves the shipped drawer's current index; and the map promotes exactly the nodes `github_conversation_model` promotes | **Verified at the BPR-0418 closing commit, corrected at BPR-0453.** 19/19 pass on `dev` for the card's own selector, and 22/22 once `qt_composed_github` is included — that suite builds the same window through the same drawer and the first selector did not match it, so the slice broke it and [[BPR-0453]] repaired it within the hour. An independent review of the pre-commit slice returned four blocking findings, every one a check that passed while its claim was false: no scroll-to-selection (2 of 7 branch nodes on screen at 1400x900), a crossing test whose assertion was reached zero times and compared `Slot` to `Slot`, a raster row satisfied by the focused box alone, and a cycle answered differently from the drawer's own tree. All four are repaired and each repair is confirmed by replaying the exact defeat that beat the first version. The map carries no read/unread state, because none exists to carry. Content is the constructed `deep-thread-v1` fixture plus one locally constructed forked shape, so this is evidence that the view renders real reply structure, not that a recorded pull request contained one. |
| The conversation-to-code boundary is the right product direction | Owner-attended offline review page, batch `0a6b4763976a05a7f14af6df` | `build_attended_review.py validate-verdict --manifest .../offline-package/attended-review-manifest.json --verdict .../human-verdict.json` | A validated, complete, digest-bound export in which the owner accepts or rejects the marker-and-drawer boundary against a capture of the shipped composition | **Rejected, and the question is still unanswered.** The owner rejected the presented state on 2026-08-25; the export validates against its exact manifest and is recorded with its validation evidence in `docs/verification/reviews/conversation-code-boundary-v001/`. The rejection does not authorize inline comment bodies — the returned claims say `inline_comment_body_authorized: false` — and none of its three grounds is the boundary question itself. The first ground is decisive: the source panes render no line numbers, so the criterion "unmistakably associated with right-side line 42" was unjudgeable as evidenced. Every automated check of that exact line passed, because they all read the model and none asked whether a person could see the number. [[BPR-0455]] and [[BPR-0456]] own the fixes; the predecessor batch `ab6145717475dfdfddd6206d` was withdrawn unanswered when [[BPR-0418]] changed the drawer it depicted. |
| A reviewer can read which line a pane row is | Composed `qt::diff_workbench` at three scales plus the `application_diff_workbench_public_path` capture | `ctest --preset dev --output-on-failure -R 'diff_workbench|workbench_window|composed_github|measured_alignment|link_map|comparison_scroll'` | Each pane paints its own one-based numbers in a reserved gutter; the gutter is sized to the document's digit count; scrolling changes the numbers; the two panes agree at the top and diverge past a one-sided block; and no ribbon, band, marker, scroll, or widget-count claim moves | **Verified at the BPR-0455 closing commit.** 23/23 pass. The roles this draws — `code_gutter_background` and `code_line_number_text` — have been declared in every palette and contrast-validated in every profile since RFC-BPR-0006, against text nothing rendered; BPR-0455 is the first consumer. Two negative runs prove the rows: painting nothing into a reserved gutter fails the raster row, and a fixed-width gutter fails the digit-count row. The first version of the rendered check passed against one-block panes because it published without a loading context, and the second sampled the pane's frame border rather than the gutter — both are corrected and recorded. The toggle the owner asked for in the same sentence is **not** here: [[BPR-0457]] owns it, because a Qt-local boolean would violate the adapter boundary. |
| Replies read as a feed grouped by root, not a staircase | Composed `qt::github_conversation_panel` with a five-deep chain | `ctest --preset dev --output-on-failure -R 'qt_github_conversation_panel'` | The view indents no row by depth; a five-deep reply occupies the same extent as a one-deep reply; the nearest painted pixel is at the same inset for both; and a reply's inset is still strictly greater than its root's | **Verified at the BPR-0456 closing commit.** 28/28 pass across the panel, map, window and diff scales, composed GitHub, and the three geometry surfaces. Two negative runs prove the rows: restoring the view's depth indent fails the settings assertion, and keeping the view flat while the delegate scales by depth fails the **pixel** assertion — which is why the check measures rasters instead of reading `indentation()`. Product direction from the BPR-0414 verdict; depth remains in the model and is read on [[BPR-0418]]'s map, which is the owner's own stated reason for accepting a flat feed. |
| Shipped workbench host and selected diff are connected | Real `application_workbench_composition_contract` executable | Accept the recorded pull request, select the real file row, navigate, invalidate exact and mismatched identities, and force SQLite-open and observation failures | Exact bounded authority reaches the host; each selected endpoint resolves on the retained cache session; complete repository/PR/anchor/blob/file/options/algorithm/generation identity reaches the diff host; mismatches preserve state | Verified by BPR-0226 and BPR-0227. Independent file-gesture, selected-request, and exact-comparison markers pass; runtime fixture-helper calls remain zero; a test-only cut of the real Qt-to-host selection wire deterministically fails the public trace while that helper still exists |
| Presentation is natively acceptable | Real macOS composition | Owner-attended inspection of the assembled workbench through the recorded selected-diff path | Readable contrast, native control rendering, coherent supported-size/profile layout, and C++ syntax colouring in both panes | Accepted by BPR-0100 on 2026-08-21. BPR-0220 later changed only an appearance-probe expectation, and the owner explicitly accepted that repair without requiring another digest-only presentation replay. Terminal closeout still owns its final replay. |

## Deferred gates

RFC-BPR-0005's own text names no owner-attended requirement. One is inherited:
the RFC-BPR-0003 and RFC-BPR-0004 ledgers both defer the integrated-workbench
presentation-polish and native visual-acceptance pass to this RFC. BPR-0100
owns it and now waits in Backlog behind BPR-0083 for terminal identity replay.
Its exact v003 response remains frozen for the historical binary, and its exact
v004 successor accepts the current final-head executable after the authorized
relink. Unchanged bytes need no new human response; a later relink before RFC
closeout requires a successor verdict.

The 2026-08-06 sitting initially could not proceed because no fixture launch
could remain open for a person. BPR-0186 added `--attended-launch`, and the
owner subsequently used it successfully; BPR-0187 supplied the recorded pull
request. The gate is about the final selected comparison, not launchability.
BPR-0227 made the selected state real, BPR-0233 added the meaningful recorded
comparison evidence, and the owner accepted the resulting v003 native workbench
pass on 2026-08-20. The owner accepted the separately digest-bound final-head
v004 successor on 2026-08-21. Ordered RFC closeout remains blocked only by its
declared dependency chain and autonomous final replay, not by attendance.

No RFC-BPR-0005 requirement needs target-native Windows execution, a GitHub
mutation, a credential, or a network service. Non-goals exclude full GitHub
mutations, so no safe test target is required.

## Documentation ledger

| Scope | Required documents | Disposition |
|---|---|---|
| Core navigation and command policy | Domain and state model | Updated. System architecture records the compact core state, the bounded cursor, and why a nonresident selection has no ordinal to step from |
| Host projection, selected resolution, and persistence | System architecture | Updated. Host ownership of the ordered set, independent endpoint lookup, bounded selected-pair recovery for multi-hop renames, reverse-pair and ambiguity rules, comparison replay, authority-refresh clearing, complete selected authority, changed-domain-only publication, and limit-rather-than-truncate are recorded |
| Workbench layout, drawers, focus, accessibility, error states | Workbench specification and appearance/accessibility documents | Updated. Command map with shortcuts, the ambiguous-shortcut rationale, compact overflow, drawer ownership split, declared focus order, and the accessible-name uniqueness rule |
| Conversation/source anchoring | Workbench specification and RFC-BPR-0005 ledger | Updated by BPR-0414. Exact identity-proven endpoint placement, root/reply inheritance, reverse filtering, the four-pixel marker, current-head binding, fail-closed missing merge-base behavior, and refusal to reuse `OriginalLine` as a current coordinate are recorded; the marker-versus-inline-body product question is isolated in one digest-bound attended packet |
| Composed comparison surface | Diff pipeline and rendering | Updated. Exact tree lookup feeds the existing comparison surface; no second cache session, mapping, or presentation authority was introduced |
| Conversation reply presentation | Workbench specification | Updated by BPR-0456. The linear feed, the single uniform reply step that keeps root grouping, why the view and the delegate each own half of it, and the fact that depth stays in the model and is read on the thread map are recorded |
| Source-pane line numbers | Workbench specification and appearance/accessibility document | Updated by BPR-0455. The per-pane gutter, one-based display over zero-based internals, the no-renumbering rule made visible, digit-count sizing and why it is not viewport-based, the two colour roles it finally consumes, and the conversation marker's unchanged position are recorded; the appearance document now says the gutter pair is rendered rather than only validated |
| Conversation reply-structure map | Workbench specification and fixture catalog | Updated by BPR-0418. The drawer's second view, its pre-order non-crossing layout, the two-weight branch highlight and the owner floor it satisfies, the scroll-to-selection behaviour that makes "highlighted" and "visible" the same claim, the single selection owner, the deliberate absence of read/unread state, the aggregate summary across a multi-thread drawer, the skipped relayout on an unchanged projection, and the promote-every-node-on-the-chain answer shared with `github_conversation_model` are recorded; the fixture catalog records that BPR-0418 consumed the constructed fixture without adding a production loader |
| Tests and fixtures | Fixture catalog and build/test guide | Updated. The three original suites, the manual-executor terminal algebra, and the virtualization figures retain their assert-counts-not-timing rule. BPR-0417 adds the separately catalogued constructed deep-thread projection, its exact provenance/topology contract, and an explicit no-impact result for shared recorded/public-path fixtures and production loaders. BPR-0256 v006 is frozen after its one attended run; BPR-0417's ordinary network-isolated `github_conversation_fixture_contract` registration is now part of the default plan and the card is closed. |

## Closeout blockers

RFC-BPR-0005 cannot become `Auto-Verified` until RFC-BPR-0004 is
`Auto-Verified`, which requires RFC-BPR-0003, which requires RFC-BPR-0002 and
therefore real Windows x64 evidence in BPR-0036 and BPR-0053. BPR-0101 stays in
`backlog/` and makes no closure claim.
