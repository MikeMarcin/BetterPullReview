---
id: BPR-0064
title: Make protocol-v2 parser bounds explicit in GCC Release
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0043, BPR-0057]
completed: 2026-08-01
---

# Make protocol-v2 parser bounds explicit in GCC Release

Repair the optimizer-only GCC 15.2 warnings exposed by BPR-0050's fresh
tests-disabled Ubuntu 26.04 x86_64 Release build. The streaming protocol-v2
parser bounds every packfile section-header payload before consumption, but
`consume_payload` relies on those distributed invariants while writing into a
nine-byte array. GCC cannot retain the proof through inlining and reports a
possible out-of-bounds transform. A separate successful-advertisement return
implicitly converts between `std::expected` error types and triggers a
libstdc++ variant maybe-uninitialized warning.

Add a local fail-closed remaining-capacity guard before the section-header
copy and materialize each successful client result without an implicit
cross-error-type `std::expected` conversion. Do not disable either warning,
weaken `-Werror`, or expand accepted protocol input.
Rerun Debug and Release protocol/transport fixtures, chunk-fragmentation and
limit cases, Clang/GCC/sanitizer builds, the tests-disabled composition and
binary/runtime/dependency audits, architecture gates, and
documentation/workflow/diff checks. Resume BPR-0050 only after the remediation
is committed.

## Result

The packfile section-header path now preflights the complete incoming fragment
against the retained nine-byte buffer and checks the retained offset again in
the same loop iteration as every byte write. Malformed capacity cannot partly
advance the parser. Successful discovery, ref-list, and fetch results now
default-initialize the client result's value alternative and move-assign the
parsed value, so GCC never reasons across an inactive variant-backed error
alternative. Warning policy, hardening, packet limits, and accepted protocol
input are unchanged.

## Evidence

- A fresh tests-disabled Ubuntu 26.04 x86_64 Release application compiled and
  linked under GCC 15.2 with `-O3`, `_FORTIFY_SOURCE=3`, and the production
  `-Werror` policy. The original `stringop-overflow` and
  `maybe-uninitialized` diagnostics no longer reproduce.
- The Release executable passed the default and explicit-GHES public
  self-tests with `NATIVE_PLATFORM_VAULT_SELECTED:true`,
  `CLI_UNAVAILABLE_OK`, and exact CLI counts 1 and 2. The target-native binary
  dependency and runtime process/socket audits reported
  `BINARY_DEPENDENCIES_OK` and `RUNTIME_SURFACE_OK`; direct ELF inspection
  confirmed the expected Qt Network/Core and Secret Service dependencies.
- The exact tests-off composition assertions passed against that fresh GCC
  artifact: the compile graph contains neither test macro nor fixture source,
  all 30 source-of-truth forbidden binary strings are absent, concrete
  composition ownership passes, and the render-evidence seam remains disabled.
  The all-in-one GCC CTest wrapper reached its fixed 600-second QEMU timeout
  while recompiling with no diagnostic; it is not counted as a passing CTest.
  The same complete composition contract passed natively on macOS in 35.17
  seconds, including its independent fresh Release build and byte scan.
- The focused protocol boundary, application protocol-v2, native smart
  transport contract, and fragmented native transport fixture passed 4/4
  under macOS Clang Debug (24.74 seconds), Ubuntu GCC Debug (34.49 seconds),
  Ubuntu GCC Release (35.12 seconds), and ASan/UBSan (29.25 seconds). The GCC
  Release run used a separate fresh tests-enabled `-O3` tree, so the optimized
  parser and transport paths executed rather than merely compiling into the
  production binary. These suites retain malformed, fragmented,
  response-limit, cancellation, and sink-abort coverage.
- The core and architecture boundary matrix passed 4/4 under macOS Clang in
  34.78 seconds and Ubuntu GCC in 412.26 seconds. No warning was disabled and
  no hardening or dependency boundary was weakened.
- The focused simplification pass retained the preflight plus same-iteration
  bounds checks: the first prevents partial mutation, while the second makes
  the write proof local for GCC. It also retained the three explicit result
  materializations because folding them through a converting helper would
  recreate the optimizer-sensitive path without clarifying ownership.
- The capability-critical independent review found one blocking evidence gap:
  GCC Release had compiled but not executed the changed parser. The fresh
  tests-enabled GCC Release matrix above closed that gap; the reviewer checked
  its `LastTest.log`, confirmed 4/4, and marked the finding resolved. No code,
  lifetime, bounds, result-propagation, warning-policy, or remaining
  high-risk finding was reported.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`; documentation structure and issue-graph tests pass 2/2;
  tracked and on-disk GitHub Actions workflow counts are both zero; and
  `git diff --check` passes.
