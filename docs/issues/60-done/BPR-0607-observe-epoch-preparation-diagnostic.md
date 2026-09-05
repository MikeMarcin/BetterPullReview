---
schema: "implement-rfc.card.v1"
id: "BPR-0607"
title: "Observe the epoch-preparation diagnostic on macOS"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0606"]
---

# Observe the epoch-preparation diagnostic on macOS

The [prepared diagnostic checkpoint](../../verification/reviews/epoch-preparation-diagnostic-v001/operator-packet.md)
binds the tests-disabled Release build at `986eb8cd` and one sign-in/PR-read
course as MikeMarcin. It supplies no publication authority. The owner requested
plain continuation instead of repeated digest statements; preserve the actual
instruction when the course is started, and record observed results truthfully.
No new live run or response is claimed by preparation.

## Required final state

- One exact-build run reaches a terminal sign-in and history result for
  MikeMarcin/BPR-Test#1 at the designated head, without publication.
- Record the first typed history failure or successful exact comparison from
  the new diagnostics and safe UI observation; no root-cause guess or raw logs.
- Explicit Sign out, quit and process absence are observed. Any deviation stays
  in the result and is not converted into a clean acceptance claim.
- A reproducible defect becomes a bounded repair before BPR-0256 prepares its
  next publication candidate; failure to observe remains a truthful blocker.

## Preparation evidence

BPR-0606's Dev/Release builds, five focused checks (49.33 seconds), three
integrator checks (5.94 seconds), scoped tidy/format and documentation gate pass.
The executable hash/length were read after build and match the packet. Reuse
these unchanged results; no full matrix, platform expansion or new code is
needed for this checkpoint. Existing ordinary Release output is the pending
consumer's candidate; no separate build tree or worktree was created.

## Owner resumed diagnostics — 2026-09-05

The owner explicitly said to go ahead without waiting for approvals for actions
already performed under earlier approvals. The actual instruction is retained
in the checkpoint response. The prepared hash revalidated and no app process
exists. Resume this bounded PR-read course and equivalent diagnostic follow-ups
without another digest ceremony; preserve actual build/outcome records and
explicit Sign out/quit. No publication grant is introduced.

## Observed result — 2026-09-05

The exact Release executable `171595c34d8c573d6855dda4e38c11334bde91b33e31791e5f1fd0a95781dd41`
ran as PID 50905 from 11:30:31 through 11:32:38 UTC. MikeMarcin sign-in and
all nine capability probes succeeded. The designated PR opened, but comparison
preparation failed. At 11:31:48.715717785 UTC the first typed failure was
`epoch_stage=protocol_acquire failure_domain=protocol failure=unsupported_version`.
Catalog, cache, pack sink and transport acquisition had succeeded. No file,
draft or publication operation followed. The exact head was not independently
observed in this run, so successful designated-head acceptance is not claimed.

The owner amended cleanup while the app was running:

> Are we still really debugging the authorization step? That feels to be working. Can't we just stop logging out?

> Seems like you should be able to diagnose things and iterate without me in the loop now on this.

The app quit without Sign out to preserve the remembered sign-in, as requested.
Shutdown and exit 0 were observed at 11:32:38 UTC and process absence confirmed.
This supersedes this card's earlier Sign out procedure for diagnostic iterations;
it is not a claim that Sign out occurred. Equivalent rebuild/reopen/read/log
iterations continue under standing authorization with no new approval ceremony.

Only appended sanitized diagnostics were inspected; no raw logs or screenshots
are committed. A bounded anonymous discovery request to the designated public
repository returned HTTP 200 with the upload-pack advertisement content type:
service pkt-line, flush, then `version 2`. The parser rejected that HTTP preamble.
[[BPR-0608]] owns the contained framing repair and regression evidence. This
card is complete as diagnostic observation, with the head-observation limitation
retained; no RFC closeout or publication acceptance is claimed.
