---
schema: "implement-rfc.card.v1"
id: "BPR-2161"
title: "Local PR drafts call their notes Local comments"
status: "In Progress"
rfcs: ["RFC-BPR-0127"]
depends_on: ["BPR-2158"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json","milestone_plan":{"id":"comments-absorb-notes-v1","path":"docs/rfcs/plans/comments-absorb-notes-v1/milestone-plan.json"},"criteria":[],"origin":"imported 2026-11-13 from comments-absorb-notes-v1 (RFC-DRAFT-0001); cut from the draft's suggested milestones"}
---

# BPR-2161: Local PR drafts call their notes Local comments

Part of [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md); milestone `local-pr-draft-vocabulary` of the [plan](../../rfcs/plans/comments-absorb-notes-v1/MILESTONES.md).

## Scope

Owner answer Q8 (yes, vocabulary and card only, last): the local PR draft's notes (RFC-BPR-0108, 0110, 0111, 0113, 0115) are called Local comments with Keep local and Keep all local (N); their record and storage are unchanged; Fix and Check the fix are unchanged; nothing follows to the pull request after Create draft PR (Q12). Its evidence is a rendered local draft showing the new words, recorded on the closeout.

## Progress (2026-11-13)

Frontend-design was loaded first. Lane commit `ff4a39c43`.

Built (vocabulary and card only; the record and storage are unchanged):
- The draft's notes list, which the tray's Comments shows while the draft shows, is "Local comments". Each row wears the computer glyph the Comments drawer's Local comments wear. Its tooltip reads "Your local comment. Only on this computer." ("From the harness. Only on this computer." for the harness's). It says "Local comment, only on this computer" to assistive technology.
- The field reads "Add a local comment" (accessible name "New local comment"). The row menu reads "Delete Local Comment".
- Discard's sentence, the worktree notice and the Status tab's "Kept here" row say local comments.
- Keep local and Keep all local (N) belong to the local draft's finding cards, which are BPR-2069's and not built. That card now names them in these words.
- Nothing follows a local draft to its pull request (Q12). Fix and Check the fix are unchanged (BPR-2069).

Evidence: `application_local_draft_desktop_public_path` checks the list's name, the field's placeholder and name, a row's computer icon, tooltip and accessible description, and that the tray's Comments shows the list. The course renders `local-draft-local-comments.png` (in the build tree's `tests/end_to_end/local-draft-desktop/`, untracked): the Comments tray holds "src/search.cpp: Check the cancellation path." beside the computer, over "Add a local comment". `qt_local_draft_window`, `application_local_draft_{publication,tour}_desktop_public_path` were not changed.
