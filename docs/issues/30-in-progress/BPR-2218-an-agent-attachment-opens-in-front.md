---
schema: "implement-rfc.card.v1"
id: "BPR-2218"
title: "Opening an agent attachment brings it to the front from any mode"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2218--BPR-2218--4840eee868c4.json","origin":"owner, 2026-11-16, about BPR-2166"}
---

# Opening an agent attachment brings it to the front from any mode

## Origin

The owner, 2026-11-16: "Opening an attachment from an agent in the overview
mode looks broken because it doesnt switch to the review tab to show it.
Opening that attachment should bring it to the front, like how opening a file
on overview switches to it."

The attachment is the report row under an agent's reply ([[BPR-2166]],
RFC-BPR-0128). Opening it adds or activates its tab in the files' strip, but
that strip and the report's place stand only in Review (and Author), so from
Overview, Discuss or Threads nothing visible happened.

## Scope and acceptance

- **AC-a.** Opening an agent attachment from a mode that does not show the
  document area (Overview, Discuss, Threads, or a review being composed)
  switches to Review and brings the attachment's tab to the front, the same
  way opening a file from Overview enters Review at that file.
- **AC-b.** From a mode already showing the document area (Review, Author),
  the attachment's tab just comes to the front; the mode does not change.
- **AC-c.** The open goes through the path a file open from Overview takes
  (enter Review first, then the tab); no parallel mechanism.

Load `frontend-design:frontend-design` before the Qt work and say so in the
report. The owner's look on a frozen build (Fixture agent only, a fresh short
session, one Accept/Reject) is filed, not waited on.

## Intended verification

`application_ask_menu_public_path_report` (composed, fixture agent): with a
file's tab in front, from Overview and from Discuss the report row's click
enters Review with the report's tab active and the report shown; from Review
the mode stays Review.
