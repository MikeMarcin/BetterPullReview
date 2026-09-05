---
id: BPR-0608
title: Accept the smart HTTP discovery preamble
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0607]
---

# Accept the smart HTTP discovery preamble

BPR-0607 isolated protocol discovery rejection. A bounded anonymous GET to
MikeMarcin/BPR-Test.git returned an upload-pack service announcement, flush,
and version 2 advertisement. The current parser expects version 2 first.

## Required final state

- Accept the exact optional upload-pack service pkt-line and following flush
  before the required version 2 advertisement; preserve unwrapped v2 support.
- Reject malformed, missing-flush, wrong-service, repeated-preamble and non-v2
  inputs while retaining capability, object-format and response bounds.
- Exercise the wrapped advertisement through protocol acquisition and the
  native transport fixture, with focused parser rejection regressions.
- Rebuild the ordinary Release app and observe the next history outcome using
  remembered MikeMarcin sign-in and the existing sanitized diagnostics. Quit
  without Sign out between iterations under the owner's amended instruction.
- No extra publication, credential export, protocol-v0 fallback, schema-query
  redesign, advanced logging or full test matrix.

## Verification plan

Run `application_git_protocol_v2`, `native_git_smart_transport_contract`,
`native_git_smart_transport_fixture` and `epoch_maintenance_integration` after
building their targets. Run scoped format, documentation gate and diff check.
The official Git [HTTP protocol](https://git-scm.com/docs/http-protocol) and
[v2 protocol](https://git-scm.com/docs/protocol-v2) describe the framing layers.

## Implementation checkpoint

The parser consumes at most one exact upload-pack announcement plus flush,
then applies unchanged v2/capability/object-format checks. Ten malformed or
unsupported wrapper cases and both object formats are covered. The native
HTTPS fixture now serves a valid wrapped advertisement and feeds received bytes
into the production parser; the real epoch/cache integration uses the wrapper.
All four planned selectors passed (49.56 seconds). Dev targets and Release app
built; scoped format, code-simplifier inspection and documentation gate pass.
Live remembered-sign-in/history observation remains in progress.

## Live result and completion — 2026-09-05

Release SHA-256 `58862689e01cfca29afa733f63aefff65f12b2aef5116203cd1bc0b1fae60b56`
at source `092dddd8` ran as sole PID 52525, 11:41:32–11:45:17 UTC.
The remembered MikeMarcin session resumed without a code request or owner
interaction; all nine capability requests succeeded. Protocol acquisition
succeeded at 11:42:46.896081314 UTC and revision preparation at
11:42:47.768826652 UTC. The UI showed base
`e2a44aa8fef704de7373463222b1872e796918c6` and head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`. Selecting
`bpr-live-review-fixture.txt` rendered its source, one insertion, actionable
file 1 of 1 and review history ready. No draft or Publish action occurred.
The app quit with exit 0 and process absence; remembered sign-in was preserved.
Only this run's appended sanitized diagnostics were read. No screenshot or log
was committed. Native capture excluded the device panel.

Strict scoped tidy identified one test-only temporary-string allocation;
corrected with append, then the protocol test passed again (0.37 seconds) and
that TU's strict tidy passed. The other three TUs passed unchanged. A direct
tidy invocation initially lacked Apple's SDK settings; the repository wrapper
supplied those settings for the final checks. No additional full matrix was run.
BPR-0256 can now continue the previously authorized one-comment course.
