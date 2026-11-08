---
schema: "implement-rfc.card.v1"
id: "BPR-1939"
title: "Include the declaration of memcpy in epoch catalogue owners"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"portability","found_by":"v0.4.2 build38 native Ubuntu GCC Release compilation","milestone_plan":{"id":"bpr-1939-epoch-cstring","path":"docs/rfcs/plans/bpr-1939-epoch-cstring/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1631a5306d8192e0f2069e1c69b6e172295b049a","evidence":{"kind":"observation","detail":"The source diff adds exactly three #include <cstring> lines: shared epoch_catalog_support.hpp read_oid, epoch_catalog_append_reservation.cpp read_pack, epoch_catalog_support.cpp read_blob. No behavior, flag, assertion or schema changes."}},{"criterion":"AC2","outcome":"pass","revision":"1631a5306d8192e0f2069e1c69b6e172295b049a","evidence":{"kind":"command","detail":"Native Ubuntu GCC15.2 native-proof-r2/receipt.json: original three failed TUs reproduce missing std::memcpy; all eight corrected TUs compile. Exact Ninja GCC argv preserved except primary source path, external cwd with unchanged copied module mapper and all object/dependency/module/temp outputs external. Corrected bytes match this source revision by SHA256. sourceUnchanged/objectsUnchanged/compileCommandsUnchanged/sourceClean/sourceRevisionUnchanged all true. Original full release failure and first launcher PATH failure retained."}},{"criterion":"AC3","outcome":"pass","revision":"1631a5306d8192e0f2069e1c69b6e172295b049a","evidence":{"kind":"command","detail":"Source revision landed/pushed authoritative main. Existing sqlite_epoch_catalog_storage_contract, sqlite_epoch_catalog_behavior_contract and sqlite_anonymous_epoch_authority pass in isolated macOS build; format22 all3 files and warnings-as-errors tidy22 both implementation TUs/shared header pass. Documentation gate DOCUMENTATION_OK, board valid and diff check clean. Frozen release38 c916a0573 source/accepted outputs untouched; native diagnostic ownership returned idle, no release acceptance claimed."}}]
---

# BPR-1939: Epoch catalogue memcpy declarations

## Acceptance

- AC1. Include `<cstring>` directly where the epoch catalogue's shared header,
  append reservation and support implementation call `std::memcpy`; preserve
  behavior, compiler flags and assertions.
- AC2. Retain native build38's actual GCC failure and obtain native compilation
  controls for the affected implementation boundary: unmodified shadow inputs
  fail at the missing declaration and corrected inputs compile all eight epoch
  catalogue translation units with the original configured flags and external
  object/dependency outputs.
- AC3. Focused existing catalogue checks and formatting/documentation gates
  pass, then land the bounded include correction on authoritative main. Keep
  frozen build38 source and its accepted output bytes unchanged.

## Bounded plan and ownership

The shared support header uses `std::memcpy` in `read_oid` without `<cstring>`.
Append reservation has its own `read_pack` call without a direct include, as
has the support implementation's `read_blob`. Prior platform builds inherited
transitive declarations; native GCC correctly exposed the unsupported include
assumption. This milestone owns direct standard-library includes only; it
changes no schema, storage policy, data representation or runtime operation.
No new test mirrors an include. Native affected-TU compilation and existing
catalogue contracts supply the relevant evidence.

The implementer uses an isolated branch/worktree. The coordinator granted
exclusive native diagnostic ownership only after build38's Linux compile and
full controller stopped at the failed barrier. Any native proof uses an
external shadow implementation directory and redirects every generated output.
No release source, configured flags, producer outputs, package identity or
published asset is changed. Final release compilation and qualification remain
separate coordinator obligations.


## Native correction evidence

The release coordinator retained the real build38 native GCC failure in
external `candidate38/linux-compile-failure/`: the shared-header error was
reported from the main, append-finalization and append-reservation translation
units, with a second reservation-owned `std::memcpy` error. No source or flag
was changed to collect that failure.

The corrected slice adds exactly one `<cstring>` include to each of the three
owners. All three corrected file hashes are bound in external
`candidate38/implementer1939/corrected-inputs.json` and checked by the native
proof before compiling. The native Ubuntu compiler is GCC 15.2.0. The proof
reads each exact command from the configured Ninja graph, retaining `-O3`,
`-DNDEBUG`, C++23, all warning/hardening flags, `-Werror`, definitions and
module/dependency flags. It copies the unchanged `$root .` module mapper into
an external working directory, changes only the primary input source path,
and creates object, dependency, module and temporary outputs there. No forced
include or warning suppression is used.

The three original failing TUs all fail again in an unmodified shadow with
the missing `std::memcpy` declaration. The corrected shadow then compiles all
eight epoch-catalogue TUs successfully: main, append finalization, append
reservation, maintenance operations, open, prune/cache delete, reads and
support. The source directory, original object directory, compile database,
source revision and clean status are unchanged before/after. The raw proof,
commands, source hashes and receipt live in external
`candidate38/implementer1939/native-proof-r2/`.

The first external launcher attempt stopped before a compiler ran because
Ninja was absent from the noninteractive PATH; its failed receipt remains.
The proof uses the exact `CMAKE_MAKE_PROGRAM` from the existing cache instead.
This operational retry changes no product source or compiler flags.

## Focused verification and limits

The isolated macOS build recompiles the affected library and passes the
existing `sqlite_epoch_catalog_storage_contract` (0.47 s),
`sqlite_epoch_catalog_behavior_contract` (12.60 s) and
`sqlite_anonymous_epoch_authority` (0.37 s). Clang-format22 checks all three
include owners. Clang-tidy22 with warnings as errors passes both changed
implementation TUs, including their shared header; no fixes or check exclusions
are used. Documentation and board validation must pass at closeout.

These controls establish the include correction and retain unchanged storage
semantics. They do not grant full release, package, native desktop, vault or
trust acceptance. Frozen build38 remains
`c916a0573c2612c4ca7170bc1073d520feadded5`; the implementer changed neither that
source nor any accepted output. The shared PC has been handed back idle to
the coordinator. Final frozen-source compilation and qualification remain
coordinator obligations.

Source integration: `1631a5306d8192e0f2069e1c69b6e172295b049a` was pushed to authoritative
main after the native and focused checks above passed. The final source diff
contains only the three direct includes; issue/plan records are separate
bookkeeping. No other main advance occurred during this slice.
