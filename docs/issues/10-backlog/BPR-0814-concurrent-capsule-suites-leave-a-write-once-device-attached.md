---
schema: "implement-rfc.card.v1"
id: "BPR-0814"
title: "Concurrent capsule suites leave a write-once device attached"
status: "Backlog"
rfcs: ["RFC-BPR-0016"]
depends_on: []
priority: "P3"
platforms: ["mac"]
metadata: {"found_by":"BPR-0692 concurrency experiment","found_at":"94f4e6dc","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0814: Concurrent capsule suites leave a write-once device attached

## Defect

While investigating [[BPR-0692]], several copies of
`tests/scripts/designated_live_macos_capsule_tests.py` were run at once. In
one copy, `hdiutil create` returned while its write-once device was still
attached. That copy's stage-zero attach then failed within 5 s and left the
device (`/dev/disk11`) attached. An ordinary detach removed it
(`build/r2-logs/bpr0692-concurrent-leftover-cleanup.log`). A single serial
suite does not show this.

## Scope

- Establish whether `hdiutil create` legitimately returns before its device
  detaches, or whether the fixture shares a path or name between copies.
- Make the fixture wait for or verify the detach it depends on, or isolate
  concurrent copies. On any failure path, stage zero or the fixture must not
  leave a device attached.

## Acceptance

- Three concurrent copies of the capsule suite pass and leave no attached test
  device in `hdiutil info`.
