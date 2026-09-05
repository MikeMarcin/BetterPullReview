---
schema: "implement-rfc.card.v1"
id: "BPR-0425"
title: "The live catalog's no-delete-share retention fails uncontained on Windows"
status: "Done"
rfcs: ["RFC-BPR-0003"]
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

## Measured and repaired — 2026-08-23

The cause, measured with a per-handle probe rather than inferred: the
retained directory chain holds every ancestor with `FILE_READ_ATTRIBUTES |
READ_CONTROL` only, and a handle carrying only attribute and control
access imposes no sharing restrictions on later opens — Windows enforces a
share mode only between handles that hold data access. So the storage
root's no-delete-share retention was never in force (a `DELETE`-access open
of the root succeeded even at the moment `open_state_directory` returned,
with all nine retained handles alive), while `state` and `v1` — held
through `open_private_directory` with `FILE_LIST_DIRECTORY | FILE_TRAVERSE`
— blocked exactly as designed. The probe read
`root=0 state=1 version=1 catalog=1`.

The product was wrong, and it is fixed where it lies:
`open_state_directory` now re-holds the storage root the way it holds the
root's children, through `open_private_directory` with delete sharing
refused, after the root's descriptor is validated. The intermediate
ancestors keep their attribute-only handles, which is correct — nothing
should hold a restrictive lock on `C:\Users`.

One adversary in the contract also carried a POSIX spelling: the
group-read permission drift has no Windows expression
(`std::filesystem::permissions` toggles the read-only attribute and
nothing else), and the manifest-symlink case needs the symlink privilege
this unelevated host does not hold. Both print the suite's
self-describing `SKIP` lines, the same boundary the file-symlink case
above them already recorded, and both remain on [[BPR-0053]]'s
target-native ledger with it.

### Evidence

| claim | command | result |
|---|---|---|
| the contract passes uncontained on Windows as registered | `ctest --preset core-only -R '^sqlite_epoch_catalog_storage_contract$'` | Passed, 2.2 s (`bpr0425-run-7.log`, 2026-08-23), where every earlier run stopped at the retention assertion |
| the retention is real, not vacuous | the probe that measured `root=0` before the fix is the same open the contract performs; the contract's own conjunction is the check | the contract's assertion |
| macOS and Linux are unchanged | the re-hold is inside the `_WIN32` branch of `open_state_directory`; POSIX `flock`-free retention is untouched | by construction |

Implemented by the integrator; the root re-hold is a storage-boundary
change and is marked for the capability-critical closeout review with
[[BPR-0436]]'s and [[BPR-0437]]'s.

## Independent review — 2026-08-23

Reviewed read-only by `rfc-reviewer` with the session's other
capability-critical changes: confirmed sound, no blocking findings. The
review's non-blocking risks and verification gaps are recorded on
[[BPR-0036]]'s procedure and [[BPR-0053]]'s ledger.
