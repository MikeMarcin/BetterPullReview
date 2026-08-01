---
id: BPR-0064
title: Make protocol-v2 parser bounds explicit in GCC Release
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0043, BPR-0057]
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
copy and construct the successful client result explicitly in place. Do not
disable either warning, weaken `-Werror`, or expand accepted protocol input.
Rerun Debug and Release protocol/transport fixtures, chunk-fragmentation and
limit cases, Clang/GCC/sanitizer builds, the tests-disabled composition and
binary/runtime/dependency audits, architecture gates, and
documentation/workflow/diff checks. Resume BPR-0050 only after the remediation
is committed.
