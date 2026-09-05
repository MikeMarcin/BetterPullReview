---
schema: "implement-rfc.card.v1"
id: "BPR-0098"
title: "Reconcile RFC-BPR-0005 documentation impact"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0097"]
---

# Reconcile RFC-BPR-0005 documentation impact

Update workbench layout, command map, focus model, drawers, epoch controls, custom rendering, accessibility, supported sizes, and error-state documentation. Identify core, host, and Qt ownership, the typed intents, events, observations, requests, and deltas, authority, concurrency and generation handling, and any evidence-backed no-impact region.

Run the immutable-base changed-path classification for this RFC.

## Changed-path classification

The RFC-BPR-0005 range from `55d365f16847d2ffc2c8fab83a5ce62eb695e2a0` to HEAD
touches 19 non-tracker paths: three core, four application, five Qt adapter,
six test, and the verification ledger. Every path routes to `src/core`,
`src/application`, `src/adapters/qt`, or `tests`, all of which have owning
instruction files, and each maps to a documented surface below.

## Dispositions

| Document | Disposition |
|---|---|
| Workbench specification | Updated. Command map with exact shortcuts, the ambiguous-shortcut rationale, compact-overflow behaviour and threshold derivation, and the drawer ownership split |
| Appearance and accessibility | Updated. Declared focus order, the rule that it never reorders a composed region's internal stops, and the accessible-name and key-sequence uniqueness rules |
| System architecture | Updated. Core, host, and Qt allocation for the workbench, including compact core state, limit-rather-than-truncate, per-domain staleness, and compose-rather-than-rebuild |
| Fixture catalog | Updated. The three new suites and the virtualization figures |
| Build and test guide | Updated. Focused commands for all three suites, offscreen and serial, plus what the scale line reports |
| RFC-BPR-0005 ledger | Updated. Every claim-matrix row carries its result; the owner-attended row stays deferred |
| Diff pipeline and rendering | No-impact. The comparison surface is composed unchanged, so RFC-BPR-0004's documents remain correct |

## What the documentation now records that it did not

Three facts were discovered during implementation rather than planned, and each
would have been lost without being written down.

The drawer toggles cannot use plain `Alt+1` and `Alt+2`. The composed epoch
control holds `Alt+1` through `Alt+3` with widget-with-children context, both
bindings match whenever that control has focus, and Qt resolves an ambiguous
shortcut by triggering neither. The plain form produced two controls that
silently did nothing. The specification now states the constraint and the
reason, so a future editor cannot "simplify" the binding back.

Accessible names must be unique among focusable widgets, and this is enforced
by a test rather than left to convention. The rule was broken four separate
times before enforcement: a retired placeholder against the composed detail
pane, the toolbar's change buttons against the comparison surface's own
buttons, the drawer toggles against the drawer panels' list views, and the
overflow menu against the button that opens it. Visible labels may repeat;
accessible names may not.

Widget count is bounded by construction rather than by a virtualization layer,
because the source panes virtualize their own text layout. The documentation
says so explicitly, so that a later reader does not go looking for machinery
that was deliberately never written.

## Direct verification

- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`; `documentation_structure` and
  `documentation_issue_graph_contract` pass.
- Every claim-matrix row in the RFC-BPR-0005 ledger now carries a result rather
  than `Planned`, and the one deferred row names its owner-attended card.

One correction was made during this slice and is worth recording: a bulk edit
initially copied the wrong result text into two claim rows, giving the link-map
row the scrollbar row's evidence and the presentation-adapter row the
selector row's. Both were corrected to their own evidence. A claim matrix whose
cells are individually plausible but collectively misattributed is harder to
catch than an obviously empty one.

## Result

Every RFC-BPR-0005 documentation obligation is discharged, and the three facts
discovered during implementation are recorded with their reasons rather than
their conclusions alone. BPR-0098 is complete.
