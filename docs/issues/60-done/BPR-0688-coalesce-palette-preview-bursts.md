---
schema: "implement-rfc.card.v1"
id: "BPR-0688"
title: "Coalesce palette preview bursts"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0684"]
acceptance: [{"criterion":"AC-10/03/09","outcome":"pass","revision":"b04d7deb plus this finish","evidence":{"kind":"observation","detail":"Latest of three distinct candidates dispatches once per UI event burst; queued Cancel, Settings close, commit, import/adoption and reset invalidate pending work. Widget lifetime owns delivery. Focused panel passed 0.79s; real application theme course passed35.48s, proving one profile generation for three candidates plus close/reset invalidation."}},{"criterion":"Quality","outcome":"pass","revision":"b04d7deb plus this finish","evidence":{"kind":"observation","detail":"Worker canonical tidy passed all three changed TUs including final composition rerun; format/diff pass. Primary reviewed bounded diff. Updated reviewed composition digest pins; matching hostile-bypass test below. No additional review loop; other state review remains applicable."}}]
---

# Coalesce palette preview bursts

## Scope

Coalesce distinct palette preview candidates offered in one UI event burst so only the latest admitted preview is dispatched/applied. Cancel, close, import replacement and commit invalidate queued candidates correctly; no delayed preview may overwrite committed state. Preserve direct single-user interaction and existing palette authority.

Owned paths: Qt appearance panel and focused tests; composition theme probe and preview dispatch only if needed.
Routing: standard | Reason: default implementation (Astra low delegated route).
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

## Acceptance

AC-10/03/09: burst of three distinct candidates applies latest once; cancel/close/commit before delivery publishes no stale preview; existing preview/commit/cancel and actual theme application course remain passing.

## Evidence

Read-only BPR-0683 acceptance scout identified this concrete missing behavior.
Not yet implemented or checked. This bounded bridge is needed for Settings
acceptance; it does not activate unrelated browser tabs or claim full RFC-0026.
