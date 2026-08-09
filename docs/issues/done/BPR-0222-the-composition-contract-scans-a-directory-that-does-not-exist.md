---
id: BPR-0222
title: The composition contract scans a directory that does not exist
status: Done
rfc: RFC-BPR-0011
depends_on: []
---

# The composition contract scans a directory that does not exist

Found while building [[BPR-0221]], which is a guard against exactly this
failure mode: a check that passes because it read nothing.

## Measured

`tests/end_to_end/composition_contract_tests.py:178-185` scans a fixed list of
roots for concrete-selection violations:

```
src/core
src/application
src/adapters/qt
src/adapters/git
src/adapters/sqlite
src/adapters/ai        <-- does not exist
```

On disk `src/adapters/` contains `agent`, `git`, `github`, `qt`, `sqlite`, and
`syntax`. There is no `ai`. The adapter was renamed to `agent`, and the same
stale name was corrected in `AGENTS.md` on 2026-08-08 — this occurrence was
missed because it is code rather than prose.

The failure is silent. `source_files()` at `:84-90` does
`root.rglob("*")`, and `rglob` on a nonexistent path yields nothing without
raising:

```
>>> pathlib.Path('src/adapters/ai').exists()
False
>>> list(pathlib.Path('src/adapters/ai').rglob('*'))
[]
```

So the scan reports zero violations for that root, forever, and reads as a pass.

## What is actually unscanned

Two adapters, for different reasons:

- **`src/adapters/agent/`** — intended to be covered, named wrongly.
- **`src/adapters/syntax/`** — never listed at all, before or after the rename.

`src/adapters/github` and `src/app` are absent **deliberately**: the former is
where the concrete provider legitimately lives and the latter is the composition
root. Those two must stay excluded. This card is about the other two.

## Required direct evidence

- Repair the list so `src/adapters/agent` and `src/adapters/syntax` are scanned,
  and confirm the suite still passes — if either genuinely contains a concrete
  selection, that is a second finding and gets its own card rather than being
  quietly fixed here.
- Make a missing root **fail loudly**. A list of directory names that silently
  tolerates a typo is the defect; repairing this one entry without repairing
  that property leaves the next rename to rot the same way. Assert each root
  exists before scanning it.
- Show it non-vacuous: introduce a nonexistent root, watch the suite fail;
  remove it, watch it pass. Then plant a concrete-selection token under
  `src/adapters/agent/`, watch the repaired scan catch what it could not catch
  before.

## What must not be done instead

Changing `"src/adapters/ai"` to `"src/adapters/agent"` and stopping. That fixes
today's typo and leaves the mechanism that hid it for as long as the rename has
existed. The existence assertion is the actual deliverable; the corrected name
is incidental.

Deriving the root list by globbing `src/adapters/*`. It would fix the typo class
permanently and simultaneously delete the deliberate exclusions of
`src/adapters/github` and `src/app`, which are load-bearing.

## Fixed, 2026-08-09

`src/adapters/agent` and `src/adapters/syntax` are scanned, and a root that is
not a directory now raises rather than yielding nothing:

```
AssertionError: src/adapters/typo is not a directory, so scanning it would
examine no files and pass. Repair the name rather than the expectation.
```

`src/adapters/github` and `src/app` stay excluded deliberately, and the
repaired scan passes — so neither newly covered adapter was hiding a concrete
selection.

### Both directions

| Probe | Result |
|---|---|
| A root that does not exist | fails, naming the directory |
| `native_github_wire_transport_factory` planted under `src/adapters/agent` | caught by path and pattern |

The second probe is the one that matters: that file could not have been caught
before this card, because the adapter it lives in was not being read.

### This could not be demonstrated until [[BPR-0224]]

The first attempt at both probes reported success. The scan was fixed, the
probes were planted, and `ctest` said `Passed` — because
`application_composition_contract` was registered with
`PASS_REGULAR_EXPRESSION "OK"`, which makes CTest ignore the exit code and
match a substring the suite's own output happens to contain.

So this card's fix was unprovable through the gate it belongs to, and the
attempt to prove it is what found that. BPR-0224 removes the property, repairs
the three tests-disabled Release seam breaks behind it, and is what makes the
table above real rather than assumed.
