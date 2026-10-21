---
schema: "implement-rfc.card.v1"
id: "BPR-1707"
title: "A reread after publishing keeps the reviewer's chosen file, and a filter gesture during a refresh is kept"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: []
priority: "P3"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1703--BPR-1708--2cc864d62b8c.json","origin":"Observed while building RFC-BPR-0089 M4's composed and live courses (2026-10-21)","related_cards":["BPR-1705","BPR-1706","BPR-1713"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4c76e46e2fe5ce67e1da5a2b00463a218f590a4f","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_commit_filter_public_path$' --repeat until-fail:3","detail":"Fixture, shipped composition, offscreen (commit-filter-comment-v1). The course no longer chooses the file again after a reread. After each of the six publications' rereads (three with C3 hidden, two with only C3 shown, one on Whole PR) it checks that the workbench has src/notes.txt selected and its diff published and displayed, that the Files drawer has it current, and that the tab presentation has settled; the next comment's placement is decided at once. After the first and the Whole PR publication, choosing src/app.cpp and then src/notes.txt shows each at the first gesture. Cause, traced in the composition: during the refresh the filter had no comparison, so the filtered set lapsed to the whole pull request and came back (two more workbench observations that retired the selection), and a kept diff's refreshed publication did not ask again for its filtered line facts. Negative control: without the re-request the course fails at 'the host says where a comment on the line goes'. Three repeated runs pass. macOS."}},{"criterion":"AC2","outcome":"pass","revision":"4c76e46e2fe5ce67e1da5a2b00463a218f590a4f","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_commit_filter|application_commit_filter_public_path)$'; python3 tests/end_to_end/rfc0089_commit_filter_live_course.py --build build/dev --work DIR --prefix bpr1707-live --comment --cleanup","detail":"core::filter_comparison_in_force keeps the accepted comparison while a refresh resolves the same request again; a comparison chosen since is not in force until resolved; with none accepted there is nothing to filter; a Hide made against the comparison in force applies (core_commit_filter, also under core-only). Composed: Refresh, then, once the refresh is accepted and its comparison is pending, Hide C3 is made once: the chip reads '2 of 3 commits shown' at once, the Commits tab says nothing of a pause, the refresh lands, the chip still reads it and src/notes.txt's diff is the merge base to C1. Negative control: with the rule reduced to 'no comparison while resolving', core_commit_filter and the composed course fail there. Live on BPR-Test #122 (closed, threads resolved, branches deleted): the post-refresh Hide that M4's course repeated, and that #121 lost at its first attempt, is made once and applies; every placement, GitHub answer and marker matched; Viewed unchanged for 326 files."}}]
---

# BPR-1707: A reread after publishing keeps the reviewer's chosen file, and a filter gesture during a refresh is kept

Two observations from RFC-BPR-0089 M4's courses. Neither changes where a
comment goes; both make the reviewer repeat a gesture.

## Observed

- **The reread drops the chosen file.** After a comment is published the
  window reads the pull request again. The diff stays on screen, but the
  workbench then has no selected file, and choosing the same file again does
  nothing until another file is chosen first. It happens on Whole PR too. In
  the composed course the filtered file list could also be published once
  more after the reread, clearing a choice made in between (about one run in
  four before the course chose the file until it stayed chosen).
- **A filter gesture during a refresh is lost.** In the live course, Hide
  selected chosen within about fifteen seconds of Refresh left the chip
  hidden with its old text; the same gesture after the refresh landed
  applied. The course now repeats it.

## Cause

A refresh accepts the pull request again and then resolves the same
comparison. While that resolution was pending the window gave the commit
filter no comparison:

- The filtered file set lapsed to the whole pull request and came back. Each
  change made the workbench observe again and retire its selection after the
  kept tab had been presented again.
- A gesture made then was refused as `suspended`, and the Commits tab said
  the filter was paused for this comparison.

In the same window, a kept diff's refreshed publication was adopted under its
new request without being presented again, and without asking again for its
filtered line facts. The epoch host answers those only under the current
request, so a comment's placement waited on the old answer. Choosing another
file and this one again presented it afresh.

## Change

- **Core.** `filter_comparison_in_force` decides the comparison a commit
  filter applies to: the accepted one, and still the accepted one while a
  refresh resolves the same request again. A comparison the reviewer chose
  since applies once it is resolved, as before.
- **Composition.** The filter's comparison comes from that decision; the
  window records the request the accepted comparison was settled for. A kept
  diff's refreshed publication asks again for its filtered line facts and
  presents the placement.
- **Courses.** The composed course no longer chooses the file again after
  each reread; it checks the chosen file is still chosen (also on Whole PR)
  and makes a Hide during a refresh's comparison resolution once. The live
  course makes its post-refresh gesture once.

## Acceptance

- **AC1.** After a publication's reread the file the reviewer had chosen is
  still chosen, on Whole PR and under a filter, and choosing it again shows
  it.
- **AC2.** A commit-filter gesture made while a refresh of the same head is in
  flight applies once the refresh lands, or is refused and says why.

## Not covered

- A line chosen while the kept tab revalidates after a reread, a window of a
  few hundred milliseconds, is replaced by the tab's remembered line.
  [[BPR-1713]] records it; the composed course waits for the tab
  presentation to settle before it selects a line.
- After a force push the comparison in force is the old head's until the new
  one resolves, so a gesture then is refused as paused and says so.
- Windows and Linux are unmeasured.
