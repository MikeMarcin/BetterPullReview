---
id: BPR-0200
title: A persisted catalog survives a fixture identity change and no test notices
status: Backlog
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
