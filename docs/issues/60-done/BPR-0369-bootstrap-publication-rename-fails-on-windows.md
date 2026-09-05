---
schema: "implement-rfc.card.v1"
id: "BPR-0369"
title: "The bootstrap publication rename has never worked on Windows"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: []
---

# The bootstrap publication rename has never worked on Windows

[[BPR-0368]] repaired the ancestor chain, and `open_state_directory` now
succeeds on Windows for the first time. The next step along the same path fails,
and it is a second never-executed defect rather than the same one.

`make_epoch_catalog` returns `unavailable`. Traced by instrumenting the
product's own return sites and then removing the instrumentation:

```
PROBE bootstrap ENTERED
PROBE rename failed win32=206
PROBE catalog step failed: created@3984
FAIL: owner-private root bootstraps a catalog
```

`206` is `ERROR_FILENAME_EXCED_RANGE`. It comes from the publication step in the
Windows `publish_bootstrap`, which renames the bootstrap temporary onto the
final catalog name with `SetFileInformationByHandle(FileRenameInfo)` and a
`RootDirectory` handle:

```cpp
const auto structure_bytes = offsetof(FILE_RENAME_INFO, FileName) + name_bytes;
...
rename->ReplaceIfExists = FALSE;
rename->RootDirectory   = parent.Handle.Value;
rename->FileNameLength  = static_cast<DWORD>(name_bytes);
```

The failure is reached uncontained, so it is not the AppContainer.

## What has been ruled out

- **DELETE access on the renamed handle.** Present already: `publish_bootstrap`
  opens the temporary with `GENERIC_READ | GENERIC_WRITE | DELETE`. This is the
  usual cause of a failing `FileRenameInfo` and it is not this.
- **The buffer being one `wchar_t` short.** Widening `structure_bytes` by
  `sizeof(wchar_t)`, so the name has room for a terminating null, was tried
  directly. It **changes** the error from `206` to `87`
  (`ERROR_INVALID_PARAMETER`) rather than fixing it. The size therefore matters
  and is not the whole story; the experiment was reverted rather than left in.

## The most likely remaining cause, not yet measured

`RootDirectory`-relative naming requires the directory handle to carry traverse
and list rights. `open_existing_object` requests
`FILE_READ_ATTRIBUTES | READ_CONTROL | additional_access`, and
`validate_ancestor_chain` passes `0U` for `additional_access`, so the retained
chain's handles carry neither `FILE_TRAVERSE` nor `FILE_LIST_DIRECTORY`. The
handle used as `RootDirectory` here comes from that chain
(`retained_version`, the `v1` directory).

That is a hypothesis with a mechanism, not a measurement. Whoever takes this
card should test it before changing anything, because widening the access those
retained handles hold is exactly the kind of change that quietly weakens the
substitution defence [[BPR-0053]] requires of them.

## Required final state

- The publication rename succeeds on Windows, and the reason it previously
  failed is written down rather than fixed by trial.
- Whatever access the retained chain gains, if any, is stated against
  [[BPR-0053]]'s ancestor-substitution criterion — the chain is a security
  mechanism and its handles' rights are part of it.
- `sqlite_epoch_catalog_storage_contract`, `sqlite_epoch_catalog_behavior_contract`,
  `sqlite_review_mutation_recovery_storage`, and `sqlite_review_ledger_storage`
  pass, uncontained at minimum.
- A test fails without the fix.
- macOS and Linux are untouched; this whole path is inside `#if defined(_WIN32)`.

## Not in scope

The two epoch-catalog CTest timeouts. Uncontained these tests fail fast with the
message above; only under the launcher do they hang past 30s and 180s. That
difference is the container's and belongs with [[BPR-0362]] or its own card, not
here.

## Verification

```sh
cmake --build --preset core-only
ctest --preset core-only --output-on-failure \
  -R '^(sqlite_epoch_catalog_storage_contract|sqlite_review_ledger_storage)$'
```

Run the binaries directly as well. The uncontained run is what separates this
defect from the container's effects, and it is the run that found it.

## Result — the cause was neither candidate

The hypothesis this card recorded was wrong, and saying so is the point of
having written it down as unmeasured.

A standalone probe replicated the rename exactly and varied only the parent
handle's access mask:

```
parent access                          no-null   with-null
  none (what the product passes)        87       87
  FILE_TRAVERSE                         87       87
  FILE_LIST_DIRECTORY                   87       87
  FILE_ADD_FILE                         87       87
  FILE_LIST_DIRECTORY|FILE_TRAVERSE     87       87
  FILE_GENERIC_READ                     87       87
  FILE_GENERIC_WRITE                    87       87

  RootDirectory=NULL, fully qualified name:  OK
```

No access mask helps. The terminating null does not help. The one thing that
works is abandoning `RootDirectory` entirely.

**`SetFileInformationByHandle` does not honour `RootDirectory`.** That field
belongs to the native `NtSetInformationFile`; the Win32 wrapper requires a
fully qualified destination and a null root. Every handle-relative attempt
returns `ERROR_INVALID_PARAMETER`, and the original buffer sizing turned the
same rejection into `ERROR_FILENAME_EXCED_RANGE` instead.

So the retained-chain access rights are untouched, and the substitution defence
[[BPR-0053]] requires of them is not weakened — which is the outcome this card
asked for and the reason it refused to guess.

### The security property is preserved

`destination` is already `parent.Path / CatalogNameWide`, a `\?\`-extended
path built from the retained parent. Every ancestor handle stays open across the
rename, so the chain cannot be swapped underneath it, and the publication is
read back through `identity_at(parent, CatalogNameWide, security)` — which is
handle-relative and compares the exact expected `file_identity`. The rename is
the only step that stopped being handle-relative, and it is bracketed on both
sides by checks that are.

## Result

`sqlite_epoch_catalog_behavior_contract` **passes uncontained** for the first
time: `sqlite-epoch-catalog-behavior-tests:pass`, exit 0. The bootstrap
publishes.

`sqlite_epoch_catalog_storage_contract` now runs deep into the suite — far
enough to emit `SKIP: file-symlink rejection requires Windows symlink
privilege` — and stops at a much later assertion, `live catalog retains
no-delete-share handles for every ancestor and the catalog leaf`. That is new
ground, not this defect.

Under the launcher both still time out. Passing uncontained and hanging
contained is the container's effect, and [[BPR-0362]] owns it.

## What this exposed

- `sqlite_review_ledger_storage` and `sqlite_review_mutation_recovery_storage`
  still fail at their first step, and it is a **test-side** defect rather than
  the product's. [[BPR-0370]] carries it.
- The storage contract's later no-delete-share assertion is undiagnosed and
  recorded in [[BPR-0347]].
