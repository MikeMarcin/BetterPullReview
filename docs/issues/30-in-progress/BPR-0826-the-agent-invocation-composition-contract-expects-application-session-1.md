---
schema: "implement-rfc.card.v1"
id: "BPR-0826"
title: "The agent invocation composition contract expects application session 1 and sees 2"
status: "In Progress"
rfcs: ["RFC-BPR-0012"]
depends_on: []
priority: "P2"
platforms: ["mac", "win"]
metadata: {"found_by":"bpr-win under BPR-0622 (pre-existing), confirmed on macOS by the dsp-002 integration run","found_at":"2ba02ddbe59bf6e07302cbb7460f61cc9cad2627","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0826: The agent invocation composition contract expects application session 1 and sees 2

## Defect

`application_agent_invocation_composition_contract` fails the same way on
Windows (`bpr-win`, while working on BPR-0622) and on macOS at `2ba02ddbe59bf6e07302cbb7460f61cc9cad2627`
(`build/r2-logs/dsp2-int4-ctest.log`). The seed's exact-context evidence
matches (`agent-exact-context-evidence:1:1:1:1:1:1:1:1:163:163`), but the
context shows `application_session 2`.
`tests/end_to_end/agent_invocation_composition_public_path.cpp` (~1555)
requires `ApplicationSession == application_session_id{ 1U }`.

On Windows, putting BPR-0622's three app conditions back to `Q_OS_UNIX`
gives the same failure, so the failure predates the Windows slices. Every
other `application_agent_invocation_*` course passes on macOS.

## Scope

- Find the first bad revision on macOS and the mechanism. What now opens a
  second application session before the invocation? Candidates include
  [[BPR-0705]]'s multiple windows over one profile runtime, window
  restoration, and startup recovery on the effect lane.
- If the second session is intended, change the course to derive the
  expected session from the composition. Do not hard-code 2. Cite the
  decision. Otherwise fix the product.

## Acceptance

- The contract passes on macOS, and the cause is named here.
- The Windows station reruns it and records the result.

## Cause and fix (dsp-002)

**First bad revision: `826fb871`** ([[BPR-0705]]). Parent `061dfcdf` passes
(26.74 s); the child fails with `application_session 2`
(`build/r2-logs/dsp2-0826-{parent,child}-*.log`). BPR-0705 gave each window
its own application session (`WindowSession = Window + 1`, distinct from the
profile runtime's). `docs/architecture/system-architecture.md` records that
rule under "Profile and window ownership": a window session owns its epoch
and agent session. The compatibility startup opens window 1, so the context
carries session 2. The course's hard-coded `== 1` dates from [[BPR-0247]],
when there was one window. **The second session is intended, so the course
changed.**

Fix `d9ea1cbcec2dc8a3109fae91405f8d0c2b311198`. A new fixture accessor, `window_application_session`, returns
the session the composition gave a live window. The course requires exactly
one live window, the one it invoked from, and requires the context's session
to equal that window's. On failure it prints
`agent-exact-context-session:<context>:<composed>:<windows>:<invoking>`.
Controls: with the product rule shifted to `Window + 5`, the course still
passes; with an accessor that returns a foreign session, it fails
(`dsp2-0826-derivation-shift-*`, `dsp2-0826-negative-controls-*`).

**macOS at `6a58b6f870691c77f98ae84395f6958a9c72221a`.** The contract passes, repeated across
`dsp2-int7-ctest.log`, `dsp2-int8-ctest.log` and `dsp2-int9-courses.log`.
All seven other `application_agent_invocation_*` courses and both
crash-recovery courses pass (`dsp2-int7-ctest.log`). The Windows rerun goes
with the next `bpr-win` envelope, and the Linux rerun with the next
`bpr-linux` envelope: the station reported the same failure there.

**Status.** The macOS criterion is met. The card stays In Progress until the
Windows station records its rerun.

## Windows and Linux reruns; finished (dsp-003)

- **Windows.** `bpr-win` reran the contract at `55937770`, which contains
  `d9ea1cbc`. It passes in 26.85 s (seed and reopen pass,
  `AGENT_INVOCATION_COMPOSITION_CONTRACT_OK`). Main merged it as `013d2036`
  ([the Windows rerun](../../verification/BPR-0826-windows-rerun.md)).
- **Linux.** `bpr-linux` reran it at `780d0a1c`, and it passes in 10.44 s
  ("BPR-0826" in [the dsp-002 Linux confirmation](../../verification/dsp2-linux-confirmation.md)).
- **macOS.** It passes again at main `8a03a5be` (`build/r2-logs/dsp3-int1-ctest.log`).
