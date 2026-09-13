# BetterPullReview: calm shell surfaces

**RFC pack v1, revision 2 · proposed RFC-BPR-0032 and RFC-BPR-0033 · 14 cards · 5 authored milestones**

Redesign the three surfaces a reviewer crosses before a review: the Accounts & hosts
page of Settings, Home, and the profile chooser, and stop the chooser from appearing
whenever the last window closes. This is a source-grounded design and implementation
pack, not a claim that the app has already been changed.

The owner's brief, 2026-09-13, with four screenshots: "this settings menu, home
screen, and profile selector [are] ugly as heck and so dang cluttered. The profile
select screen coming up every time you close the main window is terrible."

## Start here

Read `IMPLEMENTATION-PROMPT.md`, `supporting/SOURCE-AUDIT.md`, `design/DESIGN-NOTES.md`,
and the two RFCs in `repository-overlay/docs/rfcs/`. Open `design/shell-surfaces-v1.html`
(Settings and Profiles) and `design/home-v2.html` (Home) in a browser for the reference
layouts. Revision 2 (2026-09-13) replaced the first Home after the owner rejected it and
accepted the nine-board set through review; the Settings and Profiles designs are unchanged. Import `milestone-plan.json` as the default
execution plan: allocate live BPR issue IDs, remap the pack-local aliases, validate with
the installed MASK-managed implement-rfc skill, and keep its milestone grouping. Do not
replace the plan with a new unbounded planning exercise.

The pack-local aliases (`PACK-SHELL-01` to `PACK-SHELL-14`) are deliberately not
allocated live issue IDs. RFC numbers 0032 and 0033 were free in the live checkout at
`38f32226`; check for collisions at import and record a mapping rather than overwriting.

## What this delivers

| Workstream | Scope |
|---|---|
| RFC-BPR-0032 | Accounts & hosts as one card per server with sign-in inside the account row and enterprise servers as cards; Home as the bar, then rows always in state order inside tinted, collapsible panels with rails, Group by none, organization, repository or account with state bands nested inside, avatars on group headers, account chips, one Filter menu, the controls in the title strip, sentences for empties, and the session line in the status bar. |
| RFC-BPR-0033 | Closing the last window never presents the chooser: resident on macOS with reopening from the Dock, ⌘N and the Profiles menu; quit on Windows and Linux. The chooser becomes a sheet of rows with in-place rename and create and a failure notice that appears only after a failure. |

Both RFCs are presentation and composition revisions. They implement the owner-accepted
RFC-BPR-0028 Settings and Home statements and RFC-BPR-0018's row language; they change
no credential, provider, publication, queue, navigation, profile-identity or
runtime-ownership authority.

## What is deliberately not here

No new preference, no new intent, no stylesheet, no hard-coded palette, no personal
discovery (RFC-BPR-0026), no repositories drawer, no strip change, no profile deletion,
no launcher screen. The action list shows what the queue projection supplies today.

## Contents

- `repository-overlay/`: the two additive RFCs, ready to copy into `docs/rfcs/`.
- `cards/`, `milestone-plan.json`: prepared issue work and dependency-ordered milestones.
- `design/`: the reference layouts and the design notes (tokens, wireframes, critique).
- `supporting/`: the live-source audit and the integration map of seams and exclusions.
- `verification/`: acceptance matrix, attended-review subjects, pack-authoring validation.
- `scripts/`, `tests/`: offline pack-integrity helper and its negative tests.

## Verify the handoff

```sh
python3 scripts/verify_pack.py
python3 -m unittest discover -s tests -v
```

Both need only Python's standard library. Neither changes the BPR repository, starts
the app, or contacts a provider.

## Evidence and scope

Primary source: the live checkout at `38f32226351f5cc8e4657d55c3e151d8450c6ce5` and the
owner's four screenshots of 2026-09-13. Pack validation proves document and plan
integrity; it does not prove Qt behaviour, native support, or owner acceptance. The
design reference is an illustration of palette roles and layout, never runtime evidence.
