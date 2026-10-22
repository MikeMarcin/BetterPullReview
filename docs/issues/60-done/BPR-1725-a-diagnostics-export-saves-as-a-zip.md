---
schema: "implement-rfc.card.v1"
id: "BPR-1725"
title: "A diagnostics export saves as a zip"
status: "Done"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1722"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1725--BPR-1725--a4475b8a84b8.json","origin":"Owner, 2026-10-21, through the coordinator: asked whether to add zlib's compression half so the diagnostics export can be saved as a .zip, the owner said \"yes\".","dependency_approval":"Owner, 2026-10-21: zlib 1.3.2 raw deflate (deflate.c, trees.c from the pinned archive) for the diagnostics export's .zip; recorded in the core implementation inventory.","zip_writer":"No ZIP library: LibArchive is a macOS-only update-staging reader. The core's existing minimal writer (write_cycle_zip, BPR-1160) gains deflate, time and mode options and writes one entry: local file header, central directory entry, end of central directory.","related_cards":["BPR-1160","BPR-1435","BPR-1717","BPR-1722"]}
acceptance: [{"criterion":"AC1: zlib's deflate comes from the same pinned archive under the same licence, and the inventory, SPDX input, notices and packaging docs cover it","outcome":"pass","revision":"fcb3de4a7","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_dependency_boundary|core_boundary_contract|architecture_boundaries|architecture_boundary_contract|package_runtime_contract|binary_dependency_audit_contract|dependency_modernization_contract|review_cycle_dependency_closure)$'","detail":"BprZlibProvider.cmake adds deflate.c and trees.c to the target it builds from the SHA-256- and size-verified, freshly extracted zlib 1.3.2 archive with Z_PREFIX; the licence digest check is unchanged and both files carry upstream's zlib licence header. dependencies.json keeps licence Zlib, URL and digest and names raw deflate in its linkage (the SPDX sourceInfo); zlib/LICENSE stays in the installed notices; the corresponding-source asset carries the same pinned archive. The inventory, PROVENANCE.md and the build reference record the owner's 2026-10-21 approval and the one use. All eight contracts pass, including dependency_modernization_contract's offline Qt-free configure."}},{"criterion":"AC2: the saved file is a standard single-entry ZIP that the platform's archive tools open","outcome":"pass","revision":"fcb3de4a7","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_release_maintenance|core_review_cycle_bundle|core_review_cycle_bundle_pack_oracle|review_cycle_bundle|application_release_maintenance|sqlite_release_maintenance_files|qt_release_maintenance_scale_(100|125|200)|application_release_maintenance_public_path)$'","detail":"core_release_maintenance parses the archive field by field: version 2.0, flag 0x0800, method 8, CRC-32 and both sizes agree between local header and central directory, one end record places the directory and ends the file; 123456789 is stored with CRC-32 CBF43926. The public path course saves diagnostic-é.zip through Settings; unzip -t reports no errors, unzip -Z1 lists exactly diagnostic-é.txt, unzip -p and ditto -x -k (Archive Utility's extractor) give exactly the preview, mode 0600. Windows Explorer was not observed."}},{"criterion":"AC3: the save suggests a dated .zip, the entry keeps the .txt name, every policy and scope is compressed, and the text and preview are unchanged","outcome":"pass","revision":"fcb3de4a7","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_release_maintenance|core_review_cycle_bundle|core_review_cycle_bundle_pack_oracle|review_cycle_bundle|application_release_maintenance|sqlite_release_maintenance_files|qt_release_maintenance_scale_(100|125|200)|application_release_maintenance_public_path)$'","detail":"The panel suggests BetterPullReview-diagnostics-<date>-<HHmm>-9.8.7.zip with filter Diagnostic export (*.zip) and suffix zip. The entry is the file's name with .zip replaced by .txt; refused names give BetterPullReview-diagnostics.txt. Anonymized, identifying, metadata and current-session (ring) exports all save as archives whose entry equals the frozen preview, header and labels included; the preview still shows the text."}},{"criterion":"AC4: round trip, structure and size tests, each failing before the change","outcome":"pass","revision":"fcb3de4a7","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_release_maintenance|core_review_cycle_bundle|core_review_cycle_bundle_pack_oracle|review_cycle_bundle|application_release_maintenance|sqlite_release_maintenance_files|qt_release_maintenance_scale_(100|125|200)|application_release_maintenance_public_path)$'","detail":"Round trip through zlib's raw inflate and read_cycle_zip; structure as AC2; size: a typical 316-record export (284,366 B) archives to 8,485 B (33:1), a full 16,384-record export (14,737,504 B) to 369,206 B; the test requires 10:1. Before (src/ and cmake/ restored): the SQLite, panel and public path suites fail at their new expectations, the core and application suites do not compile. Controls: the old writer with only the .zip name fails the public path at the archive's entry; stored-only fails the core suite at 'the entry is deflated' and the application suite at 'compresses at least ten times'."}},{"criterion":"AC5: gates","outcome":"pass","revision":"fcb3de4a7","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <10 changed translation units>; ctest --preset core-only; python3 scripts/verify_documentation.py --root .","detail":"clang-format 22.1.8 (coding-format-check, 1,731 files) and clang-tidy 22 with warnings as errors on the 10 changed translation units pass. The focused suites (10) and the boundary, packaging and dependency contracts (8) pass in dev. core-only (offline, -g0): 308 of 313 pass on the first run. The three documentation tests need the MASK-managed .agents tree, which a worktree lacks; with a temporary link, removed after the run, documentation_issue_graph_contract and issue_id_allocation_contract pass and documentation_structure reports only this card's provisional ID. application_epoch_host failed once under -j8 at 'prepared opens perform no network read' and passes three runs alone. sqlite_review_mutation_recovery_storage fails at 'v3 migrates transactionally without altering native row': the store now migrates to user_version 7 and the test expects 6; neither file is touched here. Both reseal checks report nothing to reseal; no src/app file changed. verify_documentation_tests passes 67. verify_documentation, with the temporary link, reports only the card's provisional ID format, which allocation settles (before the rebase it also reported NEXT_ID BPR-1724 behind the shared high water BPR-1725, which main's BPR-1724 allocation settled)."}}]
---
# BPR-1725: A diagnostics export saves as a zip

The owner, 2026-10-21, through the coordinator, asked whether zlib's
compression half could be added so the diagnostics export saves as a `.zip`,
and said "yes". That is the dependency approval `AGENTS.md` requires for a new
use of a core library. [[BPR-1722]] had left the export an uncompressed
`.txt` of up to 16 MiB because the pinned zlib was built "no deflate".

## Acceptance

- **AC1, deflate admitted:** zlib's deflate sources come from the same pinned,
  verified 1.3.2 archive, under the same licence, and the inventory, SPDX
  input, notices and packaging docs say so.
- **AC2, a standard ZIP:** the saved file is a single-entry ZIP: the `.txt`
  export, DEFLATE-compressed, with correct CRC-32 and sizes and a UTF-8 name,
  that the platform's own archive tools open.
- **AC3, name and save:** the save dialog suggests
  `BetterPullReview-diagnostics-<date>-<time>-<version>.zip`, the entry keeps
  the `.txt` name, "Current session only" and the metadata export are
  compressed too, the text inside is unchanged, and the preview still shows
  the text.
- **AC4, tests:** a round trip, a structural check (with `unzip -t` and
  `ditto -x -k`) and a size check, each failing before the change.
- **AC5, gates:** focused suites, core-only, clang-format 22, clang-tidy and
  the documentation gate pass.

## Dependency approval

- **Owner approval.** 2026-10-21, through the coordinator: asked whether to
  add zlib's compression half so the diagnostics export can be saved as a
  `.zip`, the owner said "yes". Recorded in the
  [core implementation inventory](../../architecture/system-architecture.md#core-implementation-inventory)
  with that date.
- **What was added.** `cmake/BprZlibProvider.cmake` now also compiles
  `deflate.c` and `trees.c`. Both come from the archive it already pins and
  verifies (SHA-256 `bb329a0a…`, 1,502,830 bytes, re-extracted on every
  configure, source substitution refused) and need nothing else: `deflate.h`
  and `trees.h` are in the same tree, and the checksums they call (`adler32`,
  `crc32`) were already built. `Z_PREFIX` and `ZLIB_CONST` are unchanged, so
  the new symbols are `z_deflate*` and cannot meet the system zlib LibArchive
  links. No gz* file I/O, `compress`/`uncompress` helper or upstream build is
  added, and `review_cycle_zip_codec.cpp` is still the only file that includes
  `zlib.h`.
- **Licence coverage.** Both files carry upstream's own "Copyright (C)
  1995-2026 Jean-loup Gailly (and Mark Adler)" header and point to the zlib
  licence in `zlib.h`, the licence already pinned by digest
  (`e32ff4e0…`) and installed as `zlib/LICENSE` in `THIRD-PARTY-NOTICES`
  (`packaging/package_runtime.py`). The SPDX package for zlib comes from
  `packaging/dependencies.json`, whose licence (`Zlib`), version, URL and
  digest are unchanged; its `linkage`, which becomes the SPDX `sourceInfo`,
  now names raw deflate and the export's ZIP writer. The corresponding-source
  asset ships the same pinned archive. `third_party/zlib/PROVENANCE.md`, the
  build reference's "Core zlib" section and the release maintenance
  architecture say what is compiled and why. The packaging maintainers'
  component lists already name zlib and needed no change.

## The ZIP writer

- **No ZIP library.** LibArchive is pinned only for macOS update staging, built
  with only its ZIP reader enabled and linked only by the macOS update adapter;
  its writer is neither approved nor built, and it does not exist on Windows or
  Linux builds. No other archive library is approved.
- **A minimal writer, already in core.** The core has written review bundles
  with `write_cycle_zip` since [[BPR-1160]]: local file headers, central
  directory entries and one end record, stored members, fixed time, UTF-8
  names. It gains `cycle_zip_write_options` (deflate members, an MS-DOS time
  and date, the Unix mode); its defaults still write review bundles byte for
  byte as before. The diagnostics export uses it for a single entry, so this
  is the minimal single-entry local-file-header, central-directory and
  end-of-central-directory writer over zlib, not a new container
  implementation. No data descriptor, extra field, comment, encryption, ZIP64
  or multi-disk layout is written; a 16 MiB export fits the 32-bit fields.
- **What it writes.** `core::diagnostic_export_archive(file name, text, local
  time)`: version needed 2.0, flag 0x0800 (UTF-8 name), method 8 (raw deflate
  at zlib's level 6, or stored when deflate would not shrink the text), the
  text's CRC-32 (zlib's `crc32_z`), the compressed and uncompressed sizes in
  both the local header and the central directory, the local time to two
  seconds, made by Unix with mode 0600, as the saved file and the earlier
  `.txt` had, so an extracted copy stays owner-only. Equal inputs give equal
  bytes.
- **Layers.** The text is decided where it was: the application's
  classification, anonymisation, header and scope, frozen in the preview and
  digest-checked by the host before saving. Core owns the pure bytes of the
  archive and the entry name. The file adapter's output
  (`release_maintenance_files`, both POSIX and Windows) turns the frozen text
  into the archive at write time, on the effect lane, from the chosen file's
  name and its own local clock, then writes it through the same private
  temporary file, link or rename, and refusal of an existing destination as
  before. Qt only changed its filter and suggestion. No `src/app` file
  changed.

## Name and save

- The save dialog's filter is "Diagnostic export (*.zip)" and its default
  suffix `zip`; `core::diagnostic_export_file_name` now ends in `.zip`, e.g.
  `BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.zip`.
- The entry is the chosen file's name with a final `.zip`, in any case,
  replaced by `.txt` (`core::diagnostic_export_entry_name`), so the suggested
  file holds `BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.txt` and a
  file renamed `Bug 42.zip` holds `Bug 42.txt`. A name without `.zip` gains
  `.txt` once. A name that is empty, over 255 bytes, not UTF-8 or holds a
  separator or control character gives `BetterPullReview-diagnostics.txt`.
- Every policy and scope is archived the same way: anonymized, identifying,
  metadata-only and "Current session only". The archived text is byte for
  byte the preview, so the header (`exported_at`, `app_version`, `app_build`,
  the counts) and the anonymised labels are unchanged inside it. The preview
  still shows that text.

## Compression

Measured by `application_release_maintenance` on prepared exports of a
working session's mix (pull request pages, Home refreshes, review endings,
capability failures; times and durations varying):

| Export | Records | Text | Archive | Ratio |
|---|---|---|---|---|
| Anonymized | 316 | 284,366 B | 8,485 B | 33:1 |
| Identifying | 316 | 291,003 B | 8,634 B | 33:1 |
| Metadata only | 316 | 15,131 B | 434 B | 34:1 |
| Full anonymized | 16,384 | 14,737,504 B | 369,206 B | 40:1 |

The owner's hand-zipped 256 KiB exports came to about 8 KB, the same ratio. A
full export now fits well inside any attachment limit.

## Change

- `cmake/BprZlibProvider.cmake`: `deflate.c` and `trees.c`.
- `core`: `zip_codec::deflate_raw`; `cycle_zip_write_options` and
  `write_cycle_zip`'s deflate, time and mode; `diagnostic_export_entry_name`,
  `diagnostic_export_archive`, `diagnostic_export_local_time::Second`, and
  `diagnostic_export_file_name` ending in `.zip`.
- SQLite maintenance adapter: both outputs write the archive.
- Qt panel: the `.zip` filter and suffix.
- Docs: the inventory's zlib entry, `third_party/zlib/PROVENANCE.md`,
  `packaging/dependencies.json`, the build reference,
  [`release-maintenance.md`](../../architecture/release-maintenance.md), the
  product contract and the storage guide.

## Evidence

Implementation `fcb3de4a7`, on local main `07a9594c3`; macOS 26.6.2 (Apple M4
Pro), 2026-10-21, `dev` built with `-g0` and the main checkout's CPM cache,
offline.

- **Rebase.** Verified first as `041843ddf` on `8cdbace3a`; after rebasing
  onto `07a9594c3` (other cards' Qt gesture and documentation changes, none in
  these paths) the 10 focused suites and both reseal checks were run again and
  pass.
- **Before.** With `src/` and `cmake/` restored to their pre-change state and
  the new tests built against them:
  - `sqlite_release_maintenance_files` fails at "public host exports exactly
    preview bytes as the archive's one entry" (the file was the text);
  - `qt_release_maintenance_scale_100` fails at "the suggestion holds the local
    date, time and version" (it was `.txt`);
  - `application_release_maintenance_public_path` fails at "chooser suggests a
    dated filename";
  - `core_release_maintenance` and `application_release_maintenance` do not
    compile: `diagnostic_export_archive`, `diagnostic_export_entry_name` and
    `diagnostic_export_local_time::Second` did not exist.
  - Controls on the old writer with only the `.zip` name put in: the panel
    fails at "the save offers a .zip archive" and the public path at "the
    archive's entry equals the exact frozen preview".
  - Control on the new code with deflate turned off (stored only):
    `core_release_maintenance` fails at "the entry is deflated, unencrypted,
    with a UTF-8 name and its sizes in the header" and
    `application_release_maintenance` at "and compresses at least ten times".
- **After.**
  - Round trip: `core_release_maintenance` inflates the entry's data with
    zlib's raw inflate and reads the archive with `read_cycle_zip`, both
    byte-equal to the text; `application_release_maintenance` does the same
    for prepared anonymized, identifying, metadata and full exports;
    `sqlite_release_maintenance_files` and the public path read back the saved
    file.
  - Structure: `core_release_maintenance` parses the archive field by field:
    local header, central directory entry and end record agree on version,
    flags, method, time, date, CRC, sizes and name; the end record counts one
    entry, places the directory and ends the file. `123456789` is stored with
    CRC-32 `CBF43926`, the standard check value; a UTF-8 name is kept; an
    unrepresentable time gives 1980-01-01.
  - Native tools: the public path course saves `diagnostic-é.zip` through the
    real Settings dialog, then Info-ZIP `unzip -t` ("No errors detected"),
    `unzip -Z1` (exactly `diagnostic-é.txt`), `unzip -p` (exactly the preview)
    and `ditto -x -k`, the extractor behind Archive Utility (exactly one
    `diagnostic-é.txt`, the preview, mode 0600). UnZip 6.00 (Apple).
  - Size: the table above; the test requires at least 10:1.
- **Gates.**
  - `coding-format-check` (clang-format 22.1.8, 1,731 files) and clang-tidy 22
    with warnings as errors on the 10 changed translation units pass.
  - In `dev`, the 10 focused suites and the 8 boundary, packaging and
    dependency contracts of AC1 pass.
  - `core-only` (offline, `-g0`): 308 of 313 pass on the first run. The three
    documentation tests need the MASK-managed `.agents` tree, which a worktree
    lacks; with a temporary link, removed after the run,
    `documentation_issue_graph_contract` and `issue_id_allocation_contract`
    pass and `documentation_structure` reports only this card's provisional
    ID. `application_epoch_host` failed once under `-j8` at "prepared opens
    perform no network read" and passes three runs alone.
    `sqlite_review_mutation_recovery_storage` fails at "v3 migrates
    transactionally without altering native row": the store now migrates to
    `user_version` 7 and the test expects 6; neither file is touched here.
  - Both reseal checks report nothing to reseal; no `src/app` file changed.
  - `verify_documentation_tests.py` passes 67 tests. `verify_documentation.py`,
    with the temporary link, reports only the card's provisional ID format,
    which allocation settles. Before the rebase it also reported `NEXT_ID`
    BPR-1724 behind the shared high water BPR-1725, which main's BPR-1724
    allocation has since settled.

## Pending and limits

- **Windows Explorer** was not observed: no Windows machine was used. The
  archive uses only what Explorer's extractor reads (version 2.0, deflate,
  32-bit sizes, no extra fields or data descriptor). The suggested name is
  ASCII; a non-ASCII name a user types relies on Explorer honouring the UTF-8
  flag.
- **Archive Utility itself** was not driven (the owner's desktop is not
  touched); `ditto -x -k` is its command-line counterpart.
