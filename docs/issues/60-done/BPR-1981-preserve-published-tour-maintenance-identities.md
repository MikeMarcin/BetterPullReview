---
schema: "implement-rfc.card.v1"
id: "BPR-1981"
title: "Preserve published repaired sources and mixed comment namespaces during maintenance"
status: "Done"
rfcs: ["RFC-BPR-0041", "RFC-BPR-0042"]
depends_on: ["BPR-1979"]
priority: "P2"
metadata: {"kind":"bug","related_cards":["BPR-1979"],"mandate_revision":"tour-destination-repairs-r1-maintenance-corrections","milestone_plan":{"id":"tour-maintenance-corrections-v1","path":"docs/rfcs/plans/tour-maintenance-corrections-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1981--BPR-1981--5ced895e4cc7.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b5644f4a98cb3850ce50722262dad396d87cd5e4","evidence":{"kind":"artifact","detail":"Final ten-stop composed repair course proves thirteen initial writes, zero-write second Preview, then one same-discussion explanation edit; source originals/mappings and remote identities survive. Core legacy/confirmed destination, cold-import and subsequent retarget controls pass.","path":"docs/verification/BPR-1981-tour-maintenance-corrections.md"}},{"criterion":"AC2","outcome":"pass","revision":"b5644f4a98cb3850ce50722262dad396d87cd5e4","evidence":{"kind":"artifact","detail":"Production session mixed Stop/lost-response/restart/maintenance uses issue IDs601/602/603 overlapping review IDs601/602; eight stop and two overview mismatch controls refuse before writes, conversation overview collision succeeds, and exact file composition passes.","path":"docs/verification/BPR-1981-tour-maintenance-corrections.md"}},{"criterion":"AC3","outcome":"pass","revision":"b5644f4a98cb3850ce50722262dad396d87cd5e4","evidence":{"kind":"artifact","detail":"Final nine-course dev integration and four-course core-only/boundary run pass; LLVM22 format8files/tidy7TUs, documentation gate and67 regression tests pass. Baseline failures and exact command evidence preserved.","path":"docs/verification/BPR-1981-tour-maintenance-corrections.md"}}]
---

# BPR-1981: Preserve published tour maintenance identities

The owner authorized correcting both P2 critical-review findings after BPR-1979
completed. BPR-1979's original acceptance and evidence remain historical. Baseline
`69f745a9914428046f030a88617dbe68b7f2ca99`.

- Repaired stops retain an author-selected mapping after initial publication;
  a second Preview/Publish mistakes them for pending retargets and verifies the
  obsolete original subject.
- Mixed maintenance combines review and conversation comments, then chooses by
  numeric ID alone. Valid overlapping IDs produce a false conflict.

## Acceptance

- AC1. Initial repaired publication retains the confirmed published source,
  supports no-op preview and actual subsequent text maintenance, and preserves
  source bytes, stable remote identities and ordinary retarget behavior.
- AC2. Production-session maintenance supports overlapping review/conversation
  numeric IDs in stops and overview lookup; wrong node, URL, actor, kind or source
  still refuses without writes. File subjects remain distinct from conversation.
- AC3. Meaningful permanent regressions and focused/static/documentation checks
  pass for the exact candidate; correction evidence and command state are durable.

## Correction acceptance

Source/test candidate `b5644f4a98cb3850ce50722262dad396d87cd5e4` passes all
three criteria. The [verification record](../../verification/BPR-1981-tour-maintenance-corrections.md)
binds both demonstrated baseline failures, permanent core/session/composed
regressions, exact namespace/source mismatch controls, the final nine-course
integration and core-only/boundary checks. LLVM22 format/tidy, documentation and
67 documentation regressions pass. The [checkpoint](../_checkpoints/BPR-1981/handoff.md)
records commits, command identities/results and the guarded closeout.

Both critical-review findings are corrected; no selected-scope decision remains.
No live remote write or owner desktop effect occurred. Original BPR-1979
acceptance and separate live/native qualification gaps remain historical.
