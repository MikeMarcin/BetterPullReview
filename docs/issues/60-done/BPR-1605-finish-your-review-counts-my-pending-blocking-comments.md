---
schema: "implement-rfc.card.v1"
id: "BPR-1605"
title: "Finish your review counts my pending blocking comments"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-18","owner_note":"The heading's blocking-feedback chip showed 0 while my own pending blocking comment existed. Design how it should count; it must stay consistent with the Approve warning dot.","governing_rfc":"RFC-BPR-0048","related_cards":["BPR-1085","BPR-1345"],"reservation":"docs/issues/_reservations/BPR-1604--BPR-1605--1993453ea270.json","requested_skill":"frontend-design","milestone_plan":{"id":"bpr-polish-contrast-blocking","path":"docs/rfcs/plans/bpr-polish-contrast-blocking/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d06e7400e4eb470490ee630ecf76b0b9308cfdc0","evidence":{"kind":"command","detail":"application_label_publication_recovery passes including carried-blocking-preflight through the actual application/workflow/editor: selected+carried draft counts once, carried-only after navigation remains1, adding a published blocker makes2 with1 not sent. Editor tests cover pending-only1, mixed2, exact zero and Approve-dot agreement."}},{"criterion":"AC2","outcome":"pass","revision":"d06e7400e4eb470490ee630ecf76b0b9308cfdc0","evidence":{"kind":"command","detail":"application_feedback_counters verifies a root in a pending GitHub review is pending_publication, ordinary threads stay published and publication changes the root to published. The real-app course verifies selected/carried deduplication."}},{"criterion":"AC3","outcome":"pass","revision":"d06e7400e4eb470490ee630ecf76b0b9308cfdc0","evidence":{"kind":"observation","detail":"Owner ACCEPT of BPR-1605-blocking-total-v001, fingerprint 455f12cf1399826541ef4e85b920e3525e0c4b68bd7bf2320b0fde3c59077014, decision d9fdb9b1-3fd8-4bc0-9f84-cc5bdf2129ee, event 10; session 3b007b57637c66122767174fe1a527be handed back at event 11 with no pending decisions or notes. Exact frozen product and executable remain as recorded. Receipt: docs/issues/_checkpoints/BPR-1605/human-acceptance.json."}}]
---

# BPR-1605: Finish your review counts my pending blocking comments

The owner reported on 2026-10-18 that the blocking-feedback chip in Finish
your review's heading showed "0" while their own pending blocking comment
existed.

Cause, read from the code: the review preflight ([[BPR-1085]]) reads two
kinds of my feedback, my published conversations ("Yours on GitHub") and my
unsent drafts ("Yours, not sent"). The composition gives it only the one
comment still being written in the composer. The comments the review
already carries, the rows of Finish your review ([[BPR-1345]]), never reach
it, so a blocking one counts nowhere: the chip says 0 and Approve has no dot.
When a later read returns that pending comment's thread, it counted as
published ("Yours on GitHub"), which it is not.

## Design

The chip shows one number, the same one the Approve dot speaks for: all of my
open blocking feedback, published or not (`core::open_blocking`). When part
of it is not sent yet, a quiet "(not sent)" or "(1 not sent)" follows. The
tooltip and the menu keep the split by group, as before.

Rejected: two numbers side by side (published, then a pencil and the
unsent count). That is today's layout. It leads with a published-only count,
so "0" is the first thing read while something blocks, and the Approve dot
counts a different total from the chip's number.

- The review's carried comments are read as my unsent drafts, beside the
  composer's draft, each once.
- A review thread whose root comment belongs to my pending GitHub review is
  unpublished (`pending_publication`), as the pending review's body already
  is, so the same comment is never counted as on GitHub too.

## Acceptance

- AC1: With one pending blocking comment carried and none on GitHub, the
  preflight counts it as mine, not sent; the chip reads 1 with "(not sent)";
  Approve shows its dot and says 1. With one on GitHub as well, the chip reads
  2 with "(1 not sent)" and Approve says 2. With nothing open the chip reads 0
  and Approve has no dot. Proven through the review submission workflow and
  the editor, not a view model alone.
- AC2: A pending review's thread is excluded from the published population;
  published threads are unchanged; the composer's draft and a carried comment
  for the same draft count once.
- AC3 (human): The owner accepts the frozen candidate's chip, before and
  after, in Light and Dark.
