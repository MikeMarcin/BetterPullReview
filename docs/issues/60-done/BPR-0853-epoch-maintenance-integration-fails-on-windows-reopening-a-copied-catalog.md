---
schema: "implement-rfc.card.v1"
id: "BPR-0853"
title: "epoch_maintenance_integration fails on Windows reopening a copied catalog"
status: "Done"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-0846 new defect 1","found_at":"e6b7193e19862ba3f0d27c3096ba30ddc0ea60cf","run":"run-20260916-stations","dispatch":"dsp-004"}
acceptance: [{"criterion":"AC1 the course passes on Windows 3 runs in a row, still passes on macOS, and the cause is named","outcome":"pass","revision":"4dbb00fd","evidence":{"kind":"command","detail":"Cause named on the card (bpr-win task-20260917-win-0853): three course faults, not product ones: a std::filesystem::copy catalog with inherited-only descriptors that the owner-private store correctly refuses, a text-mode binding file, and a text-mode fixture agent stdout. Test-only fix 6cfb444b; the refusal is unchanged. Windows: before 3/3 fail, after 3/3 consecutive pass. macOS epoch_maintenance_integration passes at 4dbb00fd (build/r2-logs/dsp4-int1-ctest.log)."}}]
---

# BPR-0853: epoch_maintenance_integration fails on Windows reopening a copied catalog

## Defect

`epoch_maintenance_integration` fails on `bpr-win` at `e6b7193e` with
`construct real SQLite catalog`
(`docs/verification/dsp4-windows-confirmation.md`).

The failure is in `verify_real_migrated_retained_refs`
(`tests/git_contract/epoch_maintenance_integration_tests.cpp` ~996). Its
"migration" fixture opens its catalog. The "migrated" fixture then reopens a
copy of that catalog made with `std::filesystem::copy`, and the reopen
returns `epoch_catalog_failure::invalid_permissions` (2).

`aa3c6ef1` is not the cause: `catalog_database.cpp` at `aa3c6ef1^` fails the
same way. The migration course (`72d40b4f`, `776bc0d4`, `90d4ba28`, all
2026-09-12) is newer than the station's last recorded Windows pass
(2026-08-27).

A likely cause, not proven: the copied `state/v1` objects inherit a DACL
wider than the private one, and the adapter refuses them instead of repairing
them ([[BPR-0806]]). macOS and Linux pass the course.

## Scope

- Name the cause on Windows: the course's copy, or the product's refusal of
  a copied catalog.
- If the course is at fault, make it copy with the private DACL the product
  requires. If the product is at fault, fix the product without weakening
  the owner-private refusal. That is storage authority, so give it a review.

## Acceptance

- The course passes on Windows, 3 runs in a row, and still passes on macOS.
  The cause is named here.

## Cause (dsp-004, bpr-win, task-20260917-win-0853)

The course is at fault, not the product. The course has three Windows
faults in a row, and each one hid the next. Each was measured with a
temporary diagnostic that was then reverted.

1. **The copied catalog lacks the store's descriptors.** `icacls` on the two
   trees just before the reopen:

   | Object | Original (store-created) | Copy (`std::filesystem::copy`) |
   | --- | --- | --- |
   | `catalog\state` | `mikem:(OI)(CI)(F)` | `mikem:(I)(OI)(CI)(F)` |
   | `catalog\state\v1` | `mikem:(OI)(CI)(F)` | `mikem:(I)(OI)(CI)(F)` |
   | `state\v1\epoch-catalog.sqlite3` | `mikem:(F)` | `mikem:(I)(F)` |
   | `state\v1\epoch-catalog.lock` | `mikem:(F)` | `mikem:(I)(F)` |

   The copy only inherits. `validate_private_security`
   (`catalog_database.cpp` ~2275) requires an existing directory to have a
   protected DACL with one `(OI)(CI)` owner grant, and an existing store file
   to have one explicit grant. It accepts an inherited grant only while
   admitting a file SQLite has just created ([[BPR-0806]]). The reopen
   therefore returns `invalid_permissions` (2). Giving only the directories
   the store's descriptor was not enough: the course still failed the same
   way on the inherited files. The owner-private refusal is correct and was
   not changed.
2. **The frozen `binding` file was written in text mode.** With the catalog
   fixed, the MCP helper answered `snapshot_unavailable`. The course wrote
   `binding` with a text `std::ofstream`, so on Windows the line ends
   `\r\n`. The product writes this file in binary
   (`github_application.cpp` ~3261), and the helper reads it in binary and
   does not decode the carriage return.
3. **The fixture agent's stdout was in text mode.** With `binding` fixed,
   the agent's single event matched the receipt except for a trailing
   carriage return: `migrated_binding_agent_fixture.cpp` wrote its receipt
   through the CRT's text mode. Every other raw-protocol fixture selects
   binary streams (BPR-0240).

macOS and Linux have none of the three: `copy` keeps the 0700/0600 modes,
and there is no text-mode translation.

## Fix (bpr-win)

Test code only; no product, storage-authority or manifest change.

- `tests/git_contract/epoch_maintenance_integration_tests.cpp`: after the
  recursive copy, every copied catalog object, parents first, gets the
  store's explicit owner-private descriptor through the existing test
  support `bpr::tests::sqlite_contract::make_owner_private(path, directory)`.
  On POSIX it sets 0700/0600, which the copy already carried. `binding` is
  written with `std::ios::binary`.
- `tests/git_contract/migrated_binding_agent_fixture.cpp`: on Windows,
  standard streams are set to `_O_BINARY` first, the same idiom as
  `tests/fixtures/agent/agent_canary_main.cpp`.

## Verification (Windows, bpr-win)

Windows 11 Home 10.0.26200, Visual Studio 18, Qt 6.10.3, Git for Windows
2.55.0.windows.3, `windows-dev` Debug, based on
`156fde6b69e54ab0bd85795cea0de9f467514845`.

- `epoch_maintenance_integration`: failed 3 of 3 before the fix (`construct
  real SQLite catalog`). After the fix it passed 3 runs in a row: 13.23,
  13.25 and 13.13 s.
- `git_haves_trimmed_pack_update` (neighbour): passed, 6.61 s.
- `test_network_isolation_manifest`: passed, 6.32 s.
- `clang-format`: no drift in either file.

## For macOS

Run `epoch_maintenance_integration` once. The only POSIX-visible change is
the `make_owner_private` pass over the copied catalog, which sets 0700 on
directories and 0600 on files.
