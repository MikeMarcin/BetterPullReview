---
id: BPR-0063
title: Retain cache-fixture commit hex across Git invocation
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0043]
---

# Retain cache-fixture commit hex across Git invocation

Repair the Git cache security fixture defect exposed by BPR-0050's Ubuntu
26.04 x86_64 run. The force-push helper stores a string view returned from
`canonical_hex` on a temporary owner, then uses that dangling view to build
the `git update-ref` argument. Linux passed corrupted nonhex bytes and Git
correctly exited 128; prior macOS runs happened to retain readable temporary
storage.

Keep the canonical hex owner alive across argument construction and the
fixture Git call without weakening exact-OID verification. Rerun the cache,
process, maintenance, protocol, and architecture contracts under Clang and
GCC, run sanitizers and documentation/workflow/diff gates, and resume
BPR-0050 only after the remediation is committed.
