---
schema: "implement-rfc.card.v1"
id: "BPR-1254"
title: "The staleness contract once refused to start its agent with agent_session_catalog_unavailable, in a long serial run on macOS"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"bpr-main-mac's integration pass for BPR-1121, BPR-1128 and BPR-1139, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1254--BPR-1254--30728f44971c.json","related_cards":["BPR-1211","BPR-1245","BPR-1139"]}
---

# BPR-1254: The staleness contract once refused to start its agent with agent_session_catalog_unavailable, in a long serial run on macOS

## Observation

macOS 26 on Apple Silicon, Apple clang, Qt 6.11.2, `build/dev` Debug at
`4aa6f544d`. A serial run of 103 rows (`ctest --preset dev -j1`, the union
of [[BPR-1121]]'s and [[BPR-1139]]'s selections), from 15:24:56Z to
16:04:10Z, failed `application_agent_invocation_staleness_contract` after
64.62 s:

```
agent-invocation-composition-failed:typed-terminal-cleanup agent-terminal-status=Not started: agent_session_catalog_unavailable agent-terminal-expected=Finished: stale_result agent-terminal-output-bytes=0 agent-terminal-cleanups=1 agent-terminal-quiescent=true reached=status-is-expected-terminal cleaned=
```

Run alone straight afterwards, twice (`--repeat until-fail:2`), the row
passed in 22.11 and 21.31 s. So it is intermittent, and was seen once.

The refusal is the application's, not the course's. The agent never
started: `agent_invocation_host::publish_session_catalog`
(`src/application/src/agent_invocation_host.cpp`, about line 258) refuses
the turn as `agent_session_catalog_<failure>` when the review ledger answers
the catalog read with a failure. The window's port
(`src/app/src/github_application.cpp`, `session_catalog`, about line 12380)
answers `ledger_store_failure::unavailable` at once when the window is
closing or its `ReviewLedgerHost` is not there yet. The ledger host can
also answer it. Which of these happened is not known. [[BPR-1211]] moved the
catalog read off the decision lane.

A user could meet the same refusal: "Not started" for a turn that would have
run a moment later.

## Scope

- Find which path answered `unavailable`, and whether a catalog read can
  arrive before the window's ledger is open, or race its close.
- If it can, the read waits for the ledger, or the launch is offered only
  once the catalog has been read. Not a retry loop in the course.

## Acceptance

- The cause is named, with a trial that makes the refusal happen on demand
  at the base, for example by delaying the ledger's open.
- With the fix, the same trial starts the agent, and the staleness
  contract and the other seven `application_agent_invocation_*_contract`
  rows pass.
- A refusal for a ledger that really is unavailable is still reported.

## Cause (bpr-main-mac, 2026-10-01)

Neither of the two paths the observation names. The window's port had a
ledger host, and the host queued the catalog read behind its own opening.
The opening itself was refused.

The agent had started: the course saw `Running in` and `canary-complete`
before it moved the head. Moving the head re-accepts the pull request, so
`compose_review_ledger` retires the window's ledger host and opens a new one
on the review-ledger lane, and `observe` for the new head reads the session
catalog through it. Opening runs the schema preflight in
`src/adapters/sqlite/src/review_ledger_database.cpp`: it captures the
ledger's main, WAL and SHM bytes, validates a copy, and refuses the file as
`unavailable` if any of the three changed meanwhile. The same file is read
and written by the window's private review store, on the mutation lane and
on agent turns, under a mutex of its own. A private operation inside that
window refused the opening; the ledger stayed unavailable, and the catalog
read queued behind it answered `unavailable`. That refusal reached the panel
after `Finished: stale_result` and inside one poll of the course's wait, so
the course only ever saw `Not started: agent_session_catalog_unavailable`.

The ledger is one file per profile, so the same race could refuse any
window's opening while any private store of the profile was busy, and the
private store's own opening (which runs the same preflight) while a ledger
task was.

### Trial

Instrumentation that was not committed: a print of which file the
preflight saw change, of the opening's answer and of the catalog refusal,
and `BPR_TRIAL_PREFLIGHT_DELAY_MS`, a sleep before the preflight's
recapture. Logs are in `build/logs/bpr-1254/` (`trial*`).

- At `e843d1d84` with the prints only, the first run failed exactly as
  observed: `schema changed main=0 wal=0 shm=1`, `ledger-opened ok=0`,
  `agent-terminal-status=Not started: agent_session_catalog_unavailable`.
- Widened to 500 ms, three runs of three: the reopen refused with
  `wal=1 shm=1` (the private store's `finish` of the stale turn landed in
  the window), and three catalog reads refused `unavailable`. At 100 ms the
  same. The course passed these, because its wait read the stale terminal
  before the refusal arrived.
- With the fix, the same trial at 500 ms (three runs) and 100 ms: no change
  seen, both openings answered available, no catalog refusal, and the course
  passes, its new reopen check included.

## Fix

The profile holds one review-ledger file exclusion,
`github_profile_runtime::ReviewLedgerFile`. Ledger hosts get the
review-ledger lane through `exclusive_file_effect_executor`, which runs each
task under it: opening, every read and write, closing. Each window's
`private_review_workflow` takes the same mutex as its gate instead of its
own, so every private store operation, from whatever thread, holds it too.
Only one of them touches the file at a time, so the preflight cannot see a
change this process made. No product path takes the exclusion on the
decision lane. `docs/architecture/review-ledger.md` says so.

It is not a retry. A file that really changed under the opening, or is not
a ledger at all, is refused as before.

The staleness course pins it. Before it moves the head it asks
`review_ledger_file_exclusive`: a task held on the ledger store lane must
keep a private store question unanswered, and the exclusion held must keep
a ledger catalog read from finishing on its lane. After the stale terminal
it requires the reopened ledger to be settled and available, and the panel
still to say `Finished: stale_result`. Negative controls: with the private
workflow given a mutex of its own, or the ledger host given the plain lane,
the course fails `ledger-file-exclusion answered=true waited=false`.

## Verified on macOS (bpr-main-mac, 2026-10-01)

macOS 26 arm64, Apple clang, Qt 6.11.2, `build/dev` Debug. Logs are in
`build/logs/bpr-1254/`.

- The eight `application_agent_invocation_*_contract` rows pass, `-j1`.
- The staleness course run six at a time, twice: twelve of twelve pass.
- A reopened ledger that really is unavailable (a trial opener refusing
  every opening after the first) is still reported: the course fails
  `ledger-reopened` with `ledger-available=false` and
  `status=Not started: agent_session_catalog_unavailable`.
  `application_review_ledger_composition_contract`, whose `unavailable`
  course puts a directory where the ledger should be, passes.
- Integration pass over the rows that compose the ledger or the private
  store: the agent chat, finding and crash-recovery rows (447 to 462),
  `application_review_ledger_composition_contract`,
  `application_review_round_trip_composition_contract`,
  `application_defect_composition_contract`,
  `application_profile_multi_window_public_path`, the three
  `application_private_review_*` rows, `application_review_ledger_host`,
  `application_private_review_host`, `sqlite_review_ledger_storage` and
  `sqlite_private_store_open`. All pass except
  `application_agent_finding_workshop_press_contract`, which fails the same
  way at `e843d1d84` without this change ("Choose immutable revisions or
  findings in Private history", run-quiescent=false): that is
  [[BPR-1245]]'s parked workshop question.
- The round-trip contract's pins are resealed for `github_application.cpp`
  and the production manifest; the mutation pin is unchanged.
- `coding_format_check`, `architecture_boundaries` and
  `architecture_boundary_contract` pass; `tidy.py --changed` reports
  nothing on a changed line (its 17 findings are all on lines this change
  did not touch); `git diff --check` is clean; DOCUMENTATION_OK.

### Seen, not changed

- The private store's claim connection ends its read and closes outside
  the exclusion. Neither writes the file, and the ledger's own and the
  cached private connections stay open, so its close is never the last one
  that would checkpoint.
- Linux and Windows were not run. The change is portable composition code;
  the card's platform is macOS.
