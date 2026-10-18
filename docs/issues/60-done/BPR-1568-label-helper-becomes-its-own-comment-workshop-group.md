---
schema: "implement-rfc.card.v1"
id: "BPR-1568"
title: "The Label helper becomes its own Comment workshop group in Settings"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"owner-decision","found_by":"Owner decision via coordinator, 2026-10-16","governing_rfc":"RFC-BPR-0048","amendment":"RFC-BPR-0048 owner amendment 3 (2026-10-16)","related_cards":["BPR-1088","BPR-1090","BPR-1091","BPR-1565","BPR-1566"],"reservation":"docs/issues/_reservations/BPR-1568--BPR-1568--12e15d37af2c.json","acceptance":["BPR-1568:moved","BPR-1568:group","BPR-1568:kept","BPR-1568:gates","BPR-1568:owner"]}
acceptance: [{"criterion":"BPR-1568:moved","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"Ten Settings pages, per-group resets and all selected scale checks pass on the merged tree."}},{"criterion":"BPR-1568:group","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"Owner amendment3 supersedes Reviews & queue location with Comments/Comment workshop, accepted and verified by BPR-1597 focused tests."}},{"criterion":"BPR-1568:kept","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"No schema/store/key/src-app change; main schema23 and exact composition pins retained. Behaviour/tour courses pass."}},{"criterion":"BPR-1568:gates","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"All 28 focused tests pass; format22/docs/diff pass; unchanged tidy evidence reused. Owner handoff schedules single full suite and repository tidy at item9."}},{"criterion":"BPR-1568:owner","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"Closed into BPR-1597 by explicit owner handoff; final design accepted in cec0a456. Rejected v002 stays rejected; its criterion is superseded."}}]
---

# BPR-1568: The Label helper becomes its own Comment workshop group in Settings

## Owner decision

Owner, 2026-10-16, through the coordinator: move the "Label helper" setting
next to the Review feedback guidance it controls. Since [[BPR-1565]] its only
effect is whether a comment workshop request carries the Review feedback
guidance ([[BPR-1091]]).

v001 made it a switch in the writing hooks panel (Agents & context). The
owner rejected it in session 1086a4c4 (decision 01958fb1), verbatim: "It makes
no sense for that to be under agents & context. Just give it it's own section
for comments or something." Recorded as
[RFC-BPR-0048 owner amendment 3](../../rfcs/RFC-BPR-0048-feedback-authoring-native-github-integration.md).

## Design (v002)

- **Removed** from Settings › Reviews & queue › Comment labels: the Label
  helper row, and the helper from that section's Reset and its accessible
  description.
- **Added** under Comment labels, a group of its own, **Comment workshop**,
  in the same row grammar as the rest of the page:
  - **Feedback guidance**: a check box "Send with each workshop request" and
    an info glyph: "The Review feedback guidance tells the agent how comment
    labels and decorations work. On: each comment workshop request carries
    it. Off, the default: requests go without it. Labels count either way."
  - **Guidance text**: the link "Agents & context › Review feedback ›", which
    opens the writing instructions with the Review feedback hook chosen, as
    Mark colours opens the label tints.
  - its own Reset, which turns Feedback guidance off and changes nothing else.
- The writing hooks panel keeps no switch. Its Review feedback line says On
  or Off and names Reviews & queue › Comment workshop. Restore default there
  restores only the text, as before v001.
- **Kept.** The stored preference (`review_behaviour_settings::LabelHelper`,
  the `label_helper` column, default off, existing values), `bpr tour hooks`
  and its `label_helper` field, and the workshop behaviour
  (`workshop_feedback_supplement`).

## Acceptance

- **BPR-1568:moved** Comment labels has no Label helper row; its Reset and
  description leave the stored helper alone (negative control: no row on the
  page is named Label helper).
- **BPR-1568:group** Comment workshop sits under Comment labels with
  Feedback guidance and Guidance text; the switch follows the published
  record, writes the stored value through the behaviour dispatch, is
  reachable by keyboard with an accessible name and description; the group's
  Reset turns it off only; Guidance text opens Agents & context on Review
  feedback, whose line says On or Off and where that is decided; in the
  composed application turning it on and off changes whether a workshop press
  carries the guidance.
- **BPR-1568:kept** A stored `on` loads as on, the schema is unchanged, and
  `bpr tour hooks` still reports `label_helper`.
- **BPR-1568:gates** Focused rows, composition contracts, clang-format 22,
  clang-tidy and the documentation gate; one full dev ctest before landing.
- **BPR-1568:owner** The owner accepts v002 in Light and Dark.

## Evidence

v001 (a switch in the writing hooks panel) was rejected in session
1086a4c4343a1f2477fda538fcf9b519 (decision 01958fb1-6398-4e7e-b94f-8b1e9f01825d).

v002 (its own Comment workshop group on Reviews & queue, branch
`claude/bpr-1568-v002-comment-workshop`) was rejected in session
fbb0fdf53d02e07ffd855f7a3ea33fc2 (decision 97a50021-807b-4c5a-8196-064e97d12a63,
2026-10-17T16:12:46Z), verbatim: "I think reviews & queues is too much of a big
grab bag of settings and we should find a better organization." Its claims are
released and its frozen build removed.

Design round (coordinator, 2026-10-17): a Settings organisation pass first.
Three mockups of the real Settings window, in Light and Dark (A by task: Home &
queue, Reviewing, Comments; B by place: Home, Overview, Review, Comments; C two
pages: Home & queue, Review & comments), each placing the "Send with comment
workshop requests" switch, wait as one choice in attended-review session
0e5dbaff0d166bcc2c23a4c2c39ac937 (state
~/.mask/attended-review-sessions/bpr-1568-settings-ia-v001-20261017), candidate
BPR-1568-settings-organisation-v001. Stored settings and keys stay unchanged and
the BPR-1289 settings search is preserved.

The owner picked A, by task, in session 0e5dbaff0d166bcc2c23a4c2c39ac937
(decision 3229df02-e5e7-47ee-89fc-7b7982ce6c9e, no note), then named the middle
page Review. [[BPR-1597]] builds it with this card's switch folded in, on branch
`claude/bpr-1597-settings-by-task`; its final Accept or Reject waits in session
a090236dc78e408d1bd6fd6752c2dfbe. This card closes into BPR-1597 when it lands.

## Closed into BPR-1597 — 2026-10-17

The owner accepted BPR-1597's built Settings-by-task candidate (decision
`cec0a456-a98a-4d02-8eda-45507c5c5458`); RFC-BPR-0048 amendment 3 supersedes
this card's old location and v002 look criterion. Feedback guidance and its
independent Reset now live in Comments › Comment workshop. The stored value,
key and workshop semantics remain unchanged. The 28 focused tests on the
merged `c6c4eeff3` tree include Settings/search/reset, all workshop guidance
courses and both composition contracts; all pass. Format22 and documentation
pass; unchanged tidy evidence is retained. The owner's handoff places the
single full suite and repository tidy at queue item 9. The earlier rejected
candidates are not retroactively accepted. No independent work remains here.
