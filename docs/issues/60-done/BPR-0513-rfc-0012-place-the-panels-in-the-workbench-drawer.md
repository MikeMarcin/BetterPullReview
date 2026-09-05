---
id: BPR-0513
title: "Qt: place the side-chat panel and finding queue in the workbench drawer"
status: Done
rfc: RFC-BPR-0012
depends_on: [BPR-0504]
---

# Qt: place the side-chat panel and finding queue in the workbench drawer

## Why

[[BPR-0504]] composes `qt::side_chat_panel` and `qt::finding_queue_view` in
the shipped application, drives them through the composed gesture path, and
cut-tests each, but does not lay them into `workbench_window`'s drawer: that
is an edit to `src/adapters/qt/`, which was outside that slice's file surface.
A reviewer today can reach a finding through the composed gestures and cannot
see the panel that presents it.

## Required final state

- The side-chat panel and the finding queue are placed in the workbench
  drawer beside the existing agent launch panel, under the drawer's existing
  collapse and scale behaviour, and the workshop view opens from a finding's
  action inside the draft editor already in the drawer.
- Placement is presentation only: the panels keep their intent-out contract,
  and no new member or edge appears in the composed hosts.
- `qt_composed_github_scale_100`, `_125`, and `_200` stay green, and the
  composed window's capture shows the panels at each scale.

## Non-goals

- A chat session host — [[BPR-0514]].
- Any new action or field on either panel — [[BPR-0502]], [[BPR-0503]].

## Required direct evidence

- `ctest --preset dev -R '^(qt_composed_github_scale_(100\|125\|200)\|application_agent_finding_composition_contract\|composed_application_hosts)$' --output-on-failure`.
- A composed-window case that opens the drawer and finds both panels by
  their accessible names; removing either placement fails it by name.

## What this slice delivered — 2026-09-02

`workbench_window` now composes `qt::side_chat_panel` and
`qt::finding_queue_view` in its conversations drawer, beside the agent launch
panel and above the draft editor whose comment workshop a promotion fills, and
hands both out through `side_chat()` and `finding_queue()`. The application
adopts those two rather than constructing a second, invisible pair: its
`SideChat` and `FindingQueue` members became observers of what is on screen,
and the fixture cut flags now cut the one edge that decides anything — the
dispatch. Neither panel gained a member, an action, or a field, so the member
guards in `side_chat_panel_tests.cpp` and `finding_queue_view_tests.cpp`
compile unchanged.

The drawer's column had already outgrown a short drawer, and two more panels
made it show every field overlapping. The column now scrolls vertically inside
the drawer. Collapse, width reporting, and the toggle are untouched, which is
what keeps this placement rather than a redesign.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Both panels are placed in the open drawer, laid out by its own column, and shown at every supported scale | `ctest --preset dev -R '^qt_composed_github_scale_(100\|125\|200)$' --output-on-failure` | Pass, 0.92 s / 0.73 s / 0.99 s | this commit |
| Placement is presentation only: neither panel's member aggregate moved | `ctest --preset dev -R '^(qt_side_chat_panel\|qt_finding_queue_view)$' --output-on-failure` | Pass, 0.34 s / 0.22 s | this commit |
| The composed application still reaches one delivered finding, one promotion, and one accepted revision with zero GitHub mutation calls | `ctest --preset dev -R '^application_agent_finding_composition_contract$' --output-on-failure` | Pass, 14.60 s | this commit |
| Both constructors are still anchored in the shipped binary | `ctest --preset dev -R '^composed_application_hosts$' --output-on-failure` | Pass, 1.65 s | this commit |
| Every construction cut still reports its effect as zero | `ctest --preset dev -R '^application_agent_finding_((full_agent)\|((finding_host\|workshop\|mcp\|chat_panel\|queue)_construction_cut))_contract$' --output-on-failure` | Pass, 6 of 6, 14.1–15.2 s | this commit |
| The digest-pinned composition file is repinned and round-trips | `ctest --preset dev -R '^application_review_round_trip_composition_contract$' --output-on-failure` | Pass, 31.80 s after repinning `bpr_expected_full_application_digest` then `bpr_expected_production_app_manifest_digest` | this commit |
| Removing either placement fails the composed-window case by name | delete `conversationLayout->addWidget(SideChat);`, rebuild, `ctest --preset dev -R '^qt_composed_github_scale_100$'`; then the same for `FindingQueue` | Fails: *the side chat is laid into the drawer's own column, not merely parented to it*, and *the finding queue is laid into the drawer's own column, not merely parented to it*. Source restored byte-exact, md5 `6f51882e84756a878c68fdbcf68b99b7` before and after each | this commit |
| Nothing else in the Qt surface or the composed render moved | `ctest --preset dev -R '^(qt_.*\|application_render)$' --output-on-failure` | Pass, 44 of 44 | this commit |

### Capture evidence

`qt_composed_github_scale_100`, `_125`, and `_200` each save
`composed-github-<scale>-agent-drawer.png` beside their existing renders under
the build tree. Each is the window's own grab cropped to the conversations
drawer's exact device rectangle — not a whole-image diff — after the column is
scrolled to the side chat. All three show the agent launcher, the side chat's
ten session-contract fields with its opt-ins, scope selector, composer, and
actions, and the finding queue's list, its seven disclosed facts, and its six
action buttons.

### Not delivered

Neither panel was added to the window's *declared* focus order. The declared
order requires an accessible name and an accessible description on every stop,
and these panels publish names without descriptions; supplying that wording is
a change to the panels, which this slice's non-goals exclude. Both are reachable
in the live tab chain today.

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0) and ran the three composed-window scales, both
panel suites, the seven RFC-0012 end-to-end contracts,
`composed_application_hosts`, `application_composition_contract` (117.71 s),
`application_review_round_trip_composition_contract`, and
`qt_workbench_window_scale_100`: 16/16. The 100 % cropped drawer capture was
inspected: launcher, the side chat's session fields, opt-ins, composer and
actions, and the finding queue with its seven facts and action row are all
present. Two things folded into this commit: `clang-format` reflowed two
lines in `github_application.cpp` that [[BPR-0504]] had landed unformatted
(the integrator's earlier check filtered for the wrong severity word), and
both digest pins were recomputed afterwards in two passes. One presentation
nit for [[BPR-0506]]: at 100 % the finding queue's six action buttons
overflow the drawer's width and the last two are clipped.
