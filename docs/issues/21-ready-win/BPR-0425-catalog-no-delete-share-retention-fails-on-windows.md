---
id: BPR-0425
title: The live catalog's no-delete-share retention fails uncontained on Windows
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# The live catalog's no-delete-share retention fails uncontained on Windows

`sqlite_epoch_catalog_storage_contract` fails on the Windows host **outside**
the AppContainer, which makes it the one SQLite storage test whose failure is
not the container's:

```
SKIP: file-symlink rejection requires Windows symlink privilege; directory-junction rejection remains covered
FAIL: live catalog retains no-delete-share handles for every ancestor and the catalog leaf
```

[[BPR-0347]] saw the same line on 2026-08-18 and filed it undiagnosed. It is
still undiagnosed; what is new is that it is now isolated. The other three
storage tests pass uncontained on this host at `a74e1df`, and this one runs
deep into its adversary set before stopping here, so the defect is in the
retention check itself or in what Windows does with a no-delete-share handle
on an ancestor — not in the chain, the descriptor, or the publication path
that [[BPR-0368]], [[BPR-0369]], and [[BPR-0370]] repaired.

The `SKIP` is deliberate and self-describing, and it means one [[BPR-0053]]
adversary — file-symlink substitution — is not exercised on an unelevated
host. That belongs on [[BPR-0053]]'s ledger rather than being rediscovered.

## Required final state

- The cause is measured, not inferred: which ancestor or leaf handle the test
  expects to block deletion, what Windows actually returned, and whether the
  product's retained handle lacks `FILE_SHARE_DELETE` as designed.
- If the product is wrong, it is fixed and the test passes; if the test's
  expectation is wrong for NTFS, the narrower truth is written where the
  claim lives rather than the check being weakened silently.
- `sqlite_epoch_catalog_storage_contract` passes uncontained on Windows, or its
  exact remaining failure is recorded on [[BPR-0053]].
- macOS and Linux behaviour is unchanged.

## Verification

```sh
ctest --preset core-only --output-on-failure -R '^sqlite_epoch_catalog_storage_contract$'
```

The test is registered `WINDOWS_UNCONTAINED` on Windows by [[BPR-0362]], so
`ctest` runs it outside the launcher there; no separate negative control is
needed.
