---
id: BPR-0200
title: A persisted catalog survives a fixture identity change and no test notices
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0197]
---

# A persisted catalog survives a fixture identity change and no test notices

[[BPR-0197]] regenerated the `epoch-history/v1` git fixture, which changed the
commit identities inside it. Any `epoch-catalog.sqlite3` written by an earlier
build refers to commits that no longer exist.

The product owner's real application-support root, dated 2026-08-01, was in
exactly that state. It was moved aside to
`~/Library/Application Support/BetterPullReview.pre-bpr0197.bak` rather than
deleted — it is the owner's data and theirs to discard.

## Why the suite did not catch it

Every course in the suite builds its catalog under a temporary root and throws
it away. That is correct for isolation and it is why the tests are reliable,
but it means **no test has ever opened a catalog it did not itself create.**
The one configuration that matters to a real user — a database written by a
previous version of the application — is the one configuration never exercised.

This is not a fixture problem. The fixture change merely revealed it. The same
hole is open for any schema change, any identity-derivation change, and any
change to how epochs are keyed.

## What is not yet known

Whether the application handles this gracefully. Nobody has looked. The three
possibilities are all plausible and they are very different in severity:

1. It detects the mismatch and rebuilds — in which case this card is a test gap
   only.
2. It surfaces an error the user can act on — acceptable, wants wording review.
3. It presents stale or empty history as though it were correct — a silent
   wrong answer about the user's review state, which is the serious one.

`PUBLIC_PATH_EPOCH_CORE_STATUS:-1` with "Review history is inactive" was
observed against the owner's stale root, which is *consistent with* 2 or 3 but
does not distinguish them, and is confounded with [[BPR-0154]] appearing on the
same run. Do not treat that observation as evidence for any of the three.

## Traced, 2026-08-08 — possibility 3 is unlikely, and not yet excluded

The card offered three possibilities and called the third — stale history
presented as correct — the serious one. Reading the path end to end says the
product is built for the second.

A commit the cache cannot produce is a typed failure, not an absence:

```
git_cache_failure::object_mismatch      (or inventory_mismatch)
  -> core::epoch_failure::repair_required        epoch_application_host.cpp:184
  -> core::epoch_reason_code::repair_required
  -> "The local review history requires repair."  epoch_comparison_control.cpp:91
```

Schema drift has its own route to the same place: `validate_schema` checks
`application_id`, `user_version`, and a `catalog_meta` row carrying the schema
format and a non-zero incarnation, and any mismatch is
`failure::schema_mismatch`, which maps to `repair_required` too.

So the wording a reviewer would see already exists, and it is actionable rather
than silent.

### The trace holds at the adapter boundary too

`installed_git_cache` returns `object_mismatch` and `repair_required` as
`std::unexpected`, never as an empty success. A commit the cache cannot produce
is a failure, not an absence, so there is no route by which a missing object
becomes a quietly empty history.

### Correcting what this card said an hour earlier

An earlier revision of this section claimed the one observation on file
*disagreed* with the trace, because the owner's stale root reported "Review
history is inactive" rather than the repair wording. **That was a misreading and
it is withdrawn.**

`epoch_status::inactive` is the *initial* status — what the control shows before
any pull request has been accepted and before any epoch work has begun. The run
that produced it also reported `GITHUB_FIXTURE_FAILED`, so no pull request was
accepted and the epoch path never started. The observation is therefore about
[[BPR-0154]] and says nothing whatever about how a stale catalog is handled.

So there is no contradicting evidence. There is no evidence at all, which is a
different and less alarming position, and it is what the card originally said
before this section over-read a marker.

### Proven, 2026-08-08

`application_epoch_host` now asserts it. The fake cache gained a failure queue
for the **resolve** path — it previously had one only for prune and delete, which
is why nothing could answer this card: the failure a stale catalog causes happens
while preparing a revision, and that path could not be made to fail.

With `object_mismatch` injected there, a diagnostic carrying
`epoch_reason_code::repair_required` reaches presentation and **no comparison is
accepted**. So the third possibility — stale history presented as correct — is
excluded by assertion rather than by reading: there is nothing to present,
because nothing was accepted.

The neighbouring case is asserted too. A *cancelled* preparation produces no
repair diagnostic, so the repair wording means what it says rather than being
what that path always emits.

Shown non-vacuous by remapping `object_mismatch` to `unavailable`, which fails
the assertion with a clean build.

### What the storage layer does not do, and should not

Nothing in `catalog_database` detects this, and that is correct rather than a
gap. `validate_schema` checks the schema objects, the application id, and the
catalog incarnation; none of them moves when only the repository's objects do. A
catalog cannot know whether a commit still exists, and a storage layer that tried
would need a repository to consult — which is exactly the dependency it is built
not to have.

The guard belongs to the cache, and the cache has it.

### Superseded — what was unproven

Whether a stale catalog reaches the cache at all. Three levels are confirmed by
reading — the adapter returns a typed failure, the host maps it to
`repair_required`, the control words it as repair — and none of that demonstrates
that resolving an epoch pair against absent commits enters the first of them
rather than short-circuiting earlier.

### What remains is narrower than it was

The test this card asks for is unchanged, but its purpose is now sharper: not
"find out what happens" but "confirm the typed repair path is the one taken,
and that the reviewer is told rather than shown an empty history".

The design decision the card implied — rebuild, surface, or refuse — is largely
answered by the code that exists. The product surfaces. What is unproven is
whether it surfaces *here*.

## Required direct evidence

- A course that opens a catalog written against different commit identities and
  asserts the resulting behaviour, whatever it is decided to be. Constructing
  the stale catalog must not require the old fixture to still exist — write it
  from a recorded schema and row set.
- The assertion is shown to fail when the handling is neutered.
- If the answer is "rebuild", the rebuild is shown to happen once and not on
  every subsequent open.

## Do not

Do not make the fix "delete the catalog when anything looks wrong." A user's
review ledger is not disposable, and [[RFC-BPR-0007]] treats receipts as
durable. Whatever is discarded must be shown to be re-derivable.
