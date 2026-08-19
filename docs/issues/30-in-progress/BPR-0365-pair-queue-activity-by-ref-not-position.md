---
id: BPR-0365
title: Pair queue activity observations by ref instead of position
status: In Progress
rfc: RFC-BPR-0010
depends_on: []
---

# Pair queue activity observations by ref instead of position

Found during BPR-0344's mechanism trace.
`queue_enrollment_register::project` accepts a
`std::span<const core::queue_activity_observation>` and pairs it to retained
records positionally: `aActivity[index]` against the register's private
insertion order. The observation type carries no `pull_request_ref`, so the
pairing is an implicit contract that every caller must reconstruct the
register's internal order exactly.

Production is correct today: the host's `activities()` assembles the span by
walking `Enrollments.records()` in the register's own order and looking each
ref up in its remembered activity list. Nothing is misattributed. What is
wrong is the shape: a future caller that assembles activity in enrollment,
completion, or store-load order would silently compute buckets for one pull
request from activity read about another, and no assertion would notice. A
mis-signed bucket is exactly the kind of quiet wrongness this board has had to
dig out before.

## Required final state

- The pairing is explicit: either the observation carries the ref it was read
  about and `project` matches by ref, or the register exposes a keyed
  interface that cannot be fed in the wrong order.
- A register-tier test feeds the same observations in a permuted order and
  proves the projection is identical, which is impossible to state under the
  positional contract.
- The host's `activities()` assembly and the Qt-facing publish path keep
  their current behavior; this is a contract hardening, not a semantic change.
- No new public API beyond what the reshaped pairing requires.

This card is deliberately independent follow-up from BPR-0344: that card's
ext4 divergence does not depend on this fragility (its course enrolls exactly
one pull request), and this repair must not wait for a Linux host.

## Verification

```sh
ctest --preset dev -R '^(application_review_queue_enrollment|application_queue_composition_contract)$' --output-on-failure
```
