# BPR-0971 MCP terminal handoff — 2026-09-25

## Status

Role: rfc-implementer | Model: gpt-6-astra | Effort: xhigh

Bounded assignment `r4-mcp-stderr-20260925`, baseline `0f0c461e`, branch
`codex/bpr0971-mcp-stderr`, source/tracker root
`/Users/jaeger/.codex/worktrees/bpr0971-mcp-stderr/BetterPullReview`.
The existing In Progress claim and `privacy-and-backpressure` plan remain live.
Codex only; no delegated agent, primary-checkout edit or push.

## Milestone result

- `895a1cef`: bind this continuation and its scope to the existing plan/claim.
- `c907c173`: replace arbitrary environment-name reports with complete counts
  and fixed allowlist presence; cap one startup record; isolate POSIX stderr
  writes with an owned, canceled-if-stalled and joined writer.

Product paths are `src/adapters/mcp/CMakeLists.txt`, the MCP public header,
`mcp_context_helper.cpp`, `mcp_context_helper_main.cpp` and the new private
`mcp_terminal_diagnostic.hpp`. Tests update the existing public helper course,
register `application_mcp_terminal_diagnostics`, and turn the previous failing
opt-in probe into an expected-passing POSIX regression. The architecture's MCP
privacy contract now describes the actual report and delivery limits.

## Verification

Core-only configure and focused build pass. Integrated CTest **5/5, 9.80 s**:
MCP public course, terminal diagnostics, architecture boundary, core dependency
boundary and conditional branch identifiers. Final direct audit at `c907c173`
passes all **20 observations**, helper SHA-256
`f8e5baf1aae980519d39787cb296493a23497c010196774b1a2dd2299ed4d7d0`.

257 hostile names yield 109 bounded bytes and no name/value/injection; a full
65,536-byte pipe loses only the diagnostic and preserves exit/987-byte protocol
response, measured at 0.041/0.035 s. Closed stderr and stdout signal restoration
pass. Prior-binary and temporary removal-bypass controls fail. Strict tidy on
all three translation units plus the new header, formatting, Python compilation,
documentation and board validation pass. Exact commands and earlier corrected
failures are in the [evidence](../../../verification/bpr0971-mcp-terminal.md).

## Review handoff

No ready human criterion or new session. Existing RFC0052 protections and review
server are untouched. This is implementer verification/self-review, not an
independent/native-platform acceptance report.

## Ownership and command state

All task-owned builds, tests, audits and tidy commands have finished. Each
synthetic helper child was reaped, each pipe closed and temporary directory
removed. The negative control was restored before the final build. No writer,
server, watch or native app is handed off. The isolated worktree/build and
ignored tidy output remain for coordinator integration. Writer ownership is
released with this checkpoint; ordered commits may be cherry-picked to main.

## Continuation and uncertainties

Integrate this bounded MCP repair and retain BPR0971 In Progress. AC1 remains
partial across the whole inventory, AC2 still has the separate classifier and
other pathological sources, and AC3 still has application synchronous stderr.
Windows has bounded diagnostic content but retains synchronous delivery and
needs its own cancellation/lifecycle implementation. Linux uses the POSIX code
but lacks native verification. Both station runs were owner-deferred. No claim
covers uninterruptible kernel/device stalls or external native crash reporting.
Next work is the independent remaining terminal/subsystem courses or resolution
of the recorded RFC0027 classifier contract; do not infer a privacy waiver.
