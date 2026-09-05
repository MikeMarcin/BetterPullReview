---
schema: "implement-rfc.card.v1"
id: "BPR-0162"
title: "Host: enrollment retention and the queue projection"
status: "Done"
rfcs: ["RFC-BPR-0010"]
depends_on: ["BPR-0158"]
---

# Host: enrollment persistence and the queue projection

Observer enrollment persists locally until the pull request closes or the
reviewer removes it — even if the path that matched has since disappeared. Each
enrolled item keeps the rule that matched it.

## Required direct evidence

- An item enrolled by a path rule survives the path being deleted, keeping its
  original reason, and is still there after a restart.
- Closing, reopening, transferring, and losing permission each produce an
  explicit queue state. A pull request must never vanish silently; a reviewer
  who cannot see it any more is owed the reason.
- The store holds no source and no comment body.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`application::queue_enrollment_register` retains enrollments and projects the
queue from them. It holds no provider, no clock, and no database; the durable
store is a separate concern.

### The decision that carries this card

**Membership is retained as it was, not recomputed.** A reviewer watching
`src/parser` keeps the pull request when the last parser file leaves it,
because the reason they are watching has not stopped being true — it is why
they wanted to see the change in the first place. A queue that re-derived
membership on every refresh would drop exactly the pull requests a path rule
exists to catch, and would do it silently, at the moment the change became
interesting.

### Other decisions worth stating

**Re-enrolling keeps both reasons.** A rule match never overwrites a role, or
deleting the rule would silently drop a pull request GitHub assigned. A role
never overwrites the rule, or the reviewer loses the answer to why an
unfamiliar item is in their queue. A second rule match does not replace the
first: the reason recorded is the one that brought the item in.

**Forbidden and gone stay distinct.** One is a permission a reviewer may be
able to ask for; the other is a pull request that is not there any more.
Collapsing them would send somebody to the wrong person.

**Only a withdrawal moves an item out.** A deferred or refused refresh says
nothing about whether the pull request is still there, and treating it as a
departure would empty the queue during an outage.

**A departed item is retained but not projected.** Kept so the reviewer can be
told what happened; dropped from the queue because a closed pull request is not
asking for attention. Reopening restores the same record rather than adding a
second beside it.

**An unobserved item is quiet, not guessed at.** Inventing an observation would
put a pull request in a bucket on no evidence.

### Evidence

Nine tests. Five breaks, each landing on its own assertion:

| Break | First failure |
|---|---|
| re-enrolling replaces the membership | `and so is the rule that was already there` |
| forbidden and gone collapse | `a forbidden pull request is recorded as access lost` |
| departed items are still projected | `but the queue shows only what is present` |
| any disposition withdraws | `but the queue shows only what is present` |
| a missing observation is invented | `and asks for nothing until something has been read about it` |

### What this card does not do

Persist anything. The register is the retention *policy*; a durable store for
it is not built here, and the card's phrase "and is still there after a
restart" is therefore not claimed. RFC-BPR-0007's ledger already demonstrates
the store pattern this would follow, and nothing in the policy assumes one.
