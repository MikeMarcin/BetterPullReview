# Repaired sign-in verification checkpoint

Status: **pending operator decision; no PR or publication authority**.

Authorize one browser/device sign-in as **MikeMarcin** using the repaired macOS
application below, followed by reading its sanitized local log, explicit
**Sign out**, and quit. BPR-0605 records this attended verification; BPR-0559's
source repair is complete. This continues the established exact-statement
sign-in-only workflow.

## Exact build and evidence

- Repair source commit: `b14a69d0`; checkout `0aa57ebe` adds documentation only.
- Executable: `/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview`.
- SHA-256: `3dd5c3bb794ae0ef6138e4921b21f4bde8dd7131874d8f5f01609464d5913a16`.
- Length: 11,066,176 bytes.
- Release; `BUILD_TESTING=OFF`; ordinary application, no designated live capsule.

The previous approved run identified `INTROSPECTION_LIMIT_EXCEEDED` in the
sanitized log. The repair uses nine fixed serial capability queries; each
limited introspection collection field expands at most twice, retaining the
same selected schema facts and aggregate limits. The owner chose to keep this
plan and deferred schema-method evaluation to BPR-0604. Live point cost and
connection latency remain unmeasured.

Development and tests-disabled Release builds pass. Integrator reproduction
passed six focused adapter/native/fixture/composed selectors and the CLI refusal
public path; both composition contracts pass after correcting an old expected
error prefix. Scoped tidy/format, architecture checks, and documentation gates
pass. The original query-limit violation is reproduced offline; an independent
critical review has no remaining blocker. These results do not prove live
acceptance of the changed build.

## Procedure and log scope

1. Reverify packet and executable digest before launch; abort on drift. Confirm
   no BetterPullReview instance is running. Start this executable once with no
   arguments; remove these variables from its child environment without reading
   or printing their values:
   `PYTHONPATH`, `PYTHONHOME`, `PYTHONUSERBASE`, `PYTHONINSPECT`, `PYTHONSTARTUP`,
   `DYLD_INSERT_LIBRARIES`, `DYLD_LIBRARY_PATH`, `DYLD_FRAMEWORK_PATH`,
   `DYLD_VERSIONED_LIBRARY_PATH`, `DYLD_VERSIONED_FRAMEWORK_PATH`,
   `DYLD_FALLBACK_LIBRARY_PATH`, `DYLD_FALLBACK_FRAMEWORK_PATH`,
   `DYLD_IMAGE_SUFFIX`, `DYLD_ROOT_PATH`, `DYLD_SHARED_REGION`,
   `LD_PRELOAD`, `LD_LIBRARY_PATH`, `LD_AUDIT`, `LD_DEBUG`, `LD_PROFILE`,
   `GH_TOKEN`, `GITHUB_TOKEN`, `GH_ENTERPRISE_TOKEN`, `GITHUB_ENTERPRISE_TOKEN`.
   Discard child stdout/stderr. Do not rebuild or substitute the executable.
2. Use **GitHub.com — Native HTTPS**, **Get a code**, and the browser control.
   The operator authorizes directly as MikeMarcin. Do not capture device codes,
   tokens, cookies, credentials or browser authentication screens.
3. Stop at the first terminal connection result. The agent reads only the new
   run's diagnostic records at
   `~/Library/Application Support/BetterPullReview/BetterPullReview/logs/diagnostics.log`
   and records the safe UI outcome. The file contains timestamp/request/status,
   fixed lifecycle outcomes and sanitized capability-error prose. It is local,
   capped at 16 MiB and retained after quit; prose may identify the service or
   account because anonymization is deferred. No raw transport dump is enabled.
   If logging is unavailable, record that fact; do not retry automatically.
4. Explicitly press **Sign out**, wait for terminal disconnected status, and
   quit normally. Report cleanup failure honestly. Do not infer vault erasure
   from process exit. Keep the log local; commit only a minimal sanitized finding
   or synthetic reproduction, never the complete live log.

No PR opening, publication, review submission, manual-token Connect, second code,
second launch or sign-in retry is included. A different account or unexpected
prompt ends the attempt. Supply no approval file, publication grant or supervisor.
The one-use limit is an operator instruction, not a new machine-enforced gate.
Prior v001/v002/v003/v004 and v007 authority is consumed. A later changed executable
requires its own authorization under this established workflow.

## Operator response

Return this exact statement:

> I approve one sign-in-only diagnostic run as MikeMarcin using BetterPullReview sha256:3dd5c3bb794ae0ef6138e4921b21f4bde8dd7131874d8f5f01609464d5913a16, including reading its sanitized local log, followed by explicit Sign out and quit. No pull request or publication is authorized.

Or:

> I reject this sign-in-only diagnostic run.
