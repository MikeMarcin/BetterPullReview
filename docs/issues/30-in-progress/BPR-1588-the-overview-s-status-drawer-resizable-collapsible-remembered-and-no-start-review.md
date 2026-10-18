---
schema: "implement-rfc.card.v1"
id: "BPR-1588"
title: "The Overview's Status drawer: resizable, collapsible, remembered, and no Start review"
status: "In Progress"
rfcs: ["RFC-BPR-0085"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "drawer-foundation", "criteria": ["AC02"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
---
# BPR-1588: The Overview's Status drawer: resizable, collapsible, remembered, and no Start review

Make the Overview's side column a real drawer (RFC-BPR-0085 principle 7) and
apply the owner's Start review decision (D1). Its content is unchanged here;
[[BPR-1589]] rebuilds it.

## Outcome

- The column becomes **Status**: 340 pixels by default, drags from 260 to 560,
  collapses from its header toggle and ⌥⇧6, keeps its width per window across
  restart with the other drawer widths, and is one keyboard stop with its
  tools (Prepare review tour, Open on GitHub, Hide).
- No Start review or Continue review button. The reviewer's first ⌘2 (or the
  Review segment) from the Overview of a pull request they have not started
  selects the first unseen change's file (`EntryPath`); a started review keeps
  its restored reading place and a link or file row its own target. Return on
  the Files list opens the selected file. Courses that pressed the button use
  these.
- The attended review also asks the promote-label question (RFC-BPR-0085,
  "Promoting private material"): To comment, Draft comment or Use as draft.

## Acceptance

- RFC-BPR-0085 AC02.
- Owner, attended (Accept or Reject): "The Overview's side panel now behaves
  like the other drawers, and Start review is gone."
- Owner, attended (option question): the promote label.

## Boundaries

Qt presentation and the window's layout store for one more drawer width; the
entry rule is the window's existing `enter_review_at`. Coordinate with
[[BPR-1576]], [[BPR-1583]], [[BPR-1585]] and [[BPR-1577]], which touch the same
page.

## Execution record (2026-10-17)

`frontend-design:frontend-design` was loaded first.

**What changed.**
- **Status is a drawer.** `overview_status_drawer` wraps the existing side
  column (content unchanged until [[BPR-1589]]) with one tab, Status, and
  Prepare review tour, Open on GitHub and Hide as icons on its tab row; the
  full-width Prepare review tour button is gone. `overview_status_splitter`
  puts it beside the main column with the workbench seam's look and held
  resize cursor. It opens at 340, drags from 260 to 560 (never narrower than
  its content needs, so a large font cannot clip a control), collapses to an
  18-pixel rail (`workbench_drawer_rail` gained a labelled constructor) from
  Hide, the rail or Alt+Shift+6 (`toggleOverviewStatusAction`, catalogue entry
  "Status drawer", on the Overview only), and hides with the column in the
  description's full view.
- **Remembered per window.** `application::workbench_status_drawer {Width,
  State}` travels the summary fraction's path: the host holds it until the
  window record loads, validates it, stores it and projects it back; settings
  schema 22 adds `status_width` and `status_state` to the window row (a v21
  store migrates expanded at the default). Profile defaults never carry it.
  Schema 22 is also claimed by [[BPR-1584]]'s parked branch for its reading
  width: whichever lands second renumbers its step to 23.
- **No Start review (D1).** The button and its dispatch are gone. The
  reviewer's first Command-2 (or Review segment) from the Overview of a pull
  request they have not started selects the first unseen change's file; a
  started review's restored reading place, a link and a file row keep their
  own target. Return on Files already opens the file (`activated`).
- **Pins.** `application_review_round_trip_composition_contract`'s
  full-application and production-manifest pins are resealed for the Status
  dispatch in `github_application.cpp`; each old pin was checked to match the
  pre-change file exactly.
- **Owed with [[BPR-1580]].** Principle 7 lists the toggle in the View menu;
  the drawer toggles are toolbar actions with keys today, so the menu entry
  waits for BPR-1580's View menu work.

**Verification** (macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug):

| Check | Result |
|---|---|
| `qt_drawer_parts_scale_{100,125,200}` | Pass: opens at 340 with nothing reported; one tab and three tools; held resize cursor; a drag of 80 px reports 420; drags past 560 and 260 stop there; Hide leaves the rail, reports collapsed and disables the seam; the rail restores the last width; adopting the host's value applies it silently; an out-of-bounds value is not adopted; keyboard stops; the tour tool follows its action |
| `sqlite_workbench_settings_storage` | Pass, with the new per-window round trip, the 260 to 560 bound, Reset and the v21 migration; every older-version synthesis drops the two v22 columns |
| `application_host` | Pass: held until load, merged on orderly close, restart projection, bound refusal, Reset |
| `application_profile_window_layout_public_path` | Pass: window B hides Status from its Hide, restarts collapsed; window A stays open |
| `qt_workbench_window_scale_{100,125,200}` | Pass: no Start review button; the first Command-2 from an unstarted Overview selects the first unseen change after entering Review; a later Command-2 keeps the file; another pull request is fresh again and a picked file row wins |
| `qt_pull_request_overview_page`, `application_overview_summary_*`, `application_setup_destination_public_path`, `qt_composed_github_scale_*`, `qt_keyboard_shortcuts_scale_*` | Pass |
| Full suite and merge | As recorded on [[BPR-1587]] |

**Owner (pending).** Frozen build and attended session recorded below.

## Owner look, session v002 (prepared 2026-10-17, unanswered)

Frozen root `/Users/jaeger/bpr1588-status-drawer-v002.z9d119p7`, exported from
`e5182fd8e` (the milestone merged with `main`), `app-release` with tests on,
executable SHA-256 `54ae00a3de59b8b7c5b31956518336b55120653c7052d7e5828e8f6faaa4b5ed`;
its own `--check` reached `ATTENDED_PULL_REQUEST_OPENED`. Launch:
`python3 /Users/jaeger/bpr1588-status-drawer-v002.z9d119p7/launch.py`
(conventional-comments-v1, signed in, on the Overview). The v001 freeze at
`c17708272` was removed unanswered when the first-entry rule was narrowed to
unstarted reviews; its session `dc326e5e…` was stopped and released.

Session `e471b4b84fdd0009bd645ca916ffe64d`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261017-bpr1588-status-v002`,
ready at sequence 6:

| Candidate | Question | Kind |
|---|---|---|
| `BPR-1588-status-drawer-v002` (`9253b89d…`) | The Overview's side panel now behaves like the other drawers, and Start review is gone. | Accept or Reject |
| `BPR-1588-promote-label-v001` (`e5bce5f5…`) | Which label should turn a private note or an agent's finding into a local draft comment? | A To comment · B Draft comment · C Use as draft |

Next: read `status --after 6`; on Accept, finish BPR-1587 and BPR-1588 and
land the milestone on `main`; record the label in RFC-BPR-0085 for
[[BPR-1592]] and [[BPR-1594]].

## Owner verdicts, session v002 (hand-back 2026-10-17T20:31:28Z, event 9)

Read with `status --after 6`; both decisions `actor: human` with the prepared
fingerprints. The host (PID 93486) was stopped, the session released and the
frozen root removed.

| Candidate | Decision | Note |
|---|---|---|
| `BPR-1588-status-drawer-v002` (`9253b89d…`) | **Accept** (`6897d336`, 20:31:26Z) | — |
| `BPR-1588-promote-label-v001` (`e5bce5f5…`) | **B. Draft comment** (`bcb5d520`, 20:23:41Z) | — |

The label is recorded in RFC-BPR-0085 for [[BPR-1592]] and [[BPR-1594]].
