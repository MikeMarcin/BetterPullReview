---
id: BPR-0608
title: Accept the smart HTTP discovery preamble
status: In Progress
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
