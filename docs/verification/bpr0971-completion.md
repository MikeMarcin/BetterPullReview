# BPR-0971 completed diagnostic audit

Assignment `r4-full-card-20260925`, coordinator-implementer recipe rev2. Source and
tracker are `/Users/jaeger/Documents/GitHub/BetterPullReview`, branch `main`;
source baseline `e571388269e969d1ceacdb3b647d019cdeb873ea`. Product/test candidate:
`9d2fe772f506b8aa63356d7cfb992f9516c9dfae`. This record completes the original whole-card AC1–AC3
on macOS under the owner's Windows/Linux native-run deferral. It grants no
RFC0014 release, native crash, Windows or Linux qualification.

The [initial audit](bpr0971-diagnostics.md), [classifier investigation](bpr0971-capability-classifier.md),
[terminal slice](bpr0971-terminal-bounds.md) and [MCP slice](bpr0971-mcp-terminal.md)
remain historical evidence of their own candidates. Their unresolved application
stderr and arbitrary-provider-content failures are resolved below; their native
limits are retained. No live account, private target or remote mutation was used.

## AC1: inventory and classification

The initial audit's complete `diagnostic_event` field table remains the field
inventory. Version, operation, outcome, omission state, page operation, stage,
failure/domain/pack/relationship and transport source are closed classifications;
request/window identities are bounded local integers; HTTP status is zero or
100–599; time comes from the backend. Invalid values and ASCII controls are
refused before enqueue. The only changes to that table are the safer `detail`
classification and saturating uint64 loss counts below. No raw body/header,
credential, repository identity or source-text field was added.

| Subsystem / owning source | Retained diagnostics, bounds and evidence |
|---|---|
| GitHub: provider, window read host, native HTTPS capability boundary | Typed lifecycle/page/status fields plus classified capability summary, at most 2,048 encoded bytes. Sanitizer caps body at 1 MiB, depth 32, members 64, array 1,024, eight errors, each string 8,192 Qt characters and credential 8,192 bytes. Unknown codes and schema symbols are omitted explicitly. Real sanitizer/provider/file/export canaries and the native loopback HTTPS fixture pass. |
| Git: installed Git process, epoch host | Installed process result may hold at most 16 KiB of redacted stderr in memory; it is not a diagnostic file/export field. All epoch stderr copies of that text were removed. The existing typed epoch logger preserves stage and failure. Real app cache refusal, epoch failure/durable-success matrix and large Git comparison courses pass. |
| Diff: diff host and compute | Typed result/failure/identity observations; no diagnostic logger or terminal writer. Synthetic source canary through actual diff host succeeds; over-limit input has exactly one typed refusal and quiescent completion. The pathological real app course reads the retained file and production export. |
| Syntax: classification adapter | Typed failure and token spans in the result, no diagnostic text writer. An 8 MiB-plus canary source is refused with `limit_exceeded` and no spans. Public owning output capture and composed large/binary/unsupported paths pass. |
| Persistence: epoch catalog and profile stores | Typed failure/status only; no SQLite error body enters the event. Arbitrary startup exception text now becomes an explicit omission. Five logger statuses preserve exactly one authorized catalog commit and shutdown; failed epochs preserve zero commits. |
| Queue: enrollment host and shared provider | No title/body/observer diagnostic field. A synthetic title survives the authorized queue/store path but not captured outputs.256 overflow-rule attempts at maximum collections create no persistence effect; public host assertions and output capture pass. |
| Agent: protocol loop, workspace recovery, MCP helper | Agent stderr remains a bounded 1 MiB process-result field, never Quill/export. A canary spanning fake-transport stderr chunks preserves the owning limit and terminal result. Workspace refusal keeps a fixed reason and `path=omitted`. A malformed synthetic registry under a canary path exercises that real composition. MCP keeps only environment counts, allowlisted names, fixed refusals and revision/OID binding in one 512-byte record; arbitrary names/values are excluded. |

### Capability contract interpretation

RFC0027 requires useful unknown-error explanation and explicitly permits omission
of unclassifiable spans. An uppercase code or identifier shape gives no provenance.
The sanitizer now reconstructs three closed explanation forms (unknown schema
field, absent field/type, repetition limit), retaining only exact identifier tokens
from the compiled public introspection documents. Known error codes remain;
unknown codes/types, arbitrary symbols, trailing context and unmatched prose get
explicit omission markers. Repetition counts are not echoed. Credential reflection
redaction still precedes classification. Errors/exceptions/limit refusal have no
raw fallback.

Tests keep an unfamiliar code with useful known public field/type/repetition
explanations. Historical assertions requiring arbitrary `newField` or
`UNFAMILIAR_SCHEMA_CODE` bytes were corrected because they contradicted private
content exclusion. All eight original canary payloads remain unchanged. The design
preserves classified operational information without claiming arbitrary prose safe.
It does not deliver the later RFC0027 Settings/privacy/retention work.

### Application terminal and Qt boundary

The executable explicitly owns the terminal service before Qt startup and destroys
it after application producers and Qt have quiesced. The adapter-free application
layer no longer prints epoch error data directly. Every composition stderr
producer uses the owned sink. Startup source arms and safe fixed causes remain;
arbitrary exception text is omitted after examining at most the known-literal
length plus one byte. Qt output keeps severity and an omission marker, except the
exact safe `dif-catalog-unavailable` cause. Resource/activation/cleanup reasons and
counters remain fixed typed values. Workspace paths are omitted.

Ingress uses a producer-only `try_lock`; the consumer owns the read cursor and
slot/ready semaphores publish and reclaim records without competing for that lock; the ring has 64 records of 2,048 bytes each and accepts
at most 1,968 bytes per record, reserving framing space. Oversize, contention and
full-queue attempts increment a saturating uint64 counter. The next admitted
record includes the loss count. A capped Qt text device drops oversized formatted
records whole. The one sink thread uses the inherited stderr without changing its
flags. There is no synchronous caller fallback or lazily created global owner.
The atomic pointer borrows the explicit owner only within its lifetime.

POSIX grants 20 ms for ordinary drain, then cancels the blocked write and joins;
cancellation is disabled around locks and enabled only at `write`. SIGPIPE is
ignored during the explicit process lifetime and its prior disposition restored
after joining. Windows source uses an owned `WriteFile` thread,
`CancelSynchronousIo` through the pre-write race, and join before releasing
storage; MCP uses the same Windows cancellation approach for its one record.
Both Windows branches remain native-unqualified.

Failed/partial writes and shutdown cancellation may lose records and their loss
summaries. An unavailable destination cannot report its own final losses. This
best-effort diagnostic delivery does not replace application/protocol terminals,
which retain their separate delivery and durable authority. No absolute wall-time
guarantee is made for uninterruptible device/kernel I/O or arbitrary driver stalls.

## AC2: pathological public courses

- The eight canaries include ordinary private prose, code/type-shaped values and
  schema-shaped symbols. Sanitizer, provider facade, real Quill file and real
  export all report no private-content or credential retention. The three audit
  commands are now ordinary required CTest rows, not opt-in failing probes.
- Native HTTPS loopback retains the actual credential-owner sanitization route.
  Duplicate/malformed/oversize body, field and encoded-summary refusal retain no
  original fallback. The hostile whitespace fixture is 8,193 characters, because
  classification now shortens arbitrary prose before encoded-output admission.
- `application_diagnostic_subsystems` captures stdout and stderr from actual diff,
  syntax, queue and agent owning tests with the shared canary. It checks success
  and absence of canary output. Source inventory establishes that these owners
  have no separate diagnostic file sink; this is supplemented by the full app.
- The real pathological review contains 515 files, 1,000 threads, a 20,000-line
  comparison, binary/unsupported/oversized inputs and recovery. After actual
  shutdown it reads the private Quill file and calls `prepare_release_diagnostics`.
  Synthetic source fragments, file name, local root and authentication fields are
  absent, while shutdown and nonempty exported records prove the path ran.
- The real malformed workspace registry is below a synthetic canary path. Its
  terminal output contains the classified refusal, neither path nor registry
  content. Hostile Qt text, a 1 MiB exception and an oversized terminal record
  exercise omission and loss reporting.

The export classifier remains independently bounded: 16 MiB input, 65,536 records,
8,192 bytes/line; output at most 1,024 records/256 KiB. It admits closed metadata
and discards unframed detail. Existing public frozen-preview/file-publication
verification remains passing. There is no retroactive rewrite of older logs.

## AC3: pressure, shutdown and authority

Quill retains nonblocking ingress, 64 KiB dropping queue per producer,16 remembered
producer threads per logger, backend transit soft/hard 128 and initial 32, at most 64
logger owners, and a 16 MiB ordinary file cap. Loss counters now saturate. The
stress course uses 8 writers/80,000 attempts and a raised 256 MiB test sink so the
file cap cannot hide recovery. Final Quill measurement: 78,849 dropped,
2,678,507 retained bytes, 68 ms total and less than 1 ms measured close.
Queue pressure reports exact loss on the next
retained record. The ordinary full-file and unavailable-file courses remain
separate. No aggregate RSS theorem or hard OS file-I/O deadline is claimed.

The terminal stress course submits 80,000 concurrent records and a recovery
record, checks `retained + reported_dropped == 80,001`, and verifies progress.
The pipe course fills a blocking inherited pipe or closes its reader, then runs
the terminal driver and actual app startup failure. Stdout and exit are identical
to the ordinary destination. It also runs a real authenticated app with a full
Quill file through typed cache refusal and shutdown under each pipe failure.
Observed final pressure: 5,508 retained + 74,493 dropped = 80,001,
5,772,714 bytes, 145 ms. Ordinary hostile-input output was 150 bytes. Full/closed
pipe driver/app startup took 36/95 ms and 17/76 ms respectively; authenticated
full-log workflows took 5.668/5.640 s. Timings describe this host only.
Inherited descriptor flags remain unchanged. MCP refusal exits and exact JSON-RPC
stdout remain unchanged under full/closed stderr.

The existing epoch matrix exercises available/unavailable/full/dropped/closed
logger status during success: exactly one catalog commit and quiescence remain.
The seven failure courses preserve typed failure and zero commits. Diagnostics
receive no new authority or capability. The production mutation-branch digest
is unchanged; source-manifest/full-TU reseals cover only reviewed diagnostics and
the removed now-unused workspace-path capture. This is implementer self-review,
not independent or human acceptance.

## Verification record

macOS 26.6.2 (25G83), arm64 Apple M4 Pro, 48 GiB, AppleClang 21, Qt 6.11.2.
Commands ran in the source checkout with fixtures/offscreen isolation.

- Focused integration: 10/10 in 79.84 s: epoch host, MCP terminal, native HTTPS,
  provider, capability probe, pathological large review, release-maintenance
  public path, terminal diagnostics, subsystem capture and Quill logging.
- Final changed-boundary suite: 9/9 in 32.59 s: application terminal, MCP terminal,
  startup bounds/source, review round-trip composition, three content-policy rows
  and Quill backpressure. Startup-source repeat: 20/20 in 2.91 s; ordinary terminal
  producer/consumer regression: 50/50.
- Tests-enabled app/terminal/logging/MCP build: pass. Tests-disabled production
  app build: pass (390 build steps initially, then 3 after the semaphore repair).
- Architecture boundaries: pass. Final source graph/composition and tests-disabled mutation-host inventory: pass.
- Scoped clang-format 22 C++ files and `git diff --check`: pass.
- Strict clang-tidy terminal and MCP units: pass; Quill unit: pass. Capability
  unit reports exactly three pre-existing Glaze option-name findings at lines 33–35.
  The baseline source compiled through the same filtered database/toolchain
  reproduces all three; no suppression or false clean claim was added.
- Documentation and board validation: recorded in the final handoff after card movement.

Earlier baseline provider canary failed with `content_retained=1`. The unchanged
canaries now pass. A temporary direct-`write` bypass of terminal ingress caused
the full-pipe negative control to time out; the child was killed/reaped, original
source restored and final app/driver rebuilt. Prior MCP negative controls remain
recorded in their owning evidence. No negative-control source is retained.

A final startup-source check exposed a real consumer/producer-lock contention
loss under ordinary stderr. The queue now serializes producers only; the consumer
uses bounded slot/ready semaphores. The owning startup source course passed 20 repetitions and the ordinary terminal
course passed 50 repetitions after repair.

During test development a queue observer retained 10,000 whole maximum-size
projections and grew to 4.4 GiB; the test process was stopped. The final 256-attempt
course clears the test recorder between attempts and verifies unchanged product
refusal behavior. An initial manifest inventory order/pin mismatch was corrected
and the owning source graph rerun. Two mistyped MCP build target names were
corrected to the existing target before the successful build; they did not build
or change source. These are test/harness corrections, not hidden product passes.

## Acceptance and limits

AC1–AC3 pass for this complete synthetic macOS card under the owner's explicit
native-run deferral. Windows/Linux native compilation and full/closed sink
behavior remain unqualified, including the new Windows cancellation source.
External native crash reports, debugger/core dumps, resident credential/source
memory, child process/OS-owned output and uninterruptible kernel/device stalls
are outside the retained diagnostic/export guarantee. No native crash-content
observation was supplied. Later RFC0027 controls remain their separate work.
MASK preflight remains inherited incomplete (`state.drift`), host loading
`not_verified`; no tool-state repair is claimed. No new attended candidate was
needed or created, and protected RFC0052 session/server/waiter were untouched.

## Reproduction commands

Run in the source checkout, using the current configured dev and tests-disabled
`build/release-no-tests` trees. The documentation commands require Python3.10+.

```sh
cmake --build --preset dev --target app_better_pull_review test_terminal_diagnostics test_application_logging tool_better_pull_review_mcp_context_helper --parallel 4
ctest --preset dev --output-on-failure -R '^(application_epoch_host|application_mcp_terminal_diagnostics|native_https_transport_fixture|github_provider|github_capability_probe|application_large_review_pathological|application_release_maintenance_public_path|application_terminal_diagnostics|application_diagnostic_subsystems|quill_logging)$'
ctest --preset dev --output-on-failure -R '^(application_terminal_diagnostics|application_mcp_terminal_diagnostics|application_startup_diagnostic_bounds|application_startup_failure_source|application_review_round_trip_composition_contract|github_capability_content_policy|github_provider_content_policy|quill_logging_content_policy|quill_logging_backpressure)$'
ctest --preset dev --output-on-failure -R '^application_startup_failure_source$' --repeat until-fail:20
cmake --build build/release-no-tests --target app_better_pull_review --parallel 4
python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths src/app/src/terminal_diagnostics.cpp src/adapters/mcp/src/mcp_context_helper_main.cpp src/adapters/logging/src/quill_log.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
python3 .agents/skills/implement-rfc/scripts/issue_board.py --repo-root . validate --include BPR-0971 --json
```
