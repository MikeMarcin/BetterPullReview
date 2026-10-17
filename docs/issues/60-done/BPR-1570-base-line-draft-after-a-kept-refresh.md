---
schema: "implement-rfc.card.v1"
id: "BPR-1570"
title: "A base line takes its draft after a refresh that keeps the file"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"coordinator 2026-10-17: application_label_authoring_contract hangs at its 40 s budget in phase 31 (base-line=2 draft=-), 5/20 at e30c62f25","related_cards":["BPR-1212","BPR-1277","BPR-1560"],"reservation":"docs/issues/_reservations/BPR-1570--BPR-1570--810405762bed.json"}
acceptance: [{"criterion":"BPR-1570:root-cause","outcome":"pass","revision":"a9a92ea13","evidence":{"kind":"observation","detail":"Instrumented stress (6 parallel, load 20-37): failing runs show a BPR-1277 same-diff adoption under a new workbench authority (numbered 1->0, window left still current) before the base-line click; the click's question is answered at once by BPR-1560's named merge base, apply_base_numbering sees no placement change and presents nothing. Passing runs were rescued only by an unrelated private-draft selection callback re-presenting the selection."}},{"criterion":"BPR-1570:deterministic","outcome":"pass","revision":"aaa36647e","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_line_draft_contract$'","detail":"The course forces a kept refresh before the base line (BASE_LINE_DRAFT_EVIDENCE second digit: numbering kept). Variant --stale-base-numbering makes the kept answer one about another path with the left side placed. Negative controls: product fix reverted, plain and stale fail base-numbering-lost-on-kept-refresh (1000); only the present_review_draft change reverted, stale hangs at base-line=2 draft=- (the reported symptom)."}},{"criterion":"BPR-1570:stress","outcome":"pass","revision":"770f5c9fa","evidence":{"kind":"command","command":"label-authoring probe as application_label_authoring_contract runs it, 60 runs 6 parallel, load 76-86","detail":"60/60 consecutive passes (exit 0 and every evidence marker). The same method on the unfixed tree failed 2/24 and 2/30 at the base line. An intermediate 59/60 run exposed the owed-reread race at the course's end, fixed in 770f5c9fa."}}]
---

# BPR-1570: A base line takes its draft after a refresh that keeps the file

`application_label_authoring_contract` hung until its budget at the base-line
step ("base-line=2 draft=-") in about one run in four under load, on main
before [[BPR-1567]].

## Root cause

[[BPR-1212]] keys its base-numbering answer by a request that includes the
workbench authority. A refresh that keeps the file (here the conversations
refresh after the range's publication) adopts the same diff under a new
authority without publishing it again ([[BPR-1277]]), so nothing asked the
question again: `base_numbered_as_github()` turned false while the window's
left side and the source material stayed placed. A click on a base line then
asked again; since [[BPR-1560]] a left side that is the named merge base is
answered "same" at once, inside `present_review_draft`. `apply_base_numbering`
saw the placement unchanged, returned without presenting, and
`present_review_draft` returned too. The base line had no draft and the
Comment command said no draft was available. Before BPR-1560 the answer came
later from the epoch host and flipped the placement, which presented the draft.

Users could hit it: after any kept refresh, the first click on a base line
opened nothing until another line was chosen. The test passed only when an
unrelated private-draft selection callback happened to land after the click.

## Change

- A definite answer (`same` or `differs`) holds for any request about the same
  base, head, left commit and path; `unavailable` is asked again.
- The BPR-1277 adoption checks the numbering against the adopted request.
- `present_review_draft` goes on when its own question was answered at once.
- The line-draft course forces a kept refresh before the base line and records
  that the numbering held; `--stale-base-numbering` pins the synchronous path.
  `application_line_draft_contract` runs three variants (TIMEOUT 180, RUNS 3).
- The course's base-line phases also wait for the publication's owed reread
  of the pull request (`RereadOwed`), which starts only once a pending read
  settles. Without the wait, 1 run in 60 ended with that reread in flight and
  the fixture's recording unconsumed (`PUBLIC_PATH_FAILED`, label course
  itself passed).

## Not changed

A click made while the refresh is in flight (the workbench generation ahead of
the selected publication) gets no draft, and the tab presentation that follows
the adoption reselects the tab's remembered line. That is earlier BPR-1277
behaviour, seen in the traces and left alone here.
