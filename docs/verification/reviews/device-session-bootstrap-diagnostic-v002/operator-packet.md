# Capability-refusal diagnostic checkpoint

Status: **pending operator decision; no publication authority**.

Authorize one browser/device sign-in as **MikeMarcin** using this exact ordinary
macOS application, read the connection result, use **Copy message** if needed,
then explicitly **Sign out** and quit. The prior diagnostic isolated capability
discovery; this build adds a fixed refusal category within that stage. It does
not establish that the original connection failure is repaired.

## Exact build

- Source commit: `9414ffc93b70aae51104b20c81a974b890eecf5a`.
- Executable: `/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview`.
- SHA-256: `ecf3f273f48e8f8a1dd72e15e85ed401584c8aef8669beea8c9010559669d304`.
- Length: 10,659,808 bytes.
- Release; `BUILD_TESTING=OFF`; designated live capsule `OFF`.

Dev/Release builds, scoped strict tidy and formatting pass. The final native,
parser, provider, host and three-scale Qt checks passed 8/8 in 20.54 seconds;
architecture boundary and all 62 documentation tests pass. Independent bounded
review has no unresolved findings. The existing admission, failure, retry and
cleanup behavior is preserved. Categories contain no raw provider values or
text. Unsupported categories have an explicit unavailable-detail fallback.

## Procedure

1. Reverify this packet and executable digest before launch; abort on drift.
   Start this exact executable once, with no arguments, after confirming no
   BetterPullReview instance is running. Remove these 24 variables from the
   child environment without reading or printing their values:
   `PYTHONPATH`, `PYTHONHOME`, `PYTHONUSERBASE`, `PYTHONINSPECT`, `PYTHONSTARTUP`,
   `DYLD_INSERT_LIBRARIES`, `DYLD_LIBRARY_PATH`, `DYLD_FRAMEWORK_PATH`,
   `DYLD_VERSIONED_LIBRARY_PATH`, `DYLD_VERSIONED_FRAMEWORK_PATH`,
   `DYLD_FALLBACK_LIBRARY_PATH`, `DYLD_FALLBACK_FRAMEWORK_PATH`,
   `DYLD_IMAGE_SUFFIX`, `DYLD_ROOT_PATH`, `DYLD_SHARED_REGION`,
   `LD_PRELOAD`, `LD_LIBRARY_PATH`, `LD_AUDIT`, `LD_DEBUG`, `LD_PROFILE`,
   `GH_TOKEN`, `GITHUB_TOKEN`, `GH_ENTERPRISE_TOKEN`, `GITHUB_ENTERPRISE_TOKEN`.
   Discard child stdout/stderr; do not rebuild or substitute the executable.
2. Use **GitHub.com — Native HTTPS**, **Get a code**, and the browser control.
   The operator authorizes directly as MikeMarcin. Do not capture device codes,
   tokens, credentials, cookies or browser authentication screens.
3. Stop at the first terminal connection result. Copy only the safe displayed
   message, or record the displayed non-secret account if connected. Do not
   open a PR, publish, reply, edit or submit a review, request a second code,
   retry sign-in, or use manual-token Connect. A different account or unexpected
   prompt ends the attempt.
4. Explicitly press **Sign out**, wait for terminal status, and quit normally.
   Report any cleanup failure honestly; do not infer vault erasure from process
   exit. Retain only the safe UI outcome and exit status.

Only ordinary device authorization and session reads are authorized. Supply no
publication grant, approval file, supervisor or designated CTest command. The
one-launch limit is an attended operator instruction, not a newly implemented
machine-enforced gate. Both v001 launches are consumed; their
[unchanged records](../device-session-bootstrap-diagnostic-v001/retry-authorization-and-result.md)
and v007 publication authority cannot be reused.

## Operator response

Return this exact statement:

> I approve one sign-in-only diagnostic run as MikeMarcin using BetterPullReview sha256:ecf3f273f48e8f8a1dd72e15e85ed401584c8aef8669beea8c9010559669d304, followed by Copy message if needed, explicit Sign out and quit. No pull request or publication is authorized.

Or:

> I reject this sign-in-only diagnostic run.
