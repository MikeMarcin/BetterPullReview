---
schema: "implement-rfc.card.v1"
id: "BPR-1328"
title: "Change arrows continue to the next file instead of greying out"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"found_by":"owner chat note, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1328--BPR-1328--8bce629ab070.json","related_cards":["BPR-1202","BPR-1287"],"acceptance":["BPR-1328:C1","BPR-1328:C2","BPR-1328:C3"]}
---

# BPR-1328: Change arrows continue to the next file instead of greying out

## Owner note

The owner, in chat on 2026-10-08:

> It doesn't feel good for next change arrows to get greyed out when there's only 1 change

## Behaviour before this card

The review bar carries two position groups, each a label and a pair of arrows.

- **File n of m, ‹‹ ››.** Previous file (`Alt+Up`) and Next file
  (`Alt+Down`) step through the filtered actionable order the workbench host
  publishes. With no file selected, Next file opens the first and Previous
  file the last. Under the production stop policy, Previous file is disabled
  on the first file ("This is the first actionable file.") and Next file on
  the last ("This is the last actionable file."); with an empty order both
  say "This pull request has no actionable files."
- **Changes n of m, ‹ ›.** Previous change (`Alt+PgUp`) and Next change
  (`Alt+PgDown`) step through the open comparison's semantic change groups.
  The core's `plan_change_navigation` decided them from the bounded change
  cursor alone: under stop, Previous change was disabled on the first change
  ("This is the first change in this comparison.") and Next change on the
  last ("This is the last change in this comparison."). A file with one change
  is both its first and its last change, so both arrows were grey there, which
  is the owner's note. With no comparison open both said "No file comparison
  is open."; with a comparison that has no changes, "This comparison has no
  changes to navigate."
- **Wrap.** `change_navigation_policy::wrap` makes Next file wrap to the first
  file and Next change wrap to the comparison's first change (and the reverse).
  Production always runs stop; only a fixture probe publishes wrap.

## Change

Like Visual Studio Code's next change across files, the change arrows now
continue past the ends of a comparison:

- At a file's last change, Next change moves to the **first change of the next
  actionable file**; at its first change, Previous change moves to the **last
  change of the previous file**. The destination is exactly the file Next or
  Previous file would choose.
- The arrows are disabled only where there is nowhere to go: the first change
  of the first file ("First change in the pull request.") and the last change
  of the last file ("Last change in the pull request."). A one-change file
  between two others keeps both arrows live.
- When a press would cross, the tooltip names the destination: "Continue to
  the next file: docs/guide.md", "Go back to the previous file: README.md".
- A file with no changes passes the arrows through to its neighbour.
- The wrap policy keeps wrapping inside the comparison, as before.

The core decides the target. `plan_change_navigation` returns a crossing (the
file decision the file command would make, plus whether to enter at the first
or last change) on the command projection, and the new
`cross_workbench_change_intent` decides it again in `transition`. The window
dispatches that intent when the projection offers a crossing and otherwise
sends the unchanged RFC-BPR-0004 diff intent. The host resolves the file
decision exactly as for Next file. The comparison surface is told the
destination file and the entry end; the next accepted comparison of that file
is entered through `navigate_change` from no selection, and an accepted
comparison of any other file drops the request.

## Acceptance

- **C1.** The core enables a change command at an end of the open comparison
  exactly when the matching file command has a neighbour, with a crossing that
  names that decision and the entry end; it refuses only at the ends of the
  pull request, with `at_first_change` or `at_last_change`. Tests fail before
  the change.
- **C2.** Through the real composition on `nested-folders-v1`, Next change
  walks from the first change of the first file to the last change of the
  last, crossing each file once; Previous change walks back, entering each
  file at its last change. Tooltips name each destination and each end.
- **C3.** Owner look, frozen build, one Accept/Reject: "change arrows continue
  to the next file".

## Execution record
