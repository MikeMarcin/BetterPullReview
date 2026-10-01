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
