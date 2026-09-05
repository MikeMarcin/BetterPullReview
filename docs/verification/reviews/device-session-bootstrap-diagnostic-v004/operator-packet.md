# Logged sign-in diagnostic checkpoint

Status: **pending operator decision; no PR or publication authority**.

Authorize one browser/device sign-in as **MikeMarcin** using the exact logging-
enabled macOS application below. The agent will read its sanitized local log,
then explicitly **Sign out** and quit. The previous GraphQL error envelope was
observed before logging existed; no live diagnostic log currently exists.
The exact compiled query passes its recorded schema-aware fixture. No concrete
root cause or repair is yet established.

## Exact build and evidence

- Logging implementation commit: `3d109fd4f5e937fe0d19ac84ec0212ed9168bd70`; current checkout `755a0f65` adds documentation only.
- Executable: `/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview`.
- SHA-256: `baf16706e2e0427e29b7c9ba2b36b22c6c3377d8c86d9fd3da74f9b799ed46a4`.
- Length: 11,066,048 bytes.
- Release; `BUILD_TESTING=OFF`; ordinary application, no designated live capsule.

BPR-0602 records development/Release builds, seven focused runtime/composition
selectors, native HTTPS loopback fixture, boundary/dependency checks and scoped
format/tidy passing. Integrator repeated six direct runtime selectors: 6/6 pass.
The logging path preserves unfamiliar capability error message/type/code prose,
filters active credential representations before queueing, and excludes device/
authentication bodies and arbitrary response fields. Logging failures preserve
provider completion and cleanup. A bounded independent review has no remaining
blocker. These are offline results, not evidence of a repaired live sign-in.

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
Prior v001/v002/v003 and v007 authority is consumed. A later changed executable
requires its own authorization under this established workflow.

## Operator response

Return this exact statement:

> I approve one sign-in-only diagnostic run as MikeMarcin using BetterPullReview sha256:baf16706e2e0427e29b7c9ba2b36b22c6c3377d8c86d9fd3da74f9b799ed46a4, including reading its sanitized local log, followed by explicit Sign out and quit. No pull request or publication is authorized.

Or:

> I reject this sign-in-only diagnostic run.
