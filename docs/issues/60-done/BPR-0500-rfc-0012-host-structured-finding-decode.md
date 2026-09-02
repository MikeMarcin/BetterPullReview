---
id: BPR-0500
title: "Host: structured-finding decode, limits, and typed revision-bound re-entry"
status: Done
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

## What this slice delivered — 2026-09-02

Decode of a bounded structured-finding payload in the one portable protocol
loop (`src/adapters/agent/src/agent_protocol_loop.cpp`,
`src/application/src/agent_launch.cpp`), the validation seam and typed
re-entry in the new `src/application/include/bpr/application/agent_finding_delivery.hpp`
and `.cpp`, the host's admission of a frozen snapshot and its delivery on
the completion lane in `agent_invocation_host`, the host's supplement moved
onto the opt-in `build_agent_supplement` overload, and the single
`store_note` path for a promoted note. Proven by the new
`application_agent_finding_protocol` (800 lines, driving the real canary
through each carrying protocol) plus extensions to `application_agent_launch`,
`application_agent_protocol_loop`, and `application_agent_invocation_host`.
Implemented by a bounded `rfc-worker` across two turn budgets; the integrator
inspected the complete diff and reproduced the verification below.

### Wire shape, per protocol

One JSON object per finding inside the protocol's own result framing:
Codex App Server `params.turn.bprFindings` on the completed `turn/completed`
notification; Codex JSONL top-level `bprFindings` on `turn.completed`; Claude
stream JSON top-level `bprFindings` on the successful `result` frame; the
external terminal has no carrier, and a line opening `BPR-FINDINGS/1` is
refused as `unsupported_finding_transport` rather than given framing nobody
specified. Payload `{"session":<n>,"findings":[…]}` with the core's field
names, `suggestedComment` optional, identities as 64 lowercase hex and commit
ids as canonical 40/64 hex; severity and category reuse `core::name`;
confidence's four spellings are declared only in the decoder, which is where
an undeclared enumerator is refused. Bounds `maximum_agent_finding_payload_bytes`
(48 KiB, `static_assert`ed under the frame bound so it is reachable) and
`maximum_agent_findings_per_turn` (32). Five new `agent_protocol_failure`
values, `agent_protocol_failure_count = 12` guarding the count.

### Decisions worth knowing

- **Decoding happens once, in the loop both transports reach.** Frame readers
  lift the payload out as the exact bytes the child wrote (the file-local
  `json_value` gained `Begin`/`End` spans, the widest-blast-radius edit here;
  every existing decoder case and every composed course passes unchanged).
- **The snapshot is admitted, not looked up.** `agent_invocation_request::FrozenFiles`
  carries the per-path comparison, both line counts, and the segment, group,
  and move identities; the host pairs it with the admitted session's own
  binding, and a caller cannot name a revision the session was not started
  against. An empty list is a complete answer: every finding then arrives
  unanchored.
- **The delivery type has no field the current head could occupy**; findings
  are projected immediately before `finish`, and the terminal carries no
  undecoded payload, so verdicts are the only route to a presentation.
- **`deliver_findings` is a non-pure virtual with a no-op default**, because
  a pure virtual would have edited the Qt panel outside this slice; BPR-0503
  overrides it, and until then the shipped window sees no findings.
- **The opt-in is derived from the reviewer's own selection** and keyed to the
  session, which keeps the four courses that assert selected prose reaching
  the child green; a source-order assertion in `application_agent_launch`
  proves the gated overload is called and the ungated one is not.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The host suites pass, new and extended | `cmake --build --preset dev` then `ctest --test-dir build/dev -R '^(application_agent_launch\|application_agent_protocol_loop\|application_agent_invocation_host\|application_agent_finding_protocol\|application_agent_workspace)$' --output-on-failure` | build exit 0; 5/5 passed (the same run's sixth selector, the round-trip contract, failed on the pre-existing digest drift carded as [[BPR-0510]]) | this commit |
| The canary, every composed agent course, and the boundaries hold | `ctest --test-dir build/dev -R '^(qt_agent_launch_canary\|application_agent_invocation_(composition\|cancellation\|staleness\|cleanup_retry\|workspace_diagnostic)_contract\|architecture_boundaries\|architecture_boundary_contract)$' --output-on-failure` | 8/8 passed; canary 64.0 s, composition 28.5 s | this commit |
| The core-only graph builds and runs the agent suites | `cmake --build --preset core-only` then `ctest --preset core-only -R '^(core_agent_\|application_agent_)'` | 11/11 passed; `application_agent_finding_protocol` reports `canary-unavailable` there by design | this commit |
| Formatting | `clang-format --dry-run --Werror` on every touched C++ file | exit 0 | this commit |
| The canary's payload decodes field for field on each carrying protocol; the external terminal refuses by name | `application_agent_finding_protocol` | pass | this commit |
| Every bound and malformation has its own typed cause, and all twelve causes are pairwise distinct | the 46-row sweep plus the count check | pass | this commit |
| A payload after an accepted head move is stale with its OIDs intact | `application_agent_invocation_host` | pass | this commit |
| Non-vacuity | worker mutations, each restored and md5-verified: the validation seam deleted, delivery adopting the current head, each bound removed, two causes collapsed, the host returned to the ungated overload | each failed by name | this commit |

Handed on: `FrozenFiles` is unpopulated in the shipped composition until
[[BPR-0504]] sources the per-side line counts; `store_promoted_finding` has
no caller until [[BPR-0503]]'s gesture; a single maximal finding exceeds the
48 KiB payload bound and is refused as oversize rather than truncated.
