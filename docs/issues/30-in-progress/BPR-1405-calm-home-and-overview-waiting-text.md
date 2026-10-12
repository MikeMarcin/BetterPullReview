---
schema: "implement-rfc.card.v1"
id: "BPR-1405"
title: "Calm the waiting and merge-requirement text on Home and Overview"
status: "In Progress"
rfcs: ["RFC-BPR-0069", "RFC-BPR-0070"]
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1405--BPR-1405--0e43de1c0fb9.json","origin":"owner request 2026-10-12"}
---

# Calm the waiting and merge-requirement text on Home and Overview

The owner, 2026-10-12: recent changes made the home screen and the overview
page "flood with text and generally less elegant, mostly related to required
reviews from code owners". The merge-gates pack (BPR-1394, BPR-1397) made each
fact a sentence and joined them all. A Home row for the three-team example
reads its reason, every team, a people/team count, "Required review
outstanding" and each failed check on one wrapping line. Overview's
**Merge requirements** stacks the merge assessment, review state, approval
minimum with its caveat, each missing and failed check, and a comma-joined
roster with team display names, as one paragraph above Participants.

## Acceptance

- AC1: A Home row's reason says who or what is awaited once. Named reviewers
  replace the generic "waits on reviewers" clause and the people/team count;
  the aggregate "Required review outstanding" appears only without names.
  Names and failed checks are bounded with "and N more", the complete roster
  being in Overview. The RFC-BPR-0070 example still reads "Waiting on
  code-owner review from …; Branch Freshness failed".
- AC2: Waiting on others' collapsed summary no longer repeats that its rows
  await review.
- AC3: Overview's merge section leads with a short, toned assessment and lists
  each unmet condition on its own short line. Evidence-limit caveats (approval
  minimum is not a remaining count, merge permission not established) stay in
  the details disclosure. Outstanding requests appear as a roster, one
  reviewer per line, code owners marked, not a comma-joined paragraph.
- AC4: RFC-BPR-0069 AC01–AC06 and RFC-BPR-0070 AC01–AC05 facts remain
  visible and accessible: every returned request, all three code-owner teams,
  the blocked assessment, failed and missing required checks, stale and
  unavailable states. Existing focused suites pass with updated wording.
- AC5: The owner accepts the calmer Home and Overview look (attended).
