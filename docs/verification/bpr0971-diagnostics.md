# BPR-0971 diagnostic privacy and backpressure audit

Standalone owner-r4 follow-up, assigned 2026-09-25. Baseline is
`97a7564b6c335ea94f9d8e9957e5e038cf92330d`; the audit/test candidate is
`a0bc72590ba91273806513b09dd2367dfc3069d0`. No product source changed. All inputs
were synthetic and local. This is a partial audit with a reproduced privacy
failure, not acceptance of the original RFC0013 logging requirement.

## Retained file and export fields

The complete `diagnostic_event` surface is in
`src/application/include/bpr/application/diagnostic_log_port.hpp`; its only file
sink is `src/adapters/logging/src/quill_log.cpp`. Native capability sanitization
is in `github_capability_probe.cpp`, invoked only for the capability-probe
surface by `native_https_transport.cpp`. The provider then forwards that bounded
summary. No credential is passed to Quill.

| Field | Class and bound in the readable log | Export disposition |
|---|---|---|
| `v` | Fixed format version 1 | Replaced by fixed document version |
| Timestamp | Backend-generated UTC text; no provider clock/string | Omitted |
| `op` | Five closed names: application, viewer, capability, PR, epoch | Closed numeric code |
| `outcome` | Six closed lifecycle/response names | Closed numeric code |
| `request`, `window_session` | Local unsigned 64-bit correlation identities | Omitted |
| `http_status` | Zero, or integer 100–599; other values drop the event | Preserved integer |
| `summary` | Five closed admission/omission states | Validated, then omitted |
| `dropped` | Atomic unsigned 64-bit aggregate loss count | Preserved integer |
| `detail` | Up to 2,048 bytes of adapter-sanitized provider prose; ASCII control-bearing and oversized events drop before enqueue | Entire unframed suffix omitted |
| `epoch_stage` | Ten closed names | Omitted with suffix |
| `failure_domain`, `failure`, `pack_failure` | Fixed names from typed catalog/cache/transport/protocol/epoch/GitHub/validation enums; unknown enum maps to fixed `unknown` | Omitted with suffix |
| `page_operation` | Thirteen closed names, including `none` | Omitted with suffix |
| `relationship_detail` | Fixed validation enum name | Omitted with suffix |
| `transport_source` | Four closed names; unknown value drops event | Omitted with suffix |

The summary parser first limits input to 1 MiB, depth 32, 64 object members and
1,024 array elements; it admits at most eight GraphQL error objects. Each of
`message`, `type`, `extensions.type` and `extensions.code` must be a string of
at most 8,192 Qt characters. The complete encoded summary must fit 2,048 bytes.
The credential input is nonempty and at most 8,192 bytes. Duplicate/malformed
JSON, invalid fields, missing credential, over-limit input/output and exceptions
produce an omission state with no original-text fallback. Raw authentication
bodies, requests, headers, arbitrary extensions and ordinary read bodies have
no file field.

The serializer validates the bounded prefix and never parses/copies the detail
suffix. `prepare_release_diagnostics` bounds input at 16 MiB / 65,536 records /
8,192 bytes per line and output at 1,024 records / 256 KiB. Unknown, incomplete
or malformed lines are omitted with counts. Export publishes the exact frozen
preview through the existing host and admitted new-file capability.

## Source inventory by subsystem

| Subsystem | Diagnostic path and retained material | Audit disposition |
|---|---|---|
| GitHub | `github_provider.cpp` records viewer/capability lifecycle and typed PR page observations; `github_window_read_host.cpp` records accepted/failed PR terminals. Capability errors alone carry summary text. Native/CLI transport bodies have no raw log call. | The summary exception fails the content canary; normal PR/file/error metadata remains typed. |
| Git | `epoch_application_host.cpp::epoch_diagnostics` records catalog/cache/open/recovery/prepare/protocol stages, request number and typed failure only. `git_cache_error::Detail` never enters this file logger. | Existing seven-failure matrix preserves results under log refusal; full app unsafe-cache course passes. Installed-Git diagnostic `Detail` separately retains up to 16 KiB stderr with URL/SSH-token replacement and otherwise preserves text. Developer stderr can print this detail. |
| Diff | Diff host/core produce typed status, reason and identity observations. Neither diff host nor diff compute has a logger port, Quill call or stderr writer. | No source/diff serialization into the diagnostic file/export was found. A dedicated hostile-diff-to-all-terminal-output course remains unrun. |
| Syntax | Classification port returns typed failure/token spans; syntax adapter has no logger or stderr writer. | No syntax text field in retained diagnostics. Dedicated oversized lexical input plus terminal capture remains unrun. |
| Persistence | Epoch catalog emits only typed stage/failure through the epoch wrapper. Other stores report typed application outcomes; no SQLite message/body field exists in Quill. | Five logger statuses now preserve exactly one authorized catalog commit and quiescent shutdown. Main developer exception reporting remains a separate surface. |
| Queue | Queue host has no logger port. Any shared provider PR observations have the same closed field contract above. | No queue title/body/observer payload is serialized by the logger. Dedicated queue storm plus all-output capture remains unrun. |
| Agent | Agent protocol collects at most 1 MiB stderr into its process result; it has no Quill/export path. Launcher failure strings and Git workspace error detail can remain in memory. Composition emits one workspace-recovery refusal to stderr with the local registry path and a fixed reason. | Raw agent output is not a log/export field. Native child/OS diagnostic privacy and blocked-terminal behavior remain separate unqualified surfaces. |

This inventory used source searches for the diagnostic port/event, every
`record` producer, Quill macros, stderr/stdout, `QTextStream`, Qt diagnostic calls,
and the process-result error fields. Source absence is a structural observation,
not an executed exhaustive pathological course.

### Terminal and external surfaces

Production application stderr includes fixed runtime/resource/activation/Dif
refusals, fixed cleanup-source labels/counters, and the workspace registry path.
The latter is identifying local context, has no added diagnostic-specific text
cap, and is documented in the operator guide. These direct writes do not use
Quill's dropping queue. No claim is made that a full inherited stderr pipe cannot
block them. Developer builds additionally print epoch cache details (16 KiB at
the installed-Git owner) and top-level `exception.what()` without a separate
text bound. Their compile guards exclude these developer messages from a
tests-disabled application.

The stdio MCP helper prints environment variable **names**, never values, plus
fixed refusal names/argument count and its pinned numeric/OID revision binding.
Received environment names have no explicit application-level count/length
bound before printing. A standalone invocation can supply hostile names; the
composed launcher uses its own narrowed environment. This audit did not run a
hostile helper-environment or blocked stderr course. Third-party Qt/platform
messages, native crash reports, debugger/core dumps and resident source/token
memory remain outside the file export filter. Windows/Linux and full native
crash qualification are deferred, not passes.

## Reproduced privacy failure

Both commands below deliberately return 1 on this candidate:

```sh
/Users/jaeger/.codex/worktrees/bpr0971-diagnostics/BetterPullReview/build/dev/tests/logging/quill_logging --audit-content-policy
/Users/jaeger/.codex/worktrees/bpr0971-diagnostics/BetterPullReview/build/dev/tests/github_contract/better_pull_review_github_provider_tests --audit-content-policy
```

Observed fixed output:

```text
diagnostic-content-audit credential_retained=0 content_retained=1 export_retained=0
provider-content-audit content_retained=1
```

The first course sanitizes synthetic GraphQL error strings, writes them through
the real private Quill file and processes that file through the production export
classifier. The second uses the public provider facade with its fake wire port,
real sanitizer and diagnostic port; its failed capability terminal remains
unchanged. Short ordinary-word source canaries survive sanitization. Token-shaped
redaction cannot classify arbitrary private content. This is an offline
counterexample, not evidence that a real service returned private material.
Neither course contacts HTTPS; native wire composition is established by source
inspection here, not a new TLS fixture.

A temporary test-only negative control removed the summary at the Quill call.
The first command then returned 0 with all three retention flags false. Exact
test bytes were restored, rebuilt, and the content probe returned 1 again.
The probe is opt-in and is not registered as an expected-passing CTest row;
future repair must make it pass rather than encode current retention as desired
behavior.

RFC0027 phase 1 explicitly requires useful unfamiliar sanitized error prose;
BPR0971 AC2 excludes private content and uncontrolled provider text. Those
requirements need an implemented content-classification contract. No claim is
made that blanket omission of all useful unknown errors satisfies both. BPR0603
retains broader privacy/controls work. This audit does not waive either contract.

## Verification and bounds

Environment: macOS 26.6.2 (25G83), Apple M4 Pro arm64, 48 GiB RAM, AppleClang
21.0.0, Qt 6.11.2, Debug dev preset. UI courses use the existing automated fixture
boundary and offscreen configuration. No live accounts, private targets or
remote mutations were used.

- Nine focused CTest rows passed in 49.18 seconds: `application_epoch_host`,
  `application_release_maintenance`, `github_provider`, `application_logging`,
  `application_logging_full`, `application_logging_unavailable`,
  `application_release_maintenance_public_path`, `quill_logging`, and
  `quill_logging_backpressure`.
- After adding malformed/oversized-summary cases and final test cleanup,
  `github_provider|quill_logging` passed 2/2 in 11.54 seconds. Other relevant
  inputs were unchanged. The new limit cases prove no raw fallback for 1 MiB+1
  body, 8,193-character field, 2,048-character summary plus framing, duplicate
  keys and wrong field type. An initial 2,048-byte token-shaped fixture was
  correctly redacted below the cap; it was changed to whitespace to exercise
  the intended encoded-output limit rather than assert the wrong omission.
- Final stress: eight concurrent writers, 80,000 attempts, 2,048-byte summaries;
  79,501 observed drops, 1,161,374 retained bytes, 29 ms total, 3 ms close. The
  next admitted record reports exactly the counted loss. The pressure course
  raises its file cap to 256 MiB so sink fullness cannot hide queue recovery;
  the ordinary 16 MiB cap is independently exercised by the full-file application
  course and existing crossing-cap/reopen tests. Timings are observations on
  this host, not portable latency guarantees.
- Structural pressure bounds: nonblocking `try_lock` ingress, 64 KiB bounded
  dropping queue per producer thread, at most 16 remembered producer thread IDs
  per logger, backend transit soft/hard limit 128, initial transit capacity 32,
  at most 64 logger owners, 16 MiB production file. No aggregate RSS theorem or
  hard OS I/O shutdown deadline is claimed. The drop counter has no saturation
  guard before its uint64 wrap boundary.
- Full/unavailable file sinks retain sign-in success, the typed cache refusal,
  absent comparison and ordinary shutdown through the real app. The epoch host
  preserves one durable catalog commit and final quiescence for available,
  unavailable, full, dropped and closed statuses. The seven existing failure
  modes preserve zero commits and typed failure for available/unavailable/full/
  dropped. The existing blocked-stderr child course proves Quill failure emits
  no blocking fallback; it does not cover the separate terminal writers above.

## Acceptance and continuation

AC1 is partial: the file/export field and source inventory is complete for this
candidate, with the separate terminal surfaces and their missing bounds named.
AC2 fails on provider content retention; the broader seven-subsystem composed
hostile-input inventory remains open. AC3 passes the scoped Quill pressure,
file-refusal, epoch durability and export courses, but does not establish
all-terminal backpressure or native crash privacy.

Next engineering action: define and implement an error-content classifier that
preserves justified unknown-error usefulness while omitting unclassifiable
spans; rerun both opt-in probes and existing unfamiliar-error regressions. Then
complete the named non-Quill terminal/privacy courses before requesting full
BPR0971 acceptance. Native/human evidence remains required only where its
original owning criterion requires it. This card has no ready attended candidate.
