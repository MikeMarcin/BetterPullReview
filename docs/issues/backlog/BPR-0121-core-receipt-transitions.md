---
id: BPR-0121
title: Core receipt and acceptance transitions against material identity
status: Backlog
rfc: RFC-BPR-0007
depends_on: [BPR-0120]
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
