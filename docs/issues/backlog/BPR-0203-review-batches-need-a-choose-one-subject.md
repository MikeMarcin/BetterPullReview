---
id: BPR-0203
title: An A/B/C choice is not three pass/fail questions
status: Backlog
rfc: RFC-BPR-0006
depends_on: []
---

# An A/B/C choice is not three pass/fail questions

Product owner, 2026-08-07, on the `selection_shade` batch:

> It feels like these all ask mostly the same question, it also feels a lot more
> like multiple choice rather than pass/fail. Multiple choice would be fine in
> an acceptance review to me.

They are right, and the batch format forced it. The input schema already carries
`ab_group`, so the builder knows three subjects are alternatives — but the
verdict schema only has `accepted_subjects` and `rejected_subjects`, so a
one-of-three choice has to be expressed as one accept and two rejects. The
reviewer answers the same question three times and the record says two
candidates "failed" when they were merely not chosen.

That distinction is not cosmetic. `selection-shade-mid` is now recorded as
rejected. It was not rejected; it lost. If the accepted candidate later has to
be withdrawn, the record gives no way to tell the runner-up from a genuine
failure, and the next batch would re-litigate a decision that was already made.

## What it should be

A subject kind that presents *n* alternatives and takes one selection, so the
verdict records `chosen` and `not_chosen` rather than accept and reject.
Pass/fail stays for subjects that genuinely are pass/fail — the change-group
marking in the previous batch was one, and it was answered correctly by a
rejection.

The mechanical `audit_verdict` precondition still applies per alternative: a
candidate that fails its measurement should be presented as ineligible rather
than silently offered, which is what happened to candidate A here.

## Required direct evidence

- A batch with a choose-one subject builds, renders, and validates.
- A verdict that selects one alternative validates; one that selects two, or
  none, does not.
- The existing pass/fail path is unaffected — shown by validating a recorded
  historical verdict against its own manifest, not by inspection.

## Do not

Do not model this by keeping accept/reject and adding a convention that the
first acceptance wins. The schema would still permit a verdict that accepts two
alternatives, and a convention that the validator cannot check is not a rule.
