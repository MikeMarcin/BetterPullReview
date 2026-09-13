# BetterPullReview: calm shell surfaces

**RFC pack v1, revision 3 · proposed RFC-BPR-0032, RFC-BPR-0033 and RFC-BPR-0034 · 17 cards · 6 authored milestones**

Redesign the surfaces a reviewer crosses before a review: the Accounts & hosts and
Appearance pages of Settings, Home, and the profile chooser; stop the chooser from
appearing whenever the last window closes; and let each theme keep its own colours. This is a source-grounded design and implementation
pack, not a claim that the app has already been changed.

The owner's brief, 2026-09-13, with four screenshots: "this settings menu, home
screen, and profile selector [are] ugly as heck and so dang cluttered. The profile
select screen coming up every time you close the main window is terrible."

## Start here

Read `IMPLEMENTATION-PROMPT.md`, `supporting/SOURCE-AUDIT.md`, `design/DESIGN-NOTES.md`,
and the two RFCs in `repository-overlay/docs/rfcs/`. Open `design/shell-surfaces-v1.html`
(Accounts and Profiles), `design/home-v2.html` (Home) and `design/appearance-v2.html`
(Appearance) in a browser for the reference layouts. Revision 2 (2026-09-13) replaced the
first Home after the owner rejected it; revision 3 (the same day) added Appearance and the
per-theme colour behaviour after review. Import `milestone-plan.json` as the default
execution plan: allocate live BPR issue IDs, remap the pack-local aliases, validate with
the installed MASK-managed implement-rfc skill, and keep its milestone grouping. Do not
replace the plan with a new unbounded planning exercise.

The pack-local aliases (`PACK-SHELL-01` to `PACK-SHELL-17`) are deliberately not
allocated live issue IDs. RFC numbers 0032 to 0034 were free in the live checkout at
`38f32226`; check for collisions at import and record a mapping rather than overwriting.

## What this delivers

| Workstream | Scope |
|---|---|
| RFC-BPR-0032 | Accounts & hosts as one card per server with sign-in inside the account row and enterprise servers as cards; Home as the bar, then rows always in state order inside tinted, collapsible panels with rails, Group by none, organization, repository or account with state bands nested inside, avatars on group headers, account chips, one Filter menu, the controls in the title strip, sentences for empties, and the session line in the status bar. |
| RFC-BPR-0034 | The Appearance page as a sideways theme strip of six tiles with the diagonal System split painted from the real palettes, one-line type rows with info-glyph popovers and a labelled preview that exercises every control, and a colours card titled for the theme on screen over a grouped list of all 47 roles; plus the one behaviour change: each theme keeps its own colour customisation. |
| RFC-BPR-0033 | Closing the last window never presents the chooser: resident on macOS with reopening from the Dock, ⌘N and the Profiles menu; quit on Windows and Linux. The chooser becomes a sheet of rows with in-place rename and create and a failure notice that appears only after a failure. |

RFC-BPR-0032 and RFC-BPR-0033 are presentation and composition revisions; RFC-BPR-0034 adds one domain change, one customised palette per theme, with a store migration. They implement the owner-accepted
RFC-BPR-0028 Settings and Home statements and RFC-BPR-0018's row language; they change
no credential, provider, publication, queue, navigation, profile-identity or
runtime-ownership authority.

## What is deliberately not here

No new preference beyond per-theme palettes, no stylesheet, no hard-coded palette, no personal
discovery (RFC-BPR-0026), no repositories drawer, no strip change, no profile deletion,
no launcher screen. The action list shows what the queue projection supplies today.

## Contents

- `repository-overlay/`: the three additive RFCs, ready to copy into `docs/rfcs/`.
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
