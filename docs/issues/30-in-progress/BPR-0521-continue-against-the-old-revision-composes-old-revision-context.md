---
id: BPR-0521
title: "Host: continuing against the old revision composes the old revision's context"
status: In Progress
rfc: RFC-BPR-0012
depends_on: [BPR-0517]
---

# Host: continuing against the old revision composes the old revision's context

## Why

RFC-BPR-0012's stale continuation `continue_against_old_revision` means the
reviewer keeps talking about the revision the session was bound to.
[[BPR-0514]] and [[BPR-0517]] implemented it in the session sense only: the
chat session host keeps the session's old binding, but the launch path in
`src/app/src/github_application.cpp` composes the frozen snapshot and the
supplement from the epoch host's *current* accepted revision, then admits a
session bound to the old one. The agent therefore reads new-revision
context under an old-revision session, which is the mismatch the
continuation exists to prevent. Recorded by both slices as not theirs to
change and confirmed by the [[BPR-0506]] scout.

## Required final state

- When the chosen continuation is `continue_against_old_revision`, the
  frozen snapshot and the supplement are composed from the session's bound
  revision, not the epoch host's current one; `start_new_session` keeps
  composing from the current revision.
- The binding the helper pins, the snapshot's binding record, and the
  session's binding agree in that turn; a disagreement is refused by name.
- `application_agent_chat_continued_contract` asserts the continued turn's
  snapshot binding equals the kept session binding and differs from the
  observed head; deleting the old-revision composition fails it by name.

## Non-goals

- The continuation choice and its presentation — [[BPR-0499]], [[BPR-0502]].
