---
schema: "implement-rfc.card.v1"
id: "BPR-2061"
title: "Draft all suggested comments from an agent reply with several"
status: "Done"
rfcs: ["RFC-BPR-0092"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2061--BPR-2061--37ecd03b9111.json","renumbered_from":"BPR-2058 (lane receipt BPR-2058--BPR-2059--bd1353aea511.json; main reserved BPR-2058 for another card meanwhile)","origin":"owner request, 2026-11-10","related_cards":["BPR-1753","BPR-1861","BPR-2059"]}
depends_on: []
acceptance: [{"criterion":"Scope 1-4","outcome":"pass","revision":"e4aeb679e","evidence":{"kind":"command","detail":"core_agent_draft_all; qt_workbench_window_scale_100/125/200 test_a_reply_drafts_all_its_suggested_comments; composed path --attended-agent-draft-all-check (Drafted 3, Skipped 1, Undo removed 3)"}},{"criterion":"Scope 5","outcome":"pass","revision":"52e085d57","evidence":{"kind":"command","detail":"Agent accent inventory rows in the focused set"}},{"criterion":"Scope 6","outcome":"pass","revision":"e4aeb679e","evidence":{"kind":"observation","detail":"Owner accepted BPR-2058-draft-all, session fbdd40b2768761881d2a420844cb4ec2, decision 1aad48ff-d4ed-4363-a26e-8d156bffc670"}}]
---

# Draft all suggested comments from an agent reply with several

## Origin

Renumbered from BPR-2058 on 2026-11-10: main reserved and finished
BPR-2058 (failure-first repair tests) after this lane's allocation and before
its receipt reached any branch. Commits before `38ef8e5e0` name this card
BPR-2058.


The owner, 2026-11-10: "Can we get a draft all suggested comments from an
agent response with several."

## Scope and acceptance

1. A reply whose finding cards carry two or more suggested comments offers
   **Draft all (N)**. N counts only the suggestions it would draft: not already
   drafted, not dismissed, not under way, and still placeable at the current
   head.
2. A press makes each a local draft at its file and lines exactly as the
   card's own **Add as draft comment** does. Nothing goes to GitHub.
3. Suggestions whose lines are no longer in the diff, or already drafted, are
   skipped; the reply says how many were drafted and skipped, and why.
4. One **Undo** removes the whole batch (a draft edited since is kept, and the
   reply says so).
5. Draft all starts no run: no Agent accent, and the accent inventory stays
   green.
6. Owner look on a frozen build whose agent reply holds several suggested
   comments: Accept or Reject Draft all.

Load frontend-design before the Qt work.

## Implementation (2026-11-10, `52e085d57`)

Frontend-design was loaded before the Qt work.

- `core::plan_draft_all`, `draft_all_offer`, `start_draft_all`,
  `settle_draft_all` (`src/core/include/bpr/core/agent_draft_all.hpp`): which
  suggestions a press drafts, in the cards' order, and why the rest are
  skipped. Pure; `core_agent_draft_all`.
- Agent tab (`agent_tab_view`): each turn has a Draft all row above its cards
  (a quiet button, the outcome sentence and Undo); it shows only with two or
  more suggestions and dispatches to the host. No Agent accent.
- Composition (`window_agent_setup.cpp`): `draft_all_agent_cards` drafts each
  card in turn through `act_on_agent_card`, the card's own path (opening another
  file first when needed), with a 20 s watchdog per card; a refusal because the
  file or line is not at the head counts as "no longer in the diff".
  `undo_draft_all` discards, in one write, each draft of the batch still at the
  revision it was made at and still a draft, and opens its card again; a draft
  edited since is kept. `agent_finding_placeable` says whether the file is in
  the pull request at the head and, for the file on screen, whether the line is.
- Attended agent look: `--attended-agent-draft-all-check` presses Draft all and
  Undo and prints what the reply says (verification only).

## Verification

- `core_agent_draft_all` (offer needs two suggestions; count is undrafted and
  placeable; reasons counted; outcome conserves every suggestion): pass.
- `qt_workbench_window_scale_100/125/200`
  (`test_a_reply_drafts_all_its_suggested_comments`: Draft all (3) with one
  gone and one without a suggestion; Draft all (2) after one card drafted; the
  request's cards and planned skips; "Drafted 1 private draft. Skipped 3: 1
  already drafted, 2 no longer in the diff."; Undo asks for the batch; "Removed
  1 draft." and the card open again): pass.
- Accent inventory and every other row of the focused set (29 rows, below under
  BPR-2059): pass.

### Composed path (`e4aeb679e`)

The real composition on the fixture pull request (bpr-fixtures/nested-folders#21,
`--attended-agent-look` with the suggestions fixture agent built as `codex`,
which finishes the agent setup, and `--attended-agent-draft-all-check`),
offscreen, dev build:

```text
ATTENDED_AGENT_SHOWN:findings=5
ATTENDED_DRAFT_ALL_OFFERED:Draft all (3):visible=1
ATTENDED_DRAFT_ALL_DONE:Drafted 3 private drafts. Skipped 1: 1 no longer in the diff.:cards=2,2,2,0,0:button=Draft all (0):agent_shown=1
ATTENDED_DRAFT_ALL_UNDONE:Removed 3 drafts.:cards=0,0,0,0,0:button=Draft all (3)
```

Its first run found two faults, fixed in `e4aeb679e`: the skipped suggestion
was offered again once drafting moved the screen to another file, and the
drawer was left on Comments, hiding the reply's sentence.

## Owner look (accepted)

Session `fbdd40b2768761881d2a420844cb4ec2`, candidate `BPR-2058-draft-all`
(its ID predates the renumbering), frozen root
`suggested-comments-v001.irz2s4en` from `e4aeb679e`: the owner accepted,
no note (decision `1aad48ff-d4ed-4363-a26e-8d156bffc670`, 2026-11-11T02:51Z).
Scope 6 passes. Host stopped and the root removed with
`freeze_attended_build.py remove` on 2026-11-10.
