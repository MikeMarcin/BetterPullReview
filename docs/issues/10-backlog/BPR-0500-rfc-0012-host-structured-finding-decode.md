---
id: BPR-0500
title: "Host: structured-finding decode, limits, and typed revision-bound re-entry"
status: Backlog
rfc: RFC-BPR-0012
depends_on: [BPR-0497, BPR-0499]
---

# Host: structured-finding decode, limits, and typed revision-bound re-entry

Extends the one portable protocol loop in
`src/adapters/agent/src/agent_protocol_loop.hpp` and `.cpp` to parse a bounded
structured-finding payload from a completed turn, and extends
`src/application/include/bpr/application/agent_invocation_host.hpp` to validate
that payload through BPR-0497 and re-enter it as a typed event carrying the
session's exact `agent_revision_binding`. Oversize, malformed, and stale
payloads become typed outcomes rather than silence. No agent output touches
core or Qt state directly.

## Required final state

- Decode lives in the shared loop BPR-0334 left behind the POSIX and Windows
  transports, so both backends get it once and neither reimplements it.
- The payload carries declared bounds in the style of
  `maximum_agent_stdout_bytes`, and oversize, malformed, and unknown-session
  payloads each end in a distinct typed cause, distinguishable by value.
- Every decoded finding passes BPR-0497 validation before it leaves the host.
  An invalid anchor re-enters as unanchored; it is not dropped.
- The typed event carries the binding the session started with, never the head
  current at delivery; a payload arriving after a head move is stale.
- The decode path holds no core or Qt reference it could write through;
  results arrive over the existing serialized completion lane.
- A promoted private note is stored through the existing
  `review_ledger_port::store_note`; agent output reaches no other authority.

## Non-goals

- The finding type and anchor rules — BPR-0497; promotion policy — BPR-0498.
- Entry-point legality, chat scope, and continuations — BPR-0499.
- The MCP context helper — BPR-0501.
- Any widget — BPR-0502 and BPR-0503.
- Publishing to GitHub — RFC-BPR-0008's mutation path, untouched.

## Required direct evidence

- `ctest --preset dev -R
  '^(application_agent_launch|application_agent_protocol_loop|application_agent_finding_protocol)$'
  --output-on-failure`. The implementing slice records whether the cases extend
  the two existing suites or register `application_agent_finding_protocol`.
- The canary emits a well-formed payload on each supported protocol; the host
  delivers it once, with the session's binding compared field for field.
- A sweep asserts a distinct typed cause by value for each bound and each
  malformation — one byte past each limit, malformed framing, and an unknown
  session — rather than a sample. Collapsing any two fails.
- A payload delivered after an accepted head move arrives stale with its
  analyzed OIDs intact; making it adopt the new head fails by name.
- Deleting the validation call between decode and re-entry fails a case that
  returns an invalid anchor — the seam between the two layers, not either
  layer alone.
- Removing any one declared bound fails the assertion that names it.
