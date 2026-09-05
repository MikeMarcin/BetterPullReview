---
schema: "implement-rfc.card.v1"
id: "BPR-0544"
title: "The composed refresh course lands its second store inside the budget on Windows"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
---

# The composed refresh course lands its second store inside the budget on Windows

## Why

While closing [[BPR-0543]] on the JaegAW Windows station (2026-09-03),
`application_device_authorization_refresh_course` failed in the first run of
the card's selector set and was then placed at the pristine merge head
`8f9d5616` with every BPR-0543 file reverted: it passed once immediately
after a relink and then failed eight of eight. Every failure is the single
marker `DEVICE_FLOW_VAULT_STORES:1` against the required `2`, with
`DEVICE_FLOW_STAGE:authorized`, `DEVICE_FLOW_ROTATIONS:1`,
`DEVICE_FLOW_REFRESH_STATE:scheduled`, and `DEVICE_FLOW_REFRESH_BOUND:true`
all as pinned — the rotation's second store does not land inside
`bpr_self_test_budget_ms`. The course runs over the fixture vault and never
constructs a Credential Manager vault; the fixture data root is a fresh
`QTemporaryDir` per process. The station repaired nothing. This row is one
of the 24 selectors every final-head platform replay of RFC-BPR-0016 runs,
so until it is understood the Windows replay's green on this row is a coin
flip and the ledger's refresh-lifecycle claim cannot quote it for Windows.

## Required final state

- The cause is named with evidence from the Windows host: whether the
  second store is issued late (the course-only clock offset, the timer
  lead, or the refresh transport's rotation), lands late (the fixture
  vault's effect lane), or lands after the evidence snapshot is printed;
  and why a run immediately after a relink differs from the next.
- The course pins its markers at a point that is deterministic on every
  platform — the evidence waits on the second store's completion (or a
  typed refusal) rather than on wall-clock luck — without weakening what
  `DEVICE_FLOW_VAULT_STORES:2` proves; if the budget itself is the cause,
  the bound is raised once with the measured reason stated on the row.
- Ten consecutive runs pass on the Windows station at the closing head,
  and the row still passes on macOS and Linux (the integrator reproduces
  on macOS at integration).
- The card records host, toolchain, the exact commands, and results.

## Non-goals

- Any change to the refresh lifecycle's product behaviour or to the
  markers' meaning.
- The orphan course ([[BPR-0540]]) beyond keeping it green if the shared
  course scaffolding moves.

## Lane — 2026-09-03

Carded by the integrator straight into `21-ready-win/` with no open
dependency: the flake reproduces only on the Windows host so far, and its
diagnosis needs that host. Dispatched to `bpr-win` on `windows-lane`. The
edit will touch `src/app/main.cpp`, which [[BPR-0540]]'s open slice also
edits; the integrator resolves that merge.

## The run finished before the store it was asserting — 2026-09-03

Diagnosed and fixed on `windows-lane` at `4e6b6c89` (`main` `26cd2f35` merged
into the lane). The budget was never the cause, and nothing about the refresh
lifecycle changed: the automated run simply stopped waiting one step too
early.

### Host and toolchain

```text
Windows 11 Home 10.0.26200 (x64), the JaegAW host
Visual Studio 18 Community, MSVC 14.51.36231, x64 Native Tools (vcvars64)
CMake 4.4.2   Python 3.14.7 (py -3)   Qt 6.10.3 (msvc2022_64)
clang-format 22.1.8
```

### The cause, from this host

The automated run's poll had two waits for the device courses and needed
three. The registered course waits for its first store, because that store
"lands after the stage is terminal". The refresh course waited only for
`DeviceAuthorizationRefreshRotations >= 1` and for the state to leave
`refreshing` — never for the second store, which is the very thing
`DEVICE_FLOW_VAULT_STORES:2` asserts.

The rotation counter and `scheduled` are published on the run's own lane the
moment the renewal exchange settles. The new access token reaches the vault
through an off-lane worker. Instrumenting the poll on this host — a temporary
`steady_clock` stamp at the tick where the old predicate first passed, and at
each tick after — measured the gap directly:

```text
BPR0544 rotation-settled at +0ms   stores=1
BPR0544 poll             +0ms      stores=1
BPR0544 poll             +139ms    stores=2      (run 1)
BPR0544 poll             +137ms    stores=2      (run 2)
```

The poll ticks every 10 ms, so the second store lands about fourteen ticks
after the run had already decided it was finished. The old predicate let the
run reach `transition_to_shutdown` on the first of those ticks, and the
evidence was printed with `DEVICE_FLOW_VAULT_STORES:1`. The store was
neither issued late nor lost: it landed late relative to a snapshot that was
taken too early.

**It is not the budget.** Run directly with the registered bound and with one
7.5 times larger, three runs each:

```text
--automated-run-budget-ms 8000    VAULT_STORES:1  wall 6687 / 6633 / 6607 ms
--automated-run-budget-ms 60000   VAULT_STORES:1  wall 6660 / 6581 / 6645 ms
```

Identical answer in identical time: the run was never near its bound, it was
finishing early. So the bound is not raised, and the row keeps the budget it
had.

**Why the run right after a relink differed.** The 10 ms tick that follows
the rotation is the whole race. On the first run of a freshly linked image
the process is slow enough — cold pages, the image scanned once — that the
store can land before that tick; once the image is warm the tick wins and the
count is read at 1. This is the reading the evidence supports rather than a
separate measurement: at pristine `8f9d5616` the row passed once immediately
after a relink and then failed eight consecutive times, which is what a race
decided by first-run slowness looks like.

### The fix

One predicate, in the refresh course's own block in `src/app/main.cpp`. The
run now also waits while `DeviceAuthorizationVaultStores < 2`, exactly as the
registered course waits for its first store, and stops waiting on
`reenrollment_required` — the typed refusal a vault that cannot store takes
the host to — so a genuinely broken store is still reported rather than
waited out to the budget.

Nothing else moved. `DEVICE_FLOW_VAULT_STORES:2` means what it meant; the row
is not weakened, it is waited for. No marker's meaning changed, no product
behaviour changed, and the registered course, the public path and the
lifecycle row are untouched.

`tests/end_to_end/application_review_round_trip_composition_contract.cmake`
repinned in two passes, because the production app source digest covers
`src/app/main.cpp`: `a5a183be…b018` became `cff40cfc…e279`, and the contract
passed on the second pass (16.66 s). A single pin, no cascade.

### Verification

```text
ctest --preset windows-dev -R '^application_device_authorization_refresh_course$'
  ten consecutive runs at the closing head
    run 1 Passed   run 2 Passed   run 3 Passed   run 4 Passed   run 5 Passed
    run 6 Passed   run 7 Passed   run 8 Passed   run 9 Passed   run 10 Passed
```

| Selector | Result | Time |
|---|---|---|
| `application_device_authorization_refresh_course` | Passed, 10 of 10 | ~6.8 s each |
| `application_device_authorization_refresh` | Passed | 0.04 s |
| `application_device_authorization_public_path` | Passed | 1.13 s |
| `application_device_authorization_registered_course` | Passed | 6.32 s |
| `application_review_round_trip_composition_contract` | Passed after the repin | 16.66 s |

```text
cmake --build --preset windows-dev --config Debug     exit 0, no C4456/C4189/C4996
clang-format --dry-run --Werror src/app/main.cpp      exit 0
py -3 scripts/verify_documentation.py --root .        DOCUMENTATION_OK, exit 0
git diff --check                                      exit 0
```

### Claims

| Claim | Command | Result |
|---|---|---|
| The cause is named from evidence on this host | the temporary poll instrumentation above | the second store lands ~138 ms after the old predicate passed, fourteen 10 ms ticks past the run's finish |
| The bound is not the cause | the shipped probe at 8,000 ms and 60,000 ms | identical `VAULT_STORES:1` in identical ~6.6 s; the budget is not raised |
| The evidence waits on the store rather than on wall-clock luck | `src/app/main.cpp`, the refresh course's wait | waits while `VaultStores < 2`, released by `reenrollment_required` as the typed refusal |
| The row is deterministic here | ten consecutive `ctest` runs at the closing head | 10 of 10 passed |
| The sibling device rows still pass | the three selectors above | all Passed |
| Nothing outside the authorized surface moved | `git status` | `src/app/main.cpp`, the composition pin, and this card |

The edit is confined to the refresh course's block so [[BPR-0540]]'s open
slice on the integrator's side merges small, as the dispatch asked. macOS and
Linux are unaffected by construction — the wait is platform-neutral and reads
the same published counters every platform does — and the integrator
reproduces on macOS at integration.
