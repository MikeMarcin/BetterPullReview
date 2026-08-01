---
id: BPR-0065
title: Preserve SQLite provenance through immutable-base diff hygiene
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0039]
---

# Preserve SQLite provenance through immutable-base diff hygiene

The RFC-BPR-0003 immutable-base `git diff --check` gate reports upstream
blank-at-end-of-line bytes in the verified SQLite amalgamation. Preserve the
exact published SQLite bytes and their recorded digests while teaching Git
that this one diagnostic is not a repository-authored whitespace defect for
the two byte-for-byte amalgamation inputs.

The policy must be path-specific and retain every other whitespace diagnostic.
Record the exception beside the SQLite provenance, verify the exact Git
attribute values, remeasure both vendored digests, pass the immutable-base
`git diff --check`, and keep GitHub Actions absent.
