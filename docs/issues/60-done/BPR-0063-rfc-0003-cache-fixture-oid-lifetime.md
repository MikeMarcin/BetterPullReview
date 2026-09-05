---
schema: "implement-rfc.card.v1"
id: "BPR-0063"
title: "Retain cache-fixture commit hex across Git invocation"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0043"]
completed: "2026-08-01"
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

The adjacent lifetime audit found the same temporary-view pattern in the Qt
comparison control and epoch history model OID renderers. This card also owns
retaining those canonical owners and directly rerunning the existing exact
SHA-1/SHA-256 Qt text assertions; leaving the product renderers dangling would
silently narrow the remediation to its fixture symptom.

## Result

The force-push fixture now retains the canonical commit-hex value while its
view is converted into the `git update-ref` argument. The comparison control
and epoch history model likewise retain their canonical values through Qt
string construction. All other `canonical_hex(...).view()` uses were audited;
the remaining occurrences consume the view within the same full expression
and do not outlive the temporary owner.

## Evidence

- Ubuntu 26.04 x86_64, Linux 7.0.0-28, GCC 15.2, and Git 2.53: the focused
  protocol, three-scale Qt, installed-Git process, full cache, and maintenance
  matrix passed 8/8 in 64.56 seconds. The prior syscall trace showed the
  dangling bytes passed to `update-ref` and Git exiting 128; the repaired run
  completes the exact force-push/prune path.
- macOS arm64 Clang passed the same 8/8 matrix in 306.90 seconds. ASan/UBSan
  passed it 8/8 in 412.84 seconds, including the 309.06-second cache contract.
- The Qt cases require exact 40-character SHA-1 and 64-character SHA-256 OID
  text at 100%, 125%, and 200% scale. The source audit finds no assigned view
  from a temporary canonical-hex owner in `src/` or `tests/`.
- The macOS and Ubuntu GCC architecture contracts each passed 2/2:
  `architecture_boundaries` and `architecture_boundary_contract`. The
  target-native QEMU run completed in 429.79 seconds.
- The focused simplification review retained explicit owner and view locals;
  collapsing them recreates the lifetime defect, while a broader conversion
  helper would add churn without strengthening ownership.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`; tracked and on-disk GitHub Actions workflow counts are
  both zero; `git diff --check` passes.
