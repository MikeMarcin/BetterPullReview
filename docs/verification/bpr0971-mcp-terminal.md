# BPR-0971 MCP terminal diagnostics

Assignment `r4-mcp-stderr-20260925`, baseline `0f0c461e`, product candidate
`c907c1731947a2e747609304a553c5137631938a`. This bounded continuation of the
[terminal audit](bpr0971-terminal-bounds.md) uses synthetic local inputs only.
The original BPR0971 criteria, RFC0027 classifier question, prior failures and
owner-deferred Windows/Linux runs remain open.

## Changed boundary

The shipped MCP helper now returns an environment observation containing three
complete counts (received, retained, unexpected retained) and two fixed-size
allowlist-presence arrays. It still enumerates the complete environment before
removal and afterward; counts describe actual state, including a failed removal.
Arbitrary names are transient reduction input and never leave that function in
the report. Values were never diagnostic fields and remain absent.

One startup record contains fixed framing, those counts, presence of only the
two source-declared allowlist names, and one fixed readiness/refusal cause. An
unexpected argument contributes only its numeric count. The readiness record no
longer contains the revision binding. No path, arbitrary name, environment value,
argument text, provider response or request body is formatted. The fixed grammar
and integer fields bound construction; delivery additionally refuses any complete
serialized record over 512 bytes. Windows CRT newline translation is separate
from that serialized-byte limit. The actual hostile record is 109 bytes and the
healthy record is 267 bytes on this Mac.

The executable privately links the platform thread runtime. On POSIX it creates
one writer for one raw system write, allows 20 ms for ordinary completion, then
requests deferred cancellation and joins the writer before releasing its buffer
or entering the protocol loop. POSIX `write` is a cancellation point. There is no
retry, queue, detached writer, inherited descriptor flag change or change to MCP
exit/JSON-RPC authority. The only worker code is a write and an atomic completion
fact; it owns no lock or C++ resource that cancellation could abandon.

The helper owns SIGPIPE disposition for this startup attempt: it temporarily
ignores SIGPIPE before starting the writer and restores the prior disposition
after joining. It reads no request and writes no stdout during this interval.
A thread-only signal mask was insufficient on this Mac: the new closed-pipe
course observed exit -13, so the implementation was corrected and that course
now passes. The stdout closed-pipe course confirms the original signal behavior
is restored.

Delivery is explicitly best effort. An unavailable sink cannot report its own
loss through itself; no second sink or protocol message is invented. The full
pipe course observes zero diagnostic bytes delivered, preserved terminal status
and the complete protocol response. A missing record proves nothing about
environment reduction. Uninterruptible kernel/device stalls, process scheduling,
OS crash reports and externally forced termination are outside this bounded
pipe observation. Windows still uses synchronous stderr delivery and needs a
native cancellation/lifecycle design and course; no Windows repair is claimed.

## Verification and negative controls

macOS 26.6.2 arm64, AppleClang 21, core-only Debug, Python 3.14.7, LLVM tools
23.1.1. The helper links no Qt/application host/provider. No native Linux or
Windows result follows from conditional compilation checks.

```sh
cmake --preset core-only -DBPR_DEPENDENCY_OFFLINE=ON -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache
cmake --build --preset core-only --target tool_better_pull_review_mcp_context_helper test_better_pull_review_mcp_context_helper_tests --parallel 2
ctest --preset core-only --output-on-failure -R '^(application_mcp_context_helper|application_mcp_terminal_diagnostics|architecture_boundaries|core_dependency_boundary|conditional_branch_identifiers)$'
python3 tests/scripts/audit_terminal_diagnostics.py --helper build/core-only/src/adapters/mcp/better_pull_review_mcp_context_helper
```

The integration selection passes **5/5 in 9.80 seconds**. The direct final audit
passes all **20 observations** at product `c907c173`; helper SHA-256 is
`f8e5baf1aae980519d39787cb296493a23497c010196774b1a2dd2299ed4d7d0`.

- 257 synthetic hostile names, including a 32 KiB name with newline, carriage
  return and escape, yield exactly `257/0/0` count facts, a useful missing-env
  refusal, exit 1, zero stdout and 109 stderr bytes. Name content, injected lines
  and credential values are absent.
- A fully occupied 65,536-byte stderr pipe yields exit 1 in **0.041 s** for a
  refusal and exit 0 in **0.035 s** for a valid request. The valid request returns
  the exact same **987 bytes** and four-tool list as healthy stderr. Both lose
  their startup diagnostic, leave inherited descriptor flags unchanged and reap
  the child before the harness drains its own fill. These elapsed values include
  launch and harness polling; they are observations, not portable latency limits.
- Closed stderr preserves both exits and the response. Closed stdout retains
  its original SIGPIPE result. Healthy readiness exposes no path or binding.
- Argument injection retains only `unexpected_argument` and count 1.

The prior terminal candidate's exact helper
`fb2c4af2ecf278d0b1f1a2e86f90b021f51df995548194102efe24e64ed71866`
fails the expanded audit: **49,674 stderr bytes**, raw name/injection retained,
both full-pipe children still blocked after about **1.01 s**, and both closed
stderr cases fail. The harness drains only its own fill and reaps those children.
This is a failing negative control, never a passing expected-failure row.

A temporary source control that bypassed environment removal made the complete
count observation fail (audit exit 1), while the other checks remained passing.
The source was restored byte-for-byte in `finally`, rebuilt, and the integrated
course passed. No test-only product seam or negative-control source was retained.

Strict clang-tidy with warnings as errors passes all three touched translation
units and a separate run including the new private header. Commands supplied
`--extra-arg=-isysroot` and the Xcode MacOSX SDK path; an initial raw invocation
without the SDK failed to find `<compare>` and was not accepted as evidence.
Scoped formatting, Python compilation, `git diff --check`, documentation and
BPR0971 board validation pass. The first compiler attempt found macOS signal
helper macros cannot carry `::`; corrected before the passing build.

## Acceptance disposition

AC1 now classifies this MCP startup report and its output bound. AC2 passes only
this synthetic MCP terminal surface. AC3 passes the Mac pipe/closed-sink failure
isolation courses, with one joined writer and no surviving command. Whole-card
acceptance remains pending: the separate capability classifier, remaining
pathological subsystem inventory/courses, application synchronous stderr,
Windows terminal delivery, Linux native verification and external native/crash
limits are not qualified by this work. No new attended criterion is ready.
