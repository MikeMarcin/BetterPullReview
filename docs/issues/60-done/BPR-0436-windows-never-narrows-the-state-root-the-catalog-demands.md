---
id: BPR-0436
title: Windows never narrows the state root the catalog demands
status: Done
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

## Implemented and proven through the public path — 2026-08-23

`bpr::adapters::sqlite::make_storage_root_owner_private`
(`src/adapters/sqlite/include/bpr/adapters/sqlite/storage_root.hpp`,
implemented beside `open_state_directory` in `catalog_database.cpp`) applies
the descriptor the adapter writes on every directory it creates —
`private_security_attributes`, so the container entry comes with it — to a
directory the caller already owns, through a handle opened without
following a reparse point and without delete sharing, exactly as the later
validation opens it, and then runs `validate_private_security` over the
result. What is narrowed is what will be checked. On POSIX it is mode
`0700`, verified by reading the status back. It is idempotent, refuses a
missing directory rather than creating it, refuses a reparse point, and
requests `WRITE_DAC` only: the owner is verified against the token user
rather than rewritten, so a root the user does not own is still refused.

Callers: `prepare_application_data_root` in `github_application.cpp`, the
public-path fixture's data root (both the caller-supplied root and the
`QTemporaryDir` one, which is then spelled canonically because the Git
cache accepts a root only in its canonical native spelling), the
maintenance fixture's catalog root, and the restart-recovery tool's
`--write-prepared-crash` root, each where `permissions(owner_all)` used to
stand alone.

One test script had the same gap from the other side.
`application_workbench_composition_contract.cmake` pre-creates
`state/v1/workbench-settings.sqlite3` as a directory and `chmod`s the tree
to `0700`; on Windows `file(CHMOD)` toggles the read-only attribute and
nothing else, so the catalog found a `state` it had not written with an
unprotected, empty DACL and refused it. The script now narrows the four
directories with `icacls /inheritance:r /grant:r "<user>:(OI)(CI)F"`, which
is the same descriptor read back through `Get-Acl`: protected, one rule,
`FullControl`, `ContainerInherit, ObjectInherit`.

The startup exception itself, read once [[BPR-0435]] made a developer build
say it: `STARTUP_FAILED_CAUSE: Git cache or epoch catalog composition
failed`. That message is now two, and the catalog's carries its failure
code, because the third run of this investigation needed the enum value to
tell the root's descriptor from its child's.

### Evidence

| claim | command | result |
|---|---|---|
| the catalog refuses an inherited root, accepts it narrowed, and narrowing is idempotent | `ctest --preset windows-dev -R '^sqlite_storage_root_contract$'` (uncontained, as registered) | Passed |
| the application starts on Windows | `ctest --preset windows-dev -R '^application_public_path$'` | Passed |
| the recovery tool's root opens | `ctest --preset windows-dev -R '^live_review_mutation_restart_recovery$'` | Passed |
| the maintenance fixture's catalog constructs | `ctest --preset windows-dev -R '^epoch_maintenance_integration$'` | "construct real SQLite catalog" passes; the test then fails at "real composition retains and resolves its initial epoch", which is [[BPR-0441]] |
| the fixture-driven application tests start | `ctest --preset windows-dev -R '^(application_github_public_path_github_com\|application_workbench_composition_contract)$'` | No `STARTUP_FAILED`; both reach the public path and end on `PUBLIC_PATH_EPOCH_CORE_STATUS:8` with the budget expired, recorded on [[BPR-0441]] |
| the existing storage contracts are unchanged | `ctest --preset core-only -R '^sqlite_'` | 10 of 11; the one failure is `sqlite_epoch_catalog_storage_contract`'s no-delete-share retention, which is [[BPR-0425]] and predates this card |
| the prepared root carries exactly the one protected entry | `icacls %LOCALAPPDATA%\BetterPullReview\BetterPullReview` after a run | `JAEGAW\mikem:(OI)(CI)(F)`, no inherited entry |

Implemented by the integrator after tracing the refusal from
`open_state_directory`'s root validation to `prepare_application_data_root`;
this is a security-descriptor change on the application's own state root
and is marked for the capability-critical closeout review.
