---
schema: "implement-rfc.card.v1"
id: "BPR-1668"
title: "Mark a new Discuss card read when it opens"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner amendment, 2026-10-19: auto mark new as read when clicking a card with the new badge","reservation":"docs/issues/_reservations/BPR-1668--BPR-1668--30d95b0fbb41.json","milestone_plan":{"id":"bpr-1667-discuss-body","path":"docs/rfcs/plans/bpr-1667-discuss-body/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ffa429dd7af67de7ed2e5b0e5a2aedfeb8ba49f8","evidence":{"kind":"observation","detail":"Old Cards.choose fails the real application pointer course after24.10s: clicking New card never clears badge after released durable lane. New command captures exact identity/unread state before relayout and sends existing mark_read only on open for New feed cards. Production pointer and keyboard summary/inline cases pass; selection-only and passive refresh do not read unrelated cards."}},{"criterion":"AC2","outcome":"pass","revision":"ffa429dd7af67de7ed2e5b0e5a2aedfeb8ba49f8","evidence":{"kind":"observation","detail":"application_conversation_read_public_path passes5.62s in final integration and again after negative-control restoration. Real SQLite ledger effect lane is held: badge stays New before durable completion, clears after release, and survives fresh composition restart. Another summary and offscreen inline thread remain New. Keyboard Return and inline pointer use the same path; existing explicit read/unread, changed-summary, draft persistence and quoted-reply Send controls still pass."}},{"criterion":"AC3","outcome":"pass","revision":"ffa429dd7af67de7ed2e5b0e5a2aedfeb8ba49f8","evidence":{"kind":"observation","detail":"Final integration7/7, adjacent cards action sequence explicitly includes mark_read before Open in Review, strict touched-file format/tidy22 and both docs gates pass. No new test containment exception, skip, provider effect or production test hook. Both issue slices intentionally committed; all own commands quiescent."}}]
---

# Mark a new Discuss card read when it opens

Owner amendment to `/root/discuss_body_fix`, mandate
`discuss-body-blocker-r2-auto-read`: “While we're at it can we auto mark new as
read when we click on a card with the new badge”. Preserve BPR-1667's body and
empty-summary obligations. Use the existing semantic read command and durable
ledger; Qt performs no persistence or provider effects.

## Acceptance

- AC1: Clicking a New-badged Discuss card requests reading that exact card;
  keyboard activation follows the same opening path. Passive projection refresh
  and selection alone do not mark unrelated cards read.
- AC2: The ordinary application publishes the cleared badge only after the
  existing durable read path succeeds. Restart preserves the result; another
  unread summary, PR comment and inline thread stay new until activated.
- AC3: A deterministic old-code failure and passing focused Qt/public-composition
  regressions cover the behavior; coding/documentation checks pass and the
  combined milestone is committed and quiescent.

## Execution

Old production fails the real pointer-to-durable-read course after24.10s. The
repair captures the chosen identity and unread fact before relayout, then uses
the existing semantic Mark read command. The real application holds the ledger
lane to prove the badge waits for durability; refresh and restart preserve only
the clicked result. Keyboard summary and pointer inline cases pass, while an
unrelated offscreen thread stays new. Existing read/unread, edited-summary and
quoted-reply persistence/publication controls remain passing. See the shared
[verified handoff](../_checkpoints/BPR-1667/handoff.md).
