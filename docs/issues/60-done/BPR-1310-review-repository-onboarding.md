---
schema: "implement-rfc.card.v1"
id: "BPR-1310"
title: "Verify rendered repository onboarding and prepare owner review"
status: "Done"
rfcs: ["RFC-BPR-0062"]
depends_on: ["BPR-1309"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"873bd4b0b1705e98f3f80a94b679017e8e77bcc9","evidence":{"kind":"command","detail":"Frozen clean export of 873bd4b0b 18/18: personal-discovery public course, boundaries, qt_settings_window x4, qt_workbench_window x3, qt_home_list x3, qt_composed_github x3, settings effect boundary; 126 offscreen renders (light/dark/high-contrast dark, wide/narrow, 100-200%) inspected. Settings renders light offscreen in dark modes (existing limitation)."}},{"criterion":"C2","outcome":"pass","revision":"873bd4b0b1705e98f3f80a94b679017e8e77bcc9:86678047362af9a04c838b75f209feb2c55e42c8ba1b6a3d0015a820cf902569:f884c2c31d80a9db5ed0dc14c2214d4ea4043dfd27b4d597af1733a13de5e198","evidence":{"kind":"observation","detail":"Owner native macOS Accepts in fresh session 6226f23752342fb0ad75662926641f35 (handed back cursor 9): BPR-1310-home-v003 decision c46e3ec4, BPR-1310-repositories-v003 decision 2cda22d2, no notes. VoiceOver and Windows/Linux native not claimed."}}]
---

# Verify rendered repository onboarding and prepare owner review

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Rendered keyboard/accessibility courses cover long identities, narrow widths and 100–200% scale in supported appearances.
- C2: Owner judges Home and the addition dialog as separate Accept/Reject criteria in a fresh short attended session with an exact self-contained frozen build.

## Prepared review handoff

Implementation source: `8e2f855e9061b4ce9cc150c1668356c8301b9b01`.
Frozen binary SHA-256:
`9179cb17b777e2c38b00bad7eaed844067662f30af25780be5bbaa893119c194`.
The source, build, dependency cache and fixtures are inside
`artifacts/four-drafts-r1/home-frozen-v001/`; the frozen public course passes
10.71s. Launcher checks every path absolutely and reports early startup failure.
No owner desktop was captured or driven.

Fresh attended session `5317162ad2f023440b559ad4145ed579`, prepared sequence 6:
Home (`BPR-1310-home-v001`) and dialog (`BPR-1310-addition-v001`) are separate
one-criterion Accept/Reject candidates, both ready/protected, no verdicts.
State: `/private/tmp/bpr-repository-home-biqu9sse/state`.
Registry: `/private/tmp/bpr-repository-home-biqu9sse/registry`.
Manifest: `artifacts/four-drafts-r1/home-review-manifest.json`.
Only the frozen root/installation is protected; independent main source work
can proceed after writer release. The coordinator owns decision intake.

The child socket bind was refused by the execution sandbox. Its escalation
request was cancelled by coordinator interruption before any server started;
the coordinator owns starting the exact same session's loopback server.
Automatic wait/wake has not been qualified on this runtime. Decisions are
saved independently; if no qualified coordinator wait is sustained, hand back
and send “Resume the attended review” in the coordinator chat.

## Owner verdicts on v001 (dispatch r5, 2026-10-08)

Session `5317162ad2f023440b559ad4145ed579` was handed back at cursor 10 with
both candidates rejected; consumed by dispatch `parallel-integrator-r5-001`.

- `BPR-1310-addition-v001` (fingerprint `c27d5c2d…ed5a`), decision
  `c0939eaa-0bf6-4fc8-80e0-748ed6583ba6` (it supersedes an earlier identical
  note without the last sentence, `ca34dca7…`): "This information is still
  way hidden down the bottom of a settings page that is full of mostly
  irrelvant details. We probably need to move this out into a new settings
  page just focused on managing repositories. And with a better UX"
- `BPR-1310-home-v001` (fingerprint `4e1ef338…cd9d`), decision
  `5b1c12d0-70a8-49b4-8999-54b43f52926b`: "I want a clearer call to action for
  first time experience."

These amend RFC0062's "Settings → Reviews & queue remains the management
location" and the "same row with stronger emphasis" first-run wording; the
latest owner direction wins and the original criteria stay. The next round
gives saved repositories their own Settings page with the shared dialog, and
gives signed-in Home a clearer first-time call to action. C2 stays pending on
a new frozen candidate and a fresh session; BPR-1311 keeps its dependency.

## v002 candidates (dispatch r5, 2026-10-08)

`2f6fe7f1a` responds to both rejections; the frontend-design skill was
loaded first. Settings gains a **Repositories** page right after Accounts &
hosts. It shows an "Saved for @account on host" line, the exact saved list,
**Add repository** (the shared dialog) and **Remove** (also Delete or
Backspace), using the existing saved-repository removal intent with no new
schema. With nothing saved it shows "No repositories yet" and Add. Signed
out, it lists the kept records with their account and host and disables
Remove. Reviews & queue keeps only a pointer to the new page. Observer rules
stay there because they are queue behaviour, not the saved list. Signed-in
Home with no saved repository replaces the old row, directly under the Open
bar, with the heading "Start with a repository", one sentence and a default
**Add repository** button. That block retires after the first save; it never
shows while the queue restores, for a filtered or empty queue, or signed out.

Evidence. The worker ran 16 focused rows and all pass: Settings window at four
scales, workbench window and Home list at three scales each, the composed
GitHub rows, the Settings effect boundary, the composed personal-discovery
course and the core boundary. That composed course covers the first-run call
to action, invalid input, cancelled setup, duplicate, a real SQLite refusal
and retry, offline saves, a sibling window, restart, and removal by button
and by Delete. The integrator's combined check reran the Home rows against
HEAD plus this slice in an isolated tree and they pass, as do the clean-export
boundaries. Format and changed-line tidy pass. The worker inspected 180
offscreen renders (six appearances, two widths, three scales). Settings
renders light in every explicit mode offscreen, an existing limitation of the
Settings top-level palette.

The self-contained frozen root `artifacts/four-drafts-r1/home-frozen-v002/`
(source archive `716e1f07…5ff2`, binary `55e9ed37…0d34`, launcher
`f884c2c3…e198`) passes its public course in 15.93 s and its boundaries 3/3.
The launcher refuses at once when its files are missing. Fresh session
`940f5d334de0b75c0cbd0acf966c5438` (cursor 6) asks two separate questions:
`BPR-1310-home-v002` on the first-time call to action and
`BPR-1310-repositories-v002` on the Repositories page. C2 stays pending.

## v002 rejections (dispatch r5, 2026-10-08)

Session `940f5d334de0b75c0cbd0acf966c5438` was handed back at
2026-10-08T15:31:16Z (cursor 10) with two Rejects:

- `BPR-1310-home-v002` (fingerprint `e5ec7cbd…e274`), decision
  `c7d78e69-4e26-4a57-abae-1494eab726ef`: "Just the button that opens
  settings is okay, And if there is no repositories there is no reason to
  show the review cycles needs me I dont think, it would never have anything."
- `BPR-1310-repositories-v002` (fingerprint `59c3b03d…f57a`), decision
  `902a27c6-107b-4c96-8f32-097fc165a90a`: "This link should go to the
  repositories settings page.

  This popup sucks, make the experience more like the accounts page."
  Its two screenshots are kept in the ignored
  `artifacts/four-drafts-r1/home-v002-verdicts/`. They show the empty
  Needs me lane's "saved repositories and observer rules" link, which opened
  Reviews & queue, and the modal Add repository dialog.

The RFC records the resulting amendment to AC01, AC05 and AC06. The same
Home owner is correcting the slice as v003. Its scope: a single Home
first-run button that opens Settings › Repositories; Review Cycles lanes that
nothing can populate are hidden while no repository is saved; the empty-lane
link opens Repositories; and the modal is replaced by an Accounts-style
in-page flow. C2 stays pending.

## v003 candidates (dispatch r5, 2026-10-08)

`873bd4b0b` responds to both v002 rejections; the worker loaded the
frontend-design skill first. The modal Add repository dialog is gone.
Settings › Repositories mirrors Accounts & hosts: a "Saved for @account on
host" section with the exact list and Remove, and a "New repository" section
that opens in place with the destination line, a labelled Repository field,
progress and refusal announced beside it, "Choose account or set up
server…", Add and Cancel. Every Add repository entry point opens that page
with the field focused. A save keeps the page open and confirms what was
added.

Signed-in Home with no saved repository shows only an "Add a repository"
button under the Open bar. While nothing is saved and the queue is empty, the
Needs me lane is hidden, because only observer-rule or followed rows could
fill it and none exist then. Review Cycles and Open Project… stay, because
local cycles do not depend on repositories. The empty-lane link covers
"saved repositories" and opens Settings › Repositories. Observer rules stay
reachable through Filter › Open Reviews & queue and the Settings sidebar. The
open-PR shortcut's direct save, `src/application` and `src/app` are
unchanged.

Evidence. The worker ran 16 focused rows and all pass. The composed course
covers the exact destination, invalid input, cancelled server setup
returning to the draft, stale destination, duplicate, real SQLite refusal and
retry, offline and secondary-limit saves, sibling window, restart, removal
and zero provider mutations. Format and changed-line tidy pass, and the
worker inspected 126 offscreen renders. The self-contained frozen root
`artifacts/four-drafts-r1/home-frozen-v003/` (archive `0c90420d…0d07`,
binary `86678047…2569`, launcher `f884c2c3…e198`) passes 18/18 in its clean
export: the public course, three boundaries, Settings at four scales, and
the workbench, Home list and composed GitHub rows at three scales each. Known
offscreen limit: the Settings window renders light in explicit dark modes,
which predates this work.

Fresh session `6226f23752342fb0ad75662926641f35` (cursor 6) asks two
questions: `BPR-1310-home-v003` on the first-time Home and
`BPR-1310-repositories-v003` on the in-place Repositories page. The decided
v002 session's server is stopped. C2 stays pending.

## v003 owner acceptance (dispatch r5, 2026-10-08)

Session `6226f23752342fb0ad75662926641f35` was handed back at
2026-10-08T16:04:26Z (cursor 9) with two Accepts and no notes. They are
`BPR-1310-home-v003` (fingerprint `d1cd3c23…6b2a`, decision
`c46e3ec4-4598-4483-9631-d6f6f20db359`) and `BPR-1310-repositories-v003`
(fingerprint `7d256fb2…3808`, decision
`2cda22d2-846f-426f-9846-22e5761d6853`). The accepted candidate is source
`873bd4b0b` with frozen binary `86678047…2569` on native macOS, and no source
or test file has changed since. Its clean-export 18/18 evidence stands.
VoiceOver and Windows/Linux native behaviour are not claimed.
