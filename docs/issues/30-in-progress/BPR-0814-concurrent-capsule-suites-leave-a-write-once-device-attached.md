---
schema: "implement-rfc.card.v1"
id: "BPR-0814"
title: "Concurrent capsule suites leave a write-once device attached"
status: "In Progress"
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

## Cause and fix (dsp-003)

**Cause: `hdiutil create` itself, not a shared name.** While several creates
run at once, `hdiutil create` sometimes exits 0 with its temporary write-once
device still attached, and the device stays attached (one was still there
after 9.5 minutes). The unified log for one leak shows every helper unmounting
its synthesized APFS container; the healthy helpers then ejected their
physical disks, while the leaking one ejected the container and left its
physical disk attached. The image bytes were final (same SHA-256 before and
after an ordinary detach). An attach of a leaked image fails at once with
empty stderr under `-quiet`, which is the stage-zero failure this card
recorded; stage zero had attached nothing, so it correctly cleaned nothing.
All copies share the volume name `BetterPullReviewCapsule`, but with six
workers the leak happened 8/120 times with the shared name and 4/120 with a
unique name, so the name is not needed for it.

Logs in `build/r2-logs/`: `dsp3-0814-probe-unfixed.log` (1 of 53 creates
leaked), `dsp3-0814-leftover-anatomy.log`, `dsp3-0814-leftover-unified-log.txt`,
`dsp3-0814-probe-volname-ab.log`, `dsp3-0814-leak-then-attach.log`, and the
unfixed suite, 3 copies x 5 rounds, where round 1 had 2 of 3 copies error with
`attach failed:` and leave a device (`dsp3-0814-unfixed-concurrent3.log`).

**Fix, test fixture only** (`tests/scripts/designated_live_macos_capsule_tests.py`).
`CapsuleImageTests` lists attachments of exactly its own image path from
`hdiutil info -plist`, detaches them without force (re-listing each attempt,
60 s bound), does so right after each create and again on every exit path of
the real-image cases, and the SIGTERM case now ends stage zero through its own
SIGTERM path if an assertion fails while it still holds the mount.
`scripts/designated_live_macos_capsule.py` is unchanged, so no digest pin
moved. Its production image build has the same exposure: [[BPR-0836]].

**Verification.**

- Three concurrent copies, 5 rounds: 15/15 OK, and after every round no
  device and no process from those runs; `hdiutil info` lists only the two
  system simulator images before and after (`dsp3-0814-fixed-concurrent3.log`).
- Serial suite 44/44 OK (`dsp3-0814-fixed-serial-plain.log`), and the
  registered row `designated_live_macos_capsule_contract` passes through
  ctest's network-isolation wrapper on main at `441f6256` plus this change
  (`dsp3-0814-ctest-registered.log`).
- Fault injection on scratch copies: a simulated leak is repaired, and a
  forced failure while stage zero held the mount fails as intended and leaves
  no device or process (`dsp3-0814-fault-injection.log`).
