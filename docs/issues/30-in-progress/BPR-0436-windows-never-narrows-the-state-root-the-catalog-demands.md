---
id: BPR-0436
title: Windows never narrows the state root the catalog demands
status: In Progress
rfc: RFC-BPR-0003
depends_on: []
---

# Windows never narrows the state root the catalog demands

`open_state_directory` (`src/adapters/sqlite/src/catalog_database.cpp`)
accepts a root only if the root itself is owner-private: owner is the token
user, DACL present and protected, exactly one access-allowed entry — two
inside an AppContainer since [[BPR-0360]]. That is the right demand; it is
what keeps another local principal out of the catalog, the ledger, the queue
store and the appearance store.

On macOS and Linux the demand is met by the caller. `prepare_application_data_root`
(`src/app/src/github_application.cpp:762-780`) creates the application's data
root and applies `std::filesystem::permissions(owner_all, replace)`, which
is mode `0700`. On Windows that call changes nothing a DACL can see: the root
under `%LOCALAPPDATA%` keeps the entries it inherited — `SYSTEM`,
`Administrators`, the user — and the product refuses its own directory with
`invalid_permissions`. The exception surfaces in `main` as `STARTUP_FAILED`
([[BPR-0435]]), in every mode, on every run, before any window.

[[BPR-0370]] met the same fact in the suite's fixtures ("three fixtures
narrowed their roots only on POSIX, so Windows roots were never
owner-private") and fixed the fixtures. The product has the same gap for the
one root only it can prepare, and two tests that hand the product a root of
their own have it too: `epoch_maintenance_integration` ("construct real
SQLite catalog", a `temp_directory_path()` child narrowed with
`permissions`) and `live_review_mutation_restart_recovery` ("persistent
recovery store opens for exact scope", a `contained_temporary_directory`).

## Required final state

- On Windows the product narrows the application data root it prepares to
  exactly the descriptor `open_state_directory` requires — token-user owner,
  protected DACL, the one user entry with the directory inheritance flags,
  plus the container entry when contained — using the adapter's own
  definition of owner-private rather than a second copy of the rule.
- The narrowing is idempotent and refuses nothing the user already owns; a
  root the user does not own is still refused by the product, as today.
- macOS and Linux behaviour is unchanged: `0700` remains what it was.
- The two tests above narrow the roots they own through the same
  definition.
- A test fails without the change: the application's own `--self-test` on
  Windows, which today prints `STARTUP_FAILED`.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^(application_public_path|epoch_maintenance_integration|live_review_mutation_restart_recovery)$'
```

plus `icacls` on the prepared root, read back, showing the one protected
entry.
