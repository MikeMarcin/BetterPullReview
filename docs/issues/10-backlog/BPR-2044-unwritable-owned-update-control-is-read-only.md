---
schema: "implement-rfc.card.v1"
id: "BPR-2044"
title: "Treat an update control folder the user owns but cannot write as a read-only install"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2043--BPR-2044--459846f19aa3.json","origin":"live-freeze diagnosis, 2026-11-10, attended session 243efdc0c300f68944464fc350c097ed; evidence in docs/verification/RFC-BPR-0099.md and the v0-5-0-prerelease-targets-v1 handoff (Live-freeze correction)"}
depends_on: []
---

# Treat an update control folder the user owns but cannot write as a read-only install

## Scope and acceptance

`release_update_installation::join`
(`src/adapters/update/src/macos_update_transaction.cpp`) returns
`reason::read_only` only when `initialize_control` fails. A control folder
that already exists, is owned by the running user and is private, but whose
mode no longer allows writing (as after a frozen build was sealed with
`chmod -R a-w` following its first launch), passes that check; opening
`admission.lock` for write then fails and the join reports `reason::busy`,
as if another instance held the lock.

Distinguish a lock that cannot be opened for write from a lock that another
process holds: the first is a read-only install (the update lifetime takes the
read-only path and the app starts), the second stays busy. A contract test
covers both with a sealed control folder and a held lock.

Not scheduled in any milestone; found during `v0-5-0-prerelease-targets`.
Related: [[BPR-2043]] (how a refused join is reported).
