---
schema: "implement-rfc.card.v1"
id: "BPR-1389"
title: "Verify and close RFC-BPR-0067"
status: "Parked"
rfcs: ["RFC-BPR-0067"]
closeout_for: ["RFC-BPR-0067"]
depends_on: ["BPR-0776","BPR-1379","BPR-1388"]
priority: "P1"
metadata: {"mandate_revision":"home-draft-r1","reservation":"docs/issues/_reservations/BPR-1389--BPR-1389--0238f04f813d.json","milestone_plan":{"id":"home-draft-v1","path":"docs/rfcs/plans/home-draft-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"b781fdf93c49e8f49ff51f13cff7f42fd1c1a321","evidence":{"kind":"observation","detail":"Pass: composed native/CLI Home discovery and recent-only accepted PR courses; draft/non-draft/unknown wire truth table; shared row status/accessibility. Product source equals frozen candidate f170654e651cb57e262d6c4e4696378c5464c365. Evidence: docs/verification/RFC-BPR-0067.md and docs/issues/_checkpoints/BPR-1389/."}},{"criterion":"AC02","outcome":"pass","revision":"b781fdf93c49e8f49ff51f13cff7f42fd1c1a321","evidence":{"kind":"observation","detail":"Pass: accepted draft-to-ready in both composed windows preserves exact selection/opening and the existing request budget; host transitions retain membership/attention. Product source equals frozen candidate f170654e651cb57e262d6c4e4696378c5464c365. Evidence: docs/verification/RFC-BPR-0067.md and docs/issues/_checkpoints/BPR-1389/."}},{"criterion":"AC03","outcome":"pass","revision":"b781fdf93c49e8f49ff51f13cff7f42fd1c1a321","evidence":{"kind":"observation","detail":"Pass: 266 core-only tests, exact numeric account masking, Recent destination isolation, null-preserving queue/Recent migrations, restart agreement, failed/late/foreign completions and migration rollback. Product source equals frozen candidate f170654e651cb57e262d6c4e4696378c5464c365. Evidence: docs/verification/RFC-BPR-0067.md and docs/issues/_checkpoints/BPR-1389/."}},{"criterion":"AC04","outcome":"pending","revision":"b781fdf93c49e8f49ff51f13cff7f42fd1c1a321","evidence":{"kind":"observation","detail":"Automated pass: four appearances and 100/125/150/200% shared rows, wrapping, selection, Reviewing/account chips, opening states and accessible wording; composed full-window captures. Pending: owner verdict in short attended session df622dfed34516817f93fa5ac168bf03. Product source equals frozen candidate f170654e651cb57e262d6c4e4696378c5464c365. Evidence: docs/verification/RFC-BPR-0067.md and docs/issues/_checkpoints/BPR-1389/."}}]
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
| **AC01:** On composed Home, an unopened draft PR has the leading chip and an otherwise comparable non-draft PR does not. Active sections and Recently viewed use the defined treatment, including a recent-only draft entry. | Pass: composed native/CLI Home discovery and recent-only accepted PR courses; draft/non-draft/unknown wire truth table; shared row status/accessibility. | `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` |
| **AC02:** Accepted draft-to-ready and ready-to-draft changes update the corresponding Home rows while retaining exact selection and opening targets. The annotation adds no provider calls or full PR loads and does not change attention, section membership or row actions. | Pass: accepted draft-to-ready in both composed windows preserves exact selection/opening and the existing request budget; host transitions retain membership/attention. | `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` |
| **AC03:** Restart and an unsuccessful refresh retain a previously observed draft chip. Older entries with no fact remain unknown. Late completions and changes of profile, account, host or access mode cannot supply another row's draft fact; queue and recent representations agree for the same accepted authority. | Pass: 266 core-only tests, exact numeric account masking, Recent destination isolation, null-preserving queue/Recent migrations, restart agreement, failed/late/foreign completions and migration rollback. | `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` |
| **AC04:** Rendered Home keeps the complete chip and wrapped identity/title readable at narrow widths and 100–200% scale in the existing Light, Dark and high-contrast appearances, with Group and State headers settings, selection, Reviewing, account chips and opening states. Keyboard and accessible row output preserve the defined status and existing actions. | Automated pass: four appearances and 100/125/150/200% shared rows, wrapping, selection, Reviewing/account chips, opening states and accessible wording; composed full-window captures. Pending: owner verdict in short attended session df622dfed34516817f93fa5ac168bf03. | `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` |

## Current handoff

Engineering is complete for milestone `home-draft-v1/draft-home`, mandate
`home-draft-r1`. Product candidate `f170654e651cb57e262d6c4e4696378c5464c365`; regression refinement
`b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` changes tests only and has no `src/` diff from that candidate.
The [verification ledger](../../verification/RFC-BPR-0067.md) reconciles failures,
corrections and actual evidence. AC04’s owner judgment remains pending; this card
must stay parked until the candidate receives its recorded verdict.

Fresh session `df622dfed34516817f93fa5ac168bf03`, cursor **3**; candidate
`bpr1389-home-draft-v001`, fingerprint
`3dc0d110224cab33969d5ad212d02853480beaa07aa8d4659150cf88b7c76c88`.
The [durable review handoff](../_checkpoints/BPR-1389/review-handoff.json) names
state, root, exact binary, coordinator-owned waiter and the external complete URL.
Launch: `python3 /Users/jaeger/bpr1389-home-draft-v001.nh7rl2k8/launch.py`.
Only the review server (PID 5647, loopback port 54587) is handed off. All build,
test and tidy commands finished. The source/build tree is frozen inside that root.
No live GitHub effect, push or owner-desktop automation occurred.

MASK controls upgraded separately during this segment to AgentSkills
`4bdf12d49f0572e23d5d877b9655d977a4381ee0`. Their unrelated edits are preserved;
the loaded coordinator/implementer snapshot and original strategy selection govern
this segment. No role/configuration reload or writer takeover occurred.
