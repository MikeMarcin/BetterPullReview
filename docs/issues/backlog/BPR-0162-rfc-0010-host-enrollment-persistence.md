---
id: BPR-0162
title: Host: enrollment persistence and the queue projection
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0158]
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
