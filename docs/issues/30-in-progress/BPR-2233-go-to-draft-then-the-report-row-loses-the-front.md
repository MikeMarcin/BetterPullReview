---
schema: "implement-rfc.card.v1"
id: "BPR-2233"
title: "Opening the report right after Go to draft can lose the front to the file"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
---

# Opening the report right after Go to draft can lose the front to the file

## Origin

In `application_review_report_public_path`, pressing the report's row moments after Go to draft once in eight runs left the file in front: the file's own open was still settling and took the front back ([[BPR-2219]]). The course presses until it takes; the product race is not fixed.

## Scope and acceptance

- **AC-a.** The last of two front-taking gestures wins, whatever the order their work finishes in.
- **AC-b.** A course presses the report's row once, straight after Go to draft, and the report is in front.

## Cause

Go to draft chooses the file through the Files drawer; the host reflects the
workbench's selection into the active tab whenever the tabs are observed
(`window_navigation.cpp`). A report's tab shows no file, so while a report
was active every observation found the selection unshown and opened it: an
observation that came while the file's own open settled, after the report's
row was pressed, took the front back.

## Delivered

`ee65ed5a1`. `core::takes_selected_file` decides: a report's tab yields only
to a file choice still in force. Activating a tab (or opening the report
already active) retires a choice made before it; a refresh under a report
keeps a choice made after it. The host asks the core before reflecting. The
course presses the report's row once.

## Acceptance

| Criterion | Check | Result |
|---|---|---|
| AC-a | `core_document_tabs` (`test_the_later_of_a_file_choice_and_a_report_keeps_the_front`: the row pressed before and after the file is taken, a file chosen after the report, a refresh under the report, file tabs and no tab unchanged) | Pass at `ee65ed5a1` |
| AC-b | `application_review_report_public_path`: one press of the report's row straight after Go to draft, then three seconds of the file settling with the report checked in front throughout | Pass, 8 of 8 (`--repeat until-fail:8`); it failed about one run in eight before |
