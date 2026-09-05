---
schema: "implement-rfc.card.v1"
id: "BPR-0121"
title: "Core receipt and acceptance transitions against material identity"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: ["BPR-0120"]
---

# Core receipt and acceptance transitions against material identity

The functional core owns what a receipt *means*: the transitions between
unread, seen, read, accepted, and excluded for a file, and the seen/accepted
cursor for a conversation. None of it may touch SQLite, Qt, the filesystem, a
clock, a thread, or a credential.

A file receipt attaches to `FileMaterialId`, not to a path and not to an epoch.
That is the whole point: a path is renamed and an epoch advances, while the
material either is or is not the same bytes. A conversation receipt records the
latest seen and accepted comment identity together with its edit version,
because an edit in place changes no identity and must still reopen unread.

Model the transitions as a table and replay them. A transition that can only be
exercised through a database is not a core transition.

## Required direct evidence

- Every state is reachable and every illegal transition is rejected with a
  typed reason rather than silently ignored.
- Accepting a file does not mark it read as a side effect, and reading does not
  accept: the two are independent facts about the same material.
- A conversation whose latest comment edit version advances reopens unread even
  though no comment identity changed.
- The core target scan shows no SQLite, Qt, filesystem, clock, thread, or
  credential identifier.
- Each assertion is shown to fail when the behaviour it constrains is neutered.

## Outcome

`src/core/include/bpr/core/review_ledger.hpp` and its translation unit hold the
whole of the file-receipt and conversation-cursor policy. The core reads no
storage, holds no ledger, and reaches nothing outside its arguments;
`core-only` builds and runs it with no Qt and no SQLite present at all, which
is a stronger statement than a dependency scan.

### Reading and accepting are separate facts

`file_read_state` is a three-value progression — `unread`, `seen`, `read` —
and acceptance and exclusion are deliberately outside it as independent
booleans. Modelling all five as one enumeration would have made "accepted"
imply a read state and forced a choice about what accepting an unread file
means. It means nothing: they are different claims about the same material.
The test drives all four combinations and asserts neither overwrites the other.

A weaker read claim never moves the progression backwards — marking a read file
seen is absorbed rather than treated as a regression — but it is *legal*, and
it reports `Changed = false` rather than an error. Only an explicit
`mark_unread` reopens.

### Material travels with the command

`file_receipt_command` carries the material it was issued against, so a command
formed while the reader was looking at one comparison cannot be applied to the
comparison that has since replaced it. Without that field a slow command would
silently mark the wrong bytes read, which is precisely the failure a receipt
exists to prevent.

Identity is checked before material so that a command aimed at another file
reports `unknown_file` rather than `material_mismatch`; the two are different
mistakes and are repaired differently.

### The edit version is what makes an edit visible

`conversation_receipt` stores an identity *and* an edit version for both the
seen and the accepted cursor. An in-place edit changes no comment identity, so
an identity-only cursor would report a conversation as read while its text had
been rewritten. The test asserts the edited observation carries the *same*
comment identity before asserting it reopens, so the case cannot pass by
accident.

### Non-vacuity

Three behaviours were broken separately, rebuilt, and observed to fail for
their own reason, then restored to green:

- `mark_read` also setting `Accepted` — "marking a file read does not accept
  it".
- `unread` comparing only comment identity — "an edit to the seen comment
  reopens the conversation unread".
- `Changed` hardcoded true — "marking a read file seen changes nothing and is
  not a change".

`core-only` 40/40.
