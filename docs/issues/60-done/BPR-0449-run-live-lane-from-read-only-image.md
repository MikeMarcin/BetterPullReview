---
schema: "implement-rfc.card.v1"
id: "BPR-0449"
title: "Run the designated live lane from the read-only image"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0448"]
---

# Run the designated live lane from the read-only image

BPR-0256's v005 capability-critical review falsified the runtime half of
BPR-0353's read-only topology. `stage-zero` mounts the digest-bound image,
verifies it, and detaches immediately. It never starts stage one. The opt-in
CTest registration instead launches the writable plain-directory supervisor,
so the read-only mount protects neither the supervisor/application lifetime nor
lazy nested-resource loads after `SIGCONT`.

The v005 packet disclosed the plain-directory launch, but packet prose cannot
waive BPR-0353's required runtime property or BPR-0354's explicit reliance on
the mount after resume.

## Required final state

- The exact digest-bound image is attached read-only beneath a fresh
  owner-private mount root, and the registered live lane launches stage one
  only from that mount.
- The mounted inventory, verifier, supervisor, application, and nested runtime
  resources remain under that same mount until the complete child lane exits.
- Writable mount, image digest drift, mount identity drift, early detach, or a
  detach that would require force refuses the lane.
- The opt-in manifest no longer names a supervisor or inventory from the plain
  build-tree capsule. Its sole command is the exact bounded stage-zero runtime
  topology, and the default graph still contains zero live entries.
- Mocked and real-image contracts prove mount → verify → supervisor/app run →
  ordinary detach ordering, including failure cleanup without force.

## Non-goals

- No live GitHub call, credential access, approval materialization, or remote
  mutation.
- No claim that DMG generation is byte-reproducible. A successor binds the
  exact stored image it reviews.
- No v006 operator packet until this card and BPR-0448 are complete and the
  entire tests-disabled Release capsule is regenerated.

## Claim — 2026-08-24

This macOS session owns the image-to-runtime topology after BPR-0448 closed at
`20e65ac`. It may change the stage-zero launcher, designated-live manifest
registration, production-shaped image/runtime tests, and their exact
architecture/source locks. It does not own a live GitHub request, credential,
approval materialization, attended launch, remote mutation, or v006 packet
generation until this card is independently verified and closed.

## Implemented and verified — 2026-08-24

Commits `b82267e` and `20c6472` connect the sole opt-in live command to the
read-only mounted image for the complete stage-one lifetime. The command starts
the fixed `/usr/bin/python3 -I -S` interpreter with a bounded preloader. That
preloader captures the source launcher once, matches it to the literal digest
from the producer's captured source manifest, and executes the captured bytes.
Configuration reads the bounded producer manifest once and uses that same byte
sequence for its directory identity, launcher identity, and source entries.

Stage zero retains the exact image descriptor, attaches its literal-digest DMG
read-only below a fresh owner-private mount root, verifies the mounted
inventory and runtime, and launches the supervisor/application from that mount.
The image, mount, inventory, verifier, supervisor, application, and nested
resources remain under observation until stage one exits. Stage one has a
1,500-second process-group bound inside CTest's 1,800-second timeout. Normal
exit, failure, timeout, and handled termination signals all pass through
post-child image/runtime revalidation, bounded group TERM/KILL, complete reap,
and ordinary non-force detach.

The primary fresh offline tests-disabled Release artifact binds:

- source manifest
  `4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc`;
- stage-zero launcher
  `b969b90a26b87d862e04c5c2064636ffd875ef3361ff392cfa5f4f8df998da05`;
  and
- exact stored DMG
  `f5c823b63b60b601be825cefc249d453eab7ad73ea7773f91ea2795df34a3301`.

That artifact passed the generated mounted probe and a separate direct
fixed-interpreter probe with literal launcher, source, and image digests. The
final hostile suites passed 44/44 capsule tests, 37/37 isolation tests, and
126/126 architecture tests. The five registered isolation contracts and four
BPR-0448 compatibility paths passed together 9/9. Formatting, direct
architecture verification, Python compilation, and `git diff --check` passed.

The simplification pass removed a redundant manifest metadata reopen, made the
image freshness claim match the check it performs, and retained the explicit
signal/process-group sequence for auditability. No attended launch, approval,
credential, public observation, network request, GitHub interaction, or remote
mutation was performed.

## Independent capability-critical review — 2026-08-24

The first review of `495de5b..b82267e` rejected two false-green boundaries. It
demonstrated an A-to-B source-manifest swap between separate hash and parse
opens, and a real blocked stage one whose externally terminated stage zero left
the DMG attached. The repair captures one manifest buffer and owns stage one as
a bounded process group. Deterministic tests now swap manifest and launcher
immediately after capture, mutate a launcher in place after capture, exercise
cooperative TERM and forced KILL, interrupt the attach window, and terminate a
real mounted-image course. All refuse or unwind without executing substituted
bytes, leaving a child group, or leaving an attachment.

A fresh critical reviewer returned **CLEAN** for exact range
`495de5b..20c6472`: no blocker, no supported code/runtime gate failure, and no
unsupported suspicion. Its independent fresh Release image differed from the
primary stored DMG, as the declared non-reproducible image format permits; each
image matched its own digest binding and mounted probe.

The review retained three explicit limits. The mounted filesystem is
read-only, but the invoking owner can still write the backing DMG during child
execution. Post-child identity/digest checks detect persistent drift but cannot
prevent transient backing-byte influence. `hdiutil attach` remains
pathname-based rather than descriptor-atomic. The generated CTest manifest and
cache remain the trusted same-UID execution plan. BPR-0256 must bind one exact
stored successor image and carry these limits into v006.
