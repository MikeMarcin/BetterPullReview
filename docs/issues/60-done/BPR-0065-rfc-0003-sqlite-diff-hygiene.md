---
id: BPR-0065
title: Preserve SQLite provenance through immutable-base diff hygiene
status: Done
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

## Outcome

- Added one character-class attribute that matches only the verified
  `sqlite3.c` and `sqlite3.h` amalgamation inputs and disables only Git's
  `blank-at-eol` diagnostic for those paths.
- Kept both vendored inputs byte-for-byte identical to the verified SQLite
  3.53.4 archive and documented why the path-specific diagnostic exception
  exists beside their provenance.
- A disposable control repository proved ordinary source still reports
  trailing whitespace; `-whitespace`, which would suppress every diagnostic,
  is not used.

## Direct verification

- `git check-attr whitespace -- third_party/sqlite/amalgamation/sqlite3.c
  third_party/sqlite/amalgamation/sqlite3.h` reported `-blank-at-eol` for both
  exact inputs.
- `openssl dgst -sha3-256` reproduced
  `67f423e9ebbbdc473cbc4772c872ee6b89f31fde4ed0279a5c25d5f65c043a16`
  for `sqlite3.c` and
  `89d0de498b1012938bb78dc18b68fea52e26f7f7b6db6347d126a2e58b86a05c`
  for `sqlite3.h`.
- `git diff --check a600ab2c29d2b4088a3f2598dbb817c35af2eafd..HEAD`
  and the working-tree `git diff --check` both passed.
- `python3 scripts/verify_documentation.py --root .` reported
  `DOCUMENTATION_OK`.
- Tracked and on-disk `.github/workflows` counts remained zero.
