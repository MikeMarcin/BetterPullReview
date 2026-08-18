---
id: BPR-0361
title: No symbol inspector reads a Windows PE
status: Ready
rfc: RFC-BPR-0013
depends_on: []
---

# No symbol inspector reads a Windows PE

Two gates prove a claim by reading a binary's symbol table. Neither can read one
on Windows, and they fail in the two different ways that are available.

## `live_mutation_isolation` selects a tool that returns nothing

[[BPR-0235]] made `scripts/verify_live_mutation_isolation.py` stop skipping on
Windows: `symbol_command` now selects `dumpbin /symbols`, then
`llvm-readobj --symbols`, then `objdump -t -C`. That removed the skip, which was
right, but the first candidate does not do the job.

`dumpbin /symbols` reads the **COFF symbol table**. A linked MSVC executable does
not have one — the linker consumes it. Measured on this host against a real
binary from this build:

```
dumpbin /symbols build/core-only/better_pull_review_network_sandbox.exe
  -> File Type: EXECUTABLE IMAGE
     Summary
           1000 .00cfg
           ...
          12000 .text
```

A section summary, and not one symbol. The gate notices, which is the one good
thing here:

```
LIVE_MUTATION_ISOLATION_FAILED: better_pull_review_review_mutation_tests.exe
does not contain review_mutation_coordinator. The instrument is not reading
symbols, so its silence about live_review_mutation_provider means nothing
```

That positive control is doing exactly its job. `POSITIVE_CONTROL_SYMBOL`
exists precisely so an unread binary cannot be mistaken for a clean one, and it
caught this. The forbidden-symbol scan for `live_review_mutation_provider`
therefore proves nothing on Windows today.

## `composed_host_verifier_contract` has no Windows candidate at all

`scripts/verify_composed_hosts.py:582` raises:

```
no supported demangling symbol inspector is available
(llvm-nm, nm plus c++filt/llvm-cxxfilt, or objdump/llvm-objdump)
```

Its candidate list is POSIX-only — `dumpbin` is not in it. Unlike the case above
this is not a wrong selection, it is an absent one, and it fails closed.

## What actually reads an MSVC PE

The symbol names are in the PDB beside the image, not in the image. Candidates,
in the order they should probably be tried:

- `llvm-pdbutil dump --symbols` or `llvm-nm` against the PDB, when the LLVM
  tools that ship with Visual Studio are present;
- `dumpbin /symbols` against the object files or the static library rather than
  the linked executable, which does have a COFF table;
- a linker map, which would require adding `/MAP` to the link and is a build
  change rather than an inspection change.

Whichever is chosen has to answer the same question both gates ask: does this
binary reference this symbol. Reading the `.obj` inputs answers a slightly
different question than reading the image, and that difference has to be stated
rather than glossed, because the claim being defended is "this non-production
binary cannot reach a live mutation".

## Required final state

- Both gates select an inspector that returns a real symbol table for an MSVC
  PE, or they fail closed with a message that names what is missing on this
  host. Neither may pass while reading nothing.
- `verify_live_mutation_isolation.py`'s positive control passes on Windows —
  `review_mutation_coordinator` is found in `review_mutation_tests` — before its
  forbidden-symbol result is treated as meaningful.
- `verify_composed_hosts.py` gains a Windows candidate, and its unit suite gains
  a case that fails without it.
- macOS and Linux selection is unchanged.
- If the chosen instrument reads object files or a PDB rather than the linked
  image, the difference in what is being proven is recorded in the gate and in
  the RFC-BPR-0008 ledger, not left implicit.

## Verification

```sh
ctest --preset core-only --output-on-failure \
  -R '^(live_mutation_isolation|live_mutation_isolation_contract|composed_host_verifier_contract)$'
py -3 tests/scripts/verify_live_mutation_isolation_tests.py
py -3 tests/scripts/verify_composed_hosts_tests.py
```

`dumpbin` is only on `PATH` inside a Visual Studio developer environment. Any
selection that depends on it must state that, because a gate that silently
degrades outside `vcvars64.bat` is the same defect in a new place.
