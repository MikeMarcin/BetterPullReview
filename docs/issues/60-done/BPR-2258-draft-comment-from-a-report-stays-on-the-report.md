---
schema: "implement-rfc.card.v1"
id: "BPR-2258"
title: "Draft comment from a report stays on the report"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2258--BPR-2258--c3a513113ed6.json","origin":"owner, 2026-11-18"}
acceptance: [{"criterion":"AC-a","outcome":"pass","revision":"d22b5a9bc","evidence":{"kind":"command","detail":"application_review_report_public_path (composed, fixture agent): with src/engine/renderer.h on screen, Draft comment on the issue at src/engine/renderer.cpp:12-13 makes the draft at line 12 while the report stays shown and the active tab at every poll, and the strip reads the same tabs and files after; the report's Draft all likewise"}},{"criterion":"AC-b","outcome":"pass","revision":"d22b5a9bc","evidence":{"kind":"command","detail":"application_review_report_public_path: the finding reads Drafted and Go to draft; Go to draft shows the file"}},{"criterion":"AC-c","outcome":"pass","revision":"d22b5a9bc","evidence":{"kind":"command","detail":"application_review_report_public_path: focus stays within the report; the report announces \"Draft comment made on src/engine/renderer.cpp, lines 12 to 13. Go to draft opens it.\" (QAccessibleAnnouncementEvent, polite)"}},{"criterion":"AC-d","outcome":"pass","revision":"d22b5a9bc","evidence":{"kind":"command","detail":"application_review_report_public_path fails on the stay-on-report check with the host change stubbed out, and passes with it (three runs)"}}]
---

# Draft comment from a report stays on the report

## Origin

The owner, 2026-11-18: "Can we make drafting a comment from a report stop
switching to that file? It's disorienting id rather stay on the report". No
look is needed.

Draft comment on a report's finding ([[BPR-2219]]) went through the finding
card's path, `act_on_agent_card`, which opens the finding's file in Review,
chooses its line and opens the line's draft there. From a report that put the
file in front, and took the file tab nearest before the report for it.

## Scope and acceptance

- **AC-a.** Draft comment (and Keep local on a file) from a report makes the
  draft (or the Local comment) at the finding's file and lines while the
  report stays the active tab and on screen; the reviewer's file tabs and
  their files are as they were.
- **AC-b.** The finding then reads Drafted and offers Go to draft, which is
  the only thing that moves to the file.
- **AC-c.** Keyboard focus stays on the report, and a screen reader hears
  that the draft was made (or why not).
- **AC-d.** A composed course asserts the report stays shown, the tabs and
  the active tab do not change, and the draft exists.

## Built

- **Host** (`window_agent_setup.cpp`, `window_quick_ask.cpp`):
  `open_behind_report` opens the finding's file in a new tab (the core's
  Open in New Tab) and activates it while the window holds the report;
  `close_behind_report` activates the report's tab again and closes that
  tab, whatever the keep's outcome, with a 30-second backstop.
  `act_on_agent_card` waits for that tab's load instead of opening the file
  in Review. The report's Draft all does the same for each finding. The
  Agent tab's own finding cards are unchanged: from there the file still
  opens.
- **Qt** (`workbench_window`): `hold_report` keeps the report on screen and
  the strip as last drawn while held; released, the strip is drawn from
  the projection. (`agent_report_view`): focus that was on the report
  returns to it with the keyboard on Go to draft; the outcome is announced
  politely (`last_announcement`).
- Feedback is the existing Drafted state and Go to draft link
  ([[BPR-2219]]), which only navigates when followed.

## Verification

On the lane at `d22b5a9bc`, dev preset, macOS:
`application_review_report_public_path` (three runs; and failing with the
host change stubbed), `qt_agent_report_view`, `qt_document_tabs_scale_100`,
`application_ask_report_public_path`, `qt_agent_finding_cards_retained`,
`application_document_tabs_restoration_public_path`. Format check passes on
the changed files; clang-tidy reports nothing on the changed lines (its
findings in those files are on lines this card did not touch). Not run: the
whole suite.
