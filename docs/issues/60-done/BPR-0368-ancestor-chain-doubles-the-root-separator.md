---
id: BPR-0368
title: The retained ancestor chain doubles the root separator and can never match
status: Done
rfc: RFC-BPR-0003
depends_on: []
---

# The retained ancestor chain doubles the root separator and can never match

`open_state_directory` has never worked on Windows. Not "fails under the
AppContainer" — never, on any Windows host, contained or not. The epoch catalog,
the review ledger, the queue enrollment store, and the appearance settings store
all open through it.

`validate_ancestor_chain` walks from the volume root down to the target, opening
each ancestor and comparing the handle's own normalized name against the path it
asked for. That comparison is the anti-substitution defence [[BPR-0053]]
requires. It built the wrong string:

```cpp
auto        candidate = std::wstring{ native.substr(0U, 7U) };   // "\\?\C:\"
std::size_t offset    = 7U;
...
candidate.append(native.substr(offset - 1U, end - offset + 1U)); // "\Users"
```

The first candidate is `\\?\C:\`, seven characters, already ending in the
separator. The first append starts at `offset - 1`, which is that same
separator, so the result is `\\?\C:\\Users`. Every later iteration is correct —
only the root is doubled — and one doubled character is enough.

`\\?\` paths are deliberately **not** normalized by Win32, so the doubling
survives to `CreateFileW`, which still opens the directory.
`GetFinalPathNameByHandleW` then returns the clean `\\?\C:\Users`, and
`same_windows_path` compares lengths before anything else:

```cpp
if (left.size() != right.size() ...) { return false; }
```

53 characters against 54. `path_matches_handle` returns `invalid_path`, the
chain fails at its first step, and no state directory ever opens.

## Measured, with the counterfactual

The product's own steps, instrumented temporarily and then removed:

| | first failing step | `make_epoch_catalog` |
|---|---|---|
| without the fix | `validate_ancestor_chain` | `failure=1` (`invalid_path`) |
| with the fix | none — the chain, the security descriptor, and the filesystem check all pass | `failure=3` (`unavailable`) |

A standalone probe, compiled against the suite's own
`platform_test_support.hpp`, shows the same boundary and then confirms every
descriptor check the product makes:

```
ok    make_owner_private
ok    is_owner_private
extended: \\?\C:\Users\mikem\AppData\Local\Temp\bpr-state-probe
ok    ancestor chain
ok    owner == token user
ok    SE_DACL_PRESENT
ok    SE_DACL_PROTECTED
ok    AceCount == 1
ok    ace flags exact          actual 3 expected 3
ok    ace mask FILE_ALL_ACCESS actual 2032127
ok    ace sid == token user
```

Before the fix the same probe printed
`\\?\C:\\Users -> \\?\C:\Users` at every level.

## Resolution

Append the separator only when the candidate does not already end in one, and
copy the component without it. One doubled character was the whole defect;
nothing about the walk, the retained handles, or the comparison changes.

## What this does not fix

`make_epoch_catalog` still fails, now with `unavailable` from a step **after**
`open_state_directory` returns successfully. That is a different defect and it
gets its own card rather than being folded in here; the four SQLite storage
tests stay red until it lands.

Two earlier claims are corrected by this card:

- [[BPR-0362]] said the four SQLite storage tests fail because the container
  cannot read path ancestors. They fail uncontained too, where `C:\` opens
  normally. The container is not why.
- [[BPR-0360]] inherited the same assumption about `open_state_directory` never
  reaching the descriptor. It does reach it now, and every descriptor check
  passes uncontained.

## Verification

```sh
cmake --build --preset core-only
ctest --preset core-only --output-on-failure \
  -R '^(sqlite_epoch_catalog_storage_contract|sqlite_review_ledger_storage)$'
```

Run the test binaries directly as well as through the launcher. The uncontained
run is what separates this defect from the container's own effects, and it is
the run that found it.
