---
id: BPR-0334
title: Extract the shared agent protocol loop out of the Windows-only translation unit
status: In Progress
rfc: RFC-BPR-0011
depends_on: []
---

# Extract the shared agent protocol loop out of the Windows-only translation unit

The preserved Windows slice [[BPR-0240]] describes duplicated roughly 250 lines
of *portable* protocol logic into a translation unit that no toolchain here
compiles. `windows_agent_process.cpp:480-729` re-implements, independently, what
`agent_process_launcher.cpp:201-345` already does:

- driving `agent_protocol_decoder::push`;
- the App Server `appServerPhase` 0 → 1 → 2 follow-up gate;
- `build_agent_protocol_followup` and `acknowledge_followup` ordering;
- the publish and backpressure path;
- the retained-stderr ceiling arithmetic;
- the seven-outcome classification.

None of that is platform logic. Handle plumbing, Job Object assignment, and
suspended process creation are; a protocol state machine is not. There are now
two implementations of the same rules, exactly one of which is compiled or
tested on any host this project has, and they can drift silently because nothing
compares them.

## Required final state

- The protocol loop, follow-up gating, publish/backpressure path, stderr ceiling
  arithmetic, and outcome classification exist once, behind a narrow transport
  seam — write, read-available, wait, terminate.
- The POSIX and Windows backends both use it; neither keeps a private copy.
- The shared loop is tested here against a fake transport, so its behaviour is
  provable on this host rather than only on Windows.
- No behaviour changes on POSIX. This is extraction, not redesign.

## Non-goals

- Do not attempt to make `windows_agent_process.cpp` compile on macOS, and do
  not weaken the seam to allow it.
- Do not fold in the POSIX stdin-write asymmetry. Windows has a bounded,
  deadline-aware `write_all` that distinguishes cancellation from timeout; POSIX
  writes into Qt's unbounded buffer and can only report `malformed_frame`. That
  is a real difference worth deciding, and it is a separate card, not a rider on
  an extraction.

## Why this waited on BPR-0240, and why it no longer does

This card carried `depends_on: [BPR-0240]` with this reason:

> [[BPR-0240]] still owns `windows_agent_process.cpp` and has an unfinished
> slice in it. Extracting shared code from under an in-flight card would
> collide. This card starts when that one settles.

That is collision-avoidance, not a technical requirement, and the card's own
non-goals concede the point — "do not attempt to make `windows_agent_process.cpp`
compile on macOS". Every bullet of the required final state is reachable on this
host.

**Lifted by product-owner direction on 2026-08-20**, and the argument for it is
stronger than permission. The ~250 duplicated lines are *portable* logic that
already compiles cleanly on macOS in `agent_process_launcher.cpp`. Their copy was
written blind into a translation unit no toolchain has ever compiled, which makes
those lines the ones most likely to be wrong. Extracting them first means
[[BPR-0240]]'s Windows session compiles 250 fewer lines of never-compiled code,
and fixes one shared loop rather than two divergent copies.

So the order is not merely permissible, it is probably the better one.

**The residual risk, stated plainly:** if [[BPR-0240]]'s Windows session finds
compile errors needing source edits in `windows_agent_process.cpp`, those land on
a file this card has just restructured, and someone rebases. That is merge work,
not lost work, and it is smaller than the duplication it removes.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(application_agent_launch|application_agent_invocation_host|qt_agent_launch_canary|architecture_boundaries|architecture_boundary_contract)$'
```

Plus a new focused selector for the shared loop against its fake transport, and
`clang-format` on every file touched.

## The design, and why "extraction, not redesign" needs qualifying — 2026-08-20

Five of the six duplicated behaviours are faithful copies and move mechanically:
driving `agent_protocol_decoder::push`
(`agent_process_launcher.cpp:237-242` / `windows_agent_process.cpp:586-591`),
the App Server phase gate (`:250-260` / `:600-610`), the publish and
backpressure path (`:206-221` / `:531-546`), the stderr ceiling arithmetic
(`:223-231` / `:550-573`), and the follow-up build/acknowledge ordering
(`:261-278` / `:611-636`).

**The sixth is not a copy, and this card's own scope statement does not survive
it.**

### The two backends tear down in opposite orders

| | POSIX | Windows |
|---|---|---|
| final drain | `drain()` at `agent_process_launcher.cpp:307` | after termination, `windows_agent_process.cpp:678-685` |
| `decoder.finish()` + trailing gate | `:308-326` | `:699-707` |
| exit code | `:340` | `:662-667` |
| terminate | `end_process_tree` at `:343` | `terminate_and_reap` at `:668-677` |

POSIX drains, decides, then tears down. Windows tears down, *then* drains. A
shared loop cannot do both, so this card has to choose — and choosing is a
behaviour decision, not an extraction.

**The choice is made here, and it is POSIX's order**: drain to exhaustion,
`finish()` and classify, then `terminate()` exactly once at the end. The card's
own non-goal decides it — "No behaviour changes on POSIX" — and it is the only
option that satisfies that bullet by construction.

**The consequence must be stated rather than absorbed: this changes Windows
behaviour, on a platform no test here can exercise.** `terminate_and_reap` moves
after the final drain. [[BPR-0240]]'s Windows session is where that first
executes, and it should be told to expect it.

### A latent bug in the Windows copy, which extraction will fix in passing

`windows_agent_process.cpp:661` resets the input handle *before* the
post-termination drain at `:678-685`. If that drain decodes a control frame
requiring a follow-up write, `write_all` runs against a null handle, returns
`write_status::failed`, and `set_write_failure` (`:513-517`) maps it to a generic
`malformed_frame` — losing whatever the exchange should actually have reported.

POSIX has the same shape in theory (`closeWriteChannel()` at `:277`, one more
`drain()` at `:307`), but Windows' drain-to-exhaustion pass gives it strictly
more opportunity. Reordering to POSIX's sequence narrows the window rather than
entrenching it. **Reachability is unproven** — it needs a child emitting a
trailing App Server control frame at phase 2 as it exits — so this is recorded as
a candidate, not a fixed defect.

### The seam cannot move as far as it looks

`cmake/VerifyArchitectureBoundaries.cmake:90-96,1568-1575` restricts
`TerminateJobObject`, `QueryInformationJobObject`, `CancelSynchronousIo` and
their siblings to exactly two hardcoded paths. So the concrete Windows `write`
and `terminate` implementations **must stay inside
`windows_agent_process.cpp`**; only the loop moves. Splitting the transport
backend into a file beside the loop — the obvious refactor — fails
`architecture_boundaries`, and that gate is not in this card's verification list.

### What the fake transport must drive

No test covers the loop today. `qt_agent_launch_canary` is the only end-to-end
exercise and it runs the real `QProcess` transport on POSIX only. The fake needs
`write` returning each of the five `write_status` values including mid-follow-up
cancellation and timeout, `read_output`/`read_error` returning scripted partial,
oversized, and split frames with `pipe_read_status`, `wait` reporting scripted
exit transitions, and a `terminate` that records **that it was called once and
where in the sequence** — which is what pins the ordering decision above.

### Two seam decisions the card does not settle

- `read-available` either fills a caller buffer, which makes POSIX copy out of
  its `QByteArray`, or returns owned bytes, which makes Windows allocate where it
  does not today. Taking the buffer form, because it matches the Windows
  primitive and keeps the allocation behaviour of the platform that cannot be
  tested here unchanged.
- `forcedOutcome` has no POSIX slot today (POSIX inlines the checks at
  `:292,298`). The shared struct carries it universally; POSIX's `write()` must
  never return `cancelled` or `timeout`, or the non-goal's write asymmetry is
  reintroduced from the other direction.

## Claimed for the authorized shared relink — 2026-08-21

The current macOS session claims this card after the attended BPR-0256 v004
checkpoint failed closed before GUI creation and was frozen as non-mutating
evidence. The product owner's exact authorization remains:

> I authorize replacing the protected dev binary to implement BPR-0409,
> BPR-0410, and BPR-0334.

The predecessor protected development executable is 51,303,392 bytes with
SHA-256
`333e88c9298f5a57182b7b3dda9994974485ffc77f07fb7496ac9ee43f40204a`.
BPR-0256's approval was not consumed and was deleted after the refused lane;
its v004 packet must not be retried. This card's first `src/` edit is therefore
allowed to make that already-aborted producer stale.

Implementation remains bounded to the shared protocol loop, the two adapter
backends, focused fake-transport tests, build wiring, and directly affected
architecture/RFC/issue documentation. Native Windows process and Job Object
plumbing stays in `windows_agent_process.cpp`; no Windows success claim will be
made from macOS. POSIX behavior and its drain/finish/classify/terminate ordering
are the reference contract.
