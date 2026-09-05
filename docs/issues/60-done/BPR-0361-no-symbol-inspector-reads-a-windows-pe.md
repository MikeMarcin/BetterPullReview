---
schema: "implement-rfc.card.v1"
id: "BPR-0361"
title: "The live-mutation guard reads no symbols from a Windows PE"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The live-mutation guard reads no symbols from a Windows PE

[[BPR-0235]] made `scripts/verify_live_mutation_isolation.py` stop skipping on
Windows: `symbol_command` selects `dumpbin /symbols`, then
`llvm-readobj --symbols`, then `objdump -t -C`. Removing the skip was right. The
first candidate does not do the job.

`dumpbin /symbols` reads the **COFF symbol table**, and a linked MSVC executable
does not have one — the linker consumes it. Measured against a real binary from
this build:

```
dumpbin /symbols build/core-only/better_pull_review_network_sandbox.exe
  -> File Type: EXECUTABLE IMAGE
     Summary
           1000 .00cfg
           ...
          12000 .text
```

A section summary, and not one symbol.

The gate caught this itself, which is the one good thing in the story:

```
LIVE_MUTATION_ISOLATION_FAILED: better_pull_review_review_mutation_tests.exe
does not contain review_mutation_coordinator. The instrument is not reading
symbols, so its silence about live_review_mutation_provider means nothing
```

`POSITIVE_CONTROL_SYMBOL` exists precisely so an unread binary cannot be
mistaken for a clean one. Without it the forbidden-symbol scan would have
reported 55 clean binaries on Windows while reading nothing at all.

## Resolution

The objects that composed each binary do carry a COFF table, so those are read
instead. `tests/CMakeLists.txt` writes a second manifest beside the existing
one, keyed by binary:

```
<binary>|<object>|<object>|…
```

built from `$<TARGET_OBJECTS:...>` for the same targets the executable manifest
already lists. It is written on every platform, so the two halves cannot drift
apart unnoticed, and it is keyed by binary rather than flattened because the
claim is that a *non-production* binary carries no live provider — not that the
tree contains none anywhere. That distinction will matter when [[BPR-0142]]
lands a live provider that production is allowed to link.

The verifier grew `--object-manifest` and `inspects_objects()`. Where the linked
image has no table it reads the objects and fails closed in three separate ways:
no object manifest, no objects recorded for a binary, or any object that
`dumpbin` cannot read. A partial table is treated as no table, because silence
about the one translation unit that could not be opened is exactly what the
positive control exists to prevent.

No demangler is needed and none ships with Visual Studio. `dumpbin` prints MSVC
decorated names, and the scan matches the undecorated identifier inside them:
`??0review_mutation_coordinator@application@bpr@@QEAA@…` contains
`review_mutation_coordinator`.

### What this changes about what is proven

Reading the objects is not identical to reading the image. The linker may
discard an unreferenced symbol, so the object set is a **superset** of what the
image contains. For a forbidden-symbol scan that is the safe direction: a
binary whose objects are clean cannot link the provider. It does mean a Windows
failure could in principle name a symbol the linker would have dropped, and that
is a false positive rather than a false negative.

## Result

```
ctest --preset core-only -R '^(live_mutation_isolation|live_mutation_isolation_contract)$'   2/2 Passed
py -3 tests/scripts/verify_live_mutation_isolation_tests.py                                  20 tests, OK   (was 14)

with the object manifest:     LIVE_MUTATION_ISOLATION_OK: 55 non-production binaries carry no
                              live_review_mutation_provider symbol
without it:                   LIVE_MUTATION_ISOLATION_FAILED: this platform has no readable symbol
                              table in a linked image, so the objects must be inspected instead
```

The counterfactual is the evidence: the same command against the same tree fails
closed when the object manifest is withheld, and reports 55 inspected binaries
when it is not.

Six unit cases were added, covering the manifest format, a binary absent from
the object manifest, an unreadable object set, a missing manifest, and a
forbidden symbol found in a decorated MSVC name.

## Split out

This card originally covered `scripts/verify_composed_hosts.py` as well, which
raises `no supported demangling symbol inspector is available` because its
candidate list is POSIX-only. That is the same defect class but not the same
fix: its real gate, `composed_application_hosts`, inspects the composed Qt
application, and the Qt graph has never been run under the AppContainer boundary
at all. It cannot be verified end to end here, so folding it in would mean
closing this card on an unproven half. [[BPR-0364]] carries it.
