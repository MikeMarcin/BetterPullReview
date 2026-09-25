# BPR-0971 terminal diagnostic bounds

Assignment `r4-terminal-20260925`, baseline `f6de40b6`, standalone continuation
of the [original audit](bpr0971-diagnostics.md). Startup candidate `e1dbf44b` and
Git candidate `2b58ce29` are local Mac engineering evidence, not BPR0971
acceptance. All inputs were synthetic; no account, private repository or remote
mutation was used. RFC0027's capability-error content policy is unchanged.

## Actual paths and limits

| Path | Retained/displayed content | Bound and result |
|---|---|---|
| Installed Git `drain_channel` → `redacted_diagnostic` → `git_process_error::Detail` | Nonzero-exit stderr with whitespace-delimited URL/SSH tokens replaced; other error text and local paths remain | Input over 16 KiB fails textlessly. Each replacement append now respects the remaining 16 KiB result space, including expansion. Exact terminal/result policy is unchanged. |
| Cache error → `epoch_application_host::cache_failure` | Numeric failure and `Detail` through developer `fprintf(stderr, ...)` | Production installed-Git owner supplies the 16 KiB bound; this test-build host writer does not independently cap a different injected port's string. Synchronous write. No Quill/export field. |
| `note_startup_failure` → `FirstStartupFailure` → developer stderr | Fixed composition-arm name plus caught exception text, first failure wins | Cause now scans at most 2,049 input bytes and copies at most 2,048, before retaining it. ASCII C0/DEL controls become spaces, and over-limit causes end ` [truncated]`. The fixed source name/framing is additional. |
| Top-level `main` exception handler → developer stderr | The same bounded cause | Same capture helper, under `BPR_BUILD_TESTING`. Qt UTF-8 conversion may replace malformed byte sequences; the 2 KiB claim is the captured cause bytes, not exact terminal encoding length. No nonblocking-I/O claim. |
| MCP helper environment reduction → `std::cerr` | Every received and retained environment name, no value | No explicit name-length/count/output bound; complete name vectors are copied before reduction. Hostile names reproduce content retention and line injection. |
| MCP fixed startup refusal → `std::cerr` | Closed refusal name and optional argument count | Even an empty environment's fixed refusal blocks behind a full inherited stderr pipe. |

`note_startup_failure`'s source-name callers use fixed application labels. Its
mutex and first-record ordering are unchanged. The capture helper is private to
the composition and included by the two tests-enabled callers only. A
tests-disabled graph keeps fixed arm names but never captures these exception
causes. This does not qualify third-party Qt/platform output, crash reports,
debugger memory or OS I/O stalls.

The Git repair bounds the string's logical bytes throughout redaction rather
than appending all replacements and resizing afterward. It does not claim an
exact allocator-capacity or Qt internal-buffer bound. In the expansion fixture,
15,000 input bytes would previously build 42,000 replacement bytes before the
final 16 KiB resize. Returned bytes are unchanged. The adapter still preserves
useful unfamiliar error text as its existing contract requires; no arbitrary
content classifier or blanket text omission was introduced.

## Focused evidence

macOS 26.6.2, Apple Silicon, AppleClang 21, Qt 6.11.2, dev preset. Configure
reused the existing pinned dependency cache with `BPR_DEPENDENCY_OFFLINE=ON`.
No platform evidence follows for Windows or Linux, which the owner deferred.

```sh
cmake --build --preset dev --target test_startup_diagnostic_bounds app_better_pull_review test_better_pull_review_git_process_contract_tests tool_better_pull_review_mcp_context_helper --parallel 2
ctest --preset dev --output-on-failure -R '^(application_startup_diagnostic_bounds|application_startup_failure_source|git_process_contract)$'
ctest --preset dev --output-on-failure -R '^(application_composition_contract|application_review_round_trip_composition_contract|architecture_boundaries)$'
```

The initial focused run passed 3/3 in 107.57 seconds. It exercised 2,047/2,048/
2,049-byte and 1 MiB startup causes, literal unknown prose, ASCII controls,
nonstandard exceptions, the real composition's retained first failure, and the
real application's unavailable-Git startup refusal. Git exercised 16,384 and
16,385 stderr bytes, URL replacement expansion, and its existing real-Git
diagnostic/refusal/cancellation courses. The first build failed because two new
test locals reused names later in the same course; those names were corrected
before the passing run. After the Git append repair, the same focused checks
passed 3/3 in 110.18 seconds. The architecture and two composition contracts
passed 3/3 in 244.92 seconds, including an actual tests-disabled Release build.
Documentation and board validation pass. Strict tidy passes the new test TUs
and main, but reports two unchanged baseline lines: `exact_current` naming in
the composition and signed `flock` flags in the Git adapter. Both exist in
`f6de40b6`; no rule was disabled. A new `#pragma once` finding was corrected to
an include guard before the final checks.

The opt-in audit below is deliberately not registered as an expected-passing
CTest row. It prints only fixed facts, never the child's diagnostic text:

```sh
python3 tests/scripts/audit_terminal_diagnostics.py --helper build/dev/src/adapters/mcp/better_pull_review_mcp_context_helper
```

Observed exit **1**, helper SHA-256
`fb2c4af2ecf278d0b1f1a2e86f90b021f51df995548194102efe24e64ed71866`:

- One name containing 32,768 filler bytes and a newline produced **32,886 stderr
  bytes**. Synthetic content in the name and the injected line survived. The
  synthetic credential **value did not appear**.
- An empty environment's refusal remained blocked after **1.001 seconds** behind
  a **65,536-byte** full inherited stderr pipe. Draining this test's fill let
  the helper exit normally with code **1**, with no stdout. The child was reaped.

These are bounded counterexamples, not a portable pipe-capacity/latency promise.
The probe always drains or kills/reaps only its own child and closes its pipe.
It writes no diagnostic file, changes no parent environment and has no network.

## Source review and remaining work

Self-review covers `f6de40b6..e1dbf44b` for the app-source pins: two guarded
callers share the same private formatter; the original first-failure storage,
failure mapping, exit code and tests-disabled branches are preserved. The
manifest gains the private header, and its public declaration documents the
bound. No credential, provider, vault, transport or mutation admission changes.
The production mutation-branch digest remains unchanged. The later Git change
`2b58ce29` only clips each replacement append to the existing output limit.

AC1 remains partial beyond these paths. AC2 still needs a content-safe contract
for the documented terminal exceptions and the other pathological courses;
bounded useful prose is not proof that private content is absent. AC3 fails
the real MCP full-pipe course and does not qualify the application's other
synchronous terminal writers. A nonblocking writer must preserve named causes,
loss/availability facts and shutdown ownership; changing a shared inherited
descriptor's flags or adding an unjoined writer is not supplied by this patch.
The next engineering round must reconcile the helper's complete-environment
diagnostic contract with bounded, content-safe observations and finish direct
terminal backpressure. No owner waiver or attended candidate is inferred.
