---
schema: "implement-rfc.card.v1"
id: "BPR-1389"
title: "Verify and close RFC-BPR-0067"
status: "Done"
rfcs: ["RFC-BPR-0067"]
closeout_for: ["RFC-BPR-0067"]
depends_on: ["BPR-0776","BPR-1379","BPR-1388"]
priority: "P1"
metadata: {"mandate_revision":"home-draft-r1","reservation":"docs/issues/_reservations/BPR-1389--BPR-1389--0238f04f813d.json","milestone_plan":{"id":"home-draft-v1","path":"docs/rfcs/plans/home-draft-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"b781fdf93c49e8f49ff51f13cff7f42fd1c1a321","evidence":{"kind":"observation","detail":"Pass: composed native/CLI Home discovery and recent-only accepted PR courses; draft/non-draft/unknown wire truth table; shared row status/accessibility. Product source equals frozen candidate f170654e651cb57e262d6c4e4696378c5464c365. Evidence: docs/verification/RFC-BPR-0067.md and docs/issues/_checkpoints/BPR-1389/."}},{"criterion":"AC02","outcome":"pass","revision":"b781fdf93c49e8f49ff51f13cff7f42fd1c1a321","evidence":{"kind":"observation","detail":"Pass: accepted draft-to-ready in both composed windows preserves exact selection/opening and the existing request budget; host transitions retain membership/attention. Product source equals frozen candidate f170654e651cb57e262d6c4e4696378c5464c365. Evidence: docs/verification/RFC-BPR-0067.md and docs/issues/_checkpoints/BPR-1389/."}},{"criterion":"AC03","outcome":"pass","revision":"b781fdf93c49e8f49ff51f13cff7f42fd1c1a321","evidence":{"kind":"observation","detail":"Pass: 266 core-only tests, exact numeric account masking, Recent destination isolation, null-preserving queue/Recent migrations, restart agreement, failed/late/foreign completions and migration rollback. Product source equals frozen candidate f170654e651cb57e262d6c4e4696378c5464c365. Evidence: docs/verification/RFC-BPR-0067.md and docs/issues/_checkpoints/BPR-1389/."}},{"criterion":"AC04","outcome":"pass","revision":"f170654e651cb57e262d6c4e4696378c5464c365","evidence":{"kind":"observation","detail":"Pass: automated appearance/scale, wrapping, keyboard, selection, Reviewing/account chips, opening states and accessible checks at b781fdf93; human ACCEPT of exact frozen bpr1389-home-draft-v001, session df622dfed34516817f93fa5ac168bf03, fingerprint 3dc0d110224cab33969d5ad212d02853480beaa07aa8d4659150cf88b7c76c88, decision 809ee3e4-ba73-49a3-941a-e0052c0c63cc at event 4; handed back with no pending at event 5. Product revision f170654e651cb57e262d6c4e4696378c5464c365; binary SHA-256 41f42b50631aa12a58ab3c974427f0a7c27a013670ca349901d6a9d978fed1d8. See docs/issues/_checkpoints/BPR-1389/human-acceptance.json and docs/verification/RFC-BPR-0067.md."}}]
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

One integrated milestone carries bounded accepted provider metadata through queue and Recent storage, adds a leading neutral chip and accessible wording, and verifies the composed public path. The fresh frozen attended candidate received human acceptance. No live provider effects are needed.

## Closeout checklist

| Criterion | Check and result | Revision |
|---|---|---|
| **AC01:** On composed Home, an unopened draft PR has the leading chip and an otherwise comparable non-draft PR does not. Active sections and Recently viewed use the defined treatment, including a recent-only draft entry. | Pass: composed native/CLI Home discovery and recent-only accepted PR courses; draft/non-draft/unknown wire truth table; shared row status/accessibility. | `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` |
| **AC02:** Accepted draft-to-ready and ready-to-draft changes update the corresponding Home rows while retaining exact selection and opening targets. The annotation adds no provider calls or full PR loads and does not change attention, section membership or row actions. | Pass: accepted draft-to-ready in both composed windows preserves exact selection/opening and the existing request budget; host transitions retain membership/attention. | `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` |
| **AC03:** Restart and an unsuccessful refresh retain a previously observed draft chip. Older entries with no fact remain unknown. Late completions and changes of profile, account, host or access mode cannot supply another row's draft fact; queue and recent representations agree for the same accepted authority. | Pass: 266 core-only tests, exact numeric account masking, Recent destination isolation, null-preserving queue/Recent migrations, restart agreement, failed/late/foreign completions and migration rollback. | `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` |
| **AC04:** Rendered Home keeps the complete chip and wrapped identity/title readable at narrow widths and 100–200% scale in the existing Light, Dark and high-contrast appearances, with Group and State headers settings, selection, Reviewing, account chips and opening states. Keyboard and accessible row output preserve the defined status and existing actions. | Automated pass: four appearances and 100/125/150/200% shared rows, wrapping, selection, Reviewing/account chips, opening states and accessible wording; composed full-window captures. Pass: human ACCEPT of exact frozen bpr1389-home-draft-v001 in session df622dfed34516817f93fa5ac168bf03, decision 809ee3e4-ba73-49a3-941a-e0052c0c63cc at event 4; handed back at event 5 with no pending. | `b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` |

## Accepted candidate and closeout

All four criteria are accepted for milestone `home-draft-v1/draft-home`, mandate
`home-draft-r1`. Product candidate `f170654e651cb57e262d6c4e4696378c5464c365`; regression refinement
`b781fdf93c49e8f49ff51f13cff7f42fd1c1a321` changes tests only and has no `src/` diff from that candidate.
The [verification ledger](../../verification/RFC-BPR-0067.md) reconciles failures,
corrections and actual evidence. The guarded finish records AC01–AC04 as passing.

The owner accepted `bpr1389-home-draft-v001` in short session
`df622dfed34516817f93fa5ac168bf03`, matching fingerprint
`3dc0d110224cab33969d5ad212d02853480beaa07aa8d4659150cf88b7c76c88`.
Decision `809ee3e4-ba73-49a3-941a-e0052c0c63cc` was saved by the human at
`2026-10-12T04:44:28.385297+00:00`, event **4**. Hand-back at event **5** left
no pending criterion or requested change. The [safe acceptance receipt](../_checkpoints/BPR-1389/human-acceptance.json)
and [review handoff](../_checkpoints/BPR-1389/review-handoff.json) preserve those
identities and the consumed cursor. The loopback host was stopped through the
attended-review tool; PID 5647 is absent and exec session 12891 is no longer registered.
The frozen source/build at `/Users/jaeger/bpr1389-home-draft-v001.nh7rl2k8`
remains protected and retained with the exact accepted binary. All task writers,
builds, tests, tidy and review commands are finished. No live GitHub effect,
push or owner-desktop automation occurred.

MASK controls upgraded separately during the earlier engineering segment to AgentSkills
`4bdf12d49f0572e23d5d877b9655d977a4381ee0`. The preserved
coordinator/implementer strategy validates under the current helper; this fresh
post-attendance owner adopts segment `implementer-2-closeout` without changing
mandate, plan, exits, product or verification inputs. Unrelated configuration,
authoring drafts, allocations and other agents’ work are preserved. Historical
RFC0032 A8 and release/native obligations keep their original scope.
