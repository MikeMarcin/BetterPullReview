---
schema: "implement-rfc.card.v1"
id: "BPR-0166"
title: "Simplification pass for RFC-BPR-0010"
status: "Done"
rfcs: ["RFC-BPR-0010"]
depends_on: ["BPR-0158"]
---

# Simplification pass for RFC-BPR-0010

The behaviour-preserving pass this repository takes after each RFC's
implementation slices: remove what the delivered design made unnecessary, merge
what turned out to be one thing, and re-run every suite to show nothing moved.

## Required direct evidence

- Each removal or merge is named, with why the delivered design made it
  unnecessary.
- Every suite passes before and after, and the diff is shown to be
  behaviour-preserving rather than asserted to be.

## What was removed, and why

Three pieces of surface this RFC's slices added and nothing used. Each was
found by asking which declared name no caller reads, not by reading for style.

**`core::matches` was public and is now internal.** Its header comment said it
was "exposed because 'did this rule match' and 'is this pull request enrolled'
are different questions, and a reviewer editing rules needs the first". Nothing
outside its own translation unit calls it, and no rule editor exists. That
justification described a feature rather than a caller, which is what
speculative generality looks like when it is written down. `enrollment_for` is
the whole of the public policy.

**`review_landing_model_statistics` is gone entirely.** Nothing read it. I
added it by copying the shape of `review_queue_model` and `defect_card_view`,
which do have counters — but the landing model's suite observes Qt's own
signals through a recorder, and that is strictly better evidence: it sees what
a view sees rather than what the model says about itself. Keeping both would
mean maintaining a second account of the same events, and the weaker one is the
one that could drift without any test noticing.

**`queue_refresh_statistics` kept every field, and every field is now read.**
`Requested` and `Deferred` were incremented and never asserted. They are
genuinely informative — one says how many requests cost a full transfer, the
other how many answers were held back — so the fix was to assert them rather
than delete them. All five fields now have a test that reads them.

## Behaviour preservation

The removals are of declarations and of counter increments. No control flow
changed: `matches` has the same body in the same file, reached only through
`enrollment_for` as before; the landing model emits exactly the same Qt signals
with the bookkeeping removed.

Every RFC-BPR-0010 suite passes unchanged, and the two new assertions on
previously unread counters pass against behaviour nobody touched — which is
what makes them a check rather than a restatement.
