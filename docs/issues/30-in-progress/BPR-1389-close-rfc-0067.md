---
schema: "implement-rfc.card.v1"
id: "BPR-1389"
title: "Verify and close RFC-BPR-0067"
status: "In Progress"
rfcs: ["RFC-BPR-0067"]
closeout_for: ["RFC-BPR-0067"]
depends_on: ["BPR-0776","BPR-1379","BPR-1388"]
priority: "P1"
metadata: {"mandate_revision":"home-draft-r1","reservation":"docs/issues/_reservations/BPR-1389--BPR-1389--0238f04f813d.json","milestone_plan":{"id":"home-draft-v1","path":"docs/rfcs/plans/home-draft-v1/milestone-plan.json"}}
---

# Show observed draft pull requests on Home

RFC base: `2beb98cc9a58eeeeeebb8bdd08cc0af4a49c4e20`.

## Acceptance criteria


- **AC01:** On composed Home, an unopened draft PR has the leading chip and
  an otherwise comparable non-draft PR does not. Active sections and Recently
  viewed use the defined treatment, including a recent-only draft entry.
- **AC02:** Accepted draft-to-ready and ready-to-draft changes update the
  corresponding Home rows while retaining exact selection and opening
  targets. The annotation adds no provider calls or full PR loads and does
  not change attention, section membership or row actions.
- **AC03:** Restart and an unsuccessful refresh retain a previously observed
  draft chip. Older entries with no fact remain unknown. Late completions
  and changes of profile, account, host or access mode cannot supply another
  row's draft fact; queue and recent representations agree for the same
  accepted authority.
- **AC04:** Rendered Home keeps the complete chip and wrapped identity/title
  readable at narrow widths and 100–200% scale in the existing Light, Dark
  and high-contrast appearances, with Group and State headers settings,
  selection, Reviewing, account chips and opening states. Keyboard and
  accessible row output preserve the defined status and existing actions.

## Execution

One integrated milestone carries bounded accepted provider metadata through queue and Recent storage, adds a leading neutral chip and accessible wording, and verifies the composed public path. Human visual acceptance remains pending until the fresh attended candidate is judged. No live provider effects are needed.

## Closeout checklist

| Criterion | Check and result | Revision |
|---|---|---|
| **AC01:** On composed Home, an unopened draft PR has the leading chip and an otherwise comparable non-draft PR does not. Active sections and Recently viewed use the defined treatment, including a recent-only draft entry. | Pending implementation and focused acceptance | Pending |
| **AC02:** Accepted draft-to-ready and ready-to-draft changes update the corresponding Home rows while retaining exact selection and opening targets. The annotation adds no provider calls or full PR loads and does not change attention, section membership or row actions. | Pending implementation and focused acceptance | Pending |
| **AC03:** Restart and an unsuccessful refresh retain a previously observed draft chip. Older entries with no fact remain unknown. Late completions and changes of profile, account, host or access mode cannot supply another row's draft fact; queue and recent representations agree for the same accepted authority. | Pending implementation and focused acceptance | Pending |
| **AC04:** Rendered Home keeps the complete chip and wrapped identity/title readable at narrow widths and 100–200% scale in the existing Light, Dark and high-contrast appearances, with Group and State headers settings, selection, Reviewing, account chips and opening states. Keyboard and accessible row output preserve the defined status and existing actions. | Pending implementation and focused acceptance | Pending |
