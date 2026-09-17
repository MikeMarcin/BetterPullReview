---
schema: "implement-rfc.card.v1"
id: "BPR-0838"
title: "The anonymous smart-Git acquisition counter is read across threads without synchronization"
status: "In Progress"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-003 BPR-0829 slice","found_at":"c1a795c3ed724eb0086aa4661082793f9643a679","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0838: The anonymous smart-Git acquisition counter is read across threads without synchronization

## Defect

The anonymous smart-Git lease factory counts leases in a plain `std::size_t`
(`Acquisitions`, `src/app/src/github_application.cpp` ~1921). Historical
retention imports acquire leases on a worker thread, and `status()` reads the
count on the Qt thread (~1959, ~13081) for `PublicPreviewSmartAcquisitions`.
That is a data race. [[BPR-0829]] made the course read the count only after
every import finished, which avoids the race in that course but leaves the
counter itself unsynchronized.

## Scope

- Make the counter safe to read from the Qt thread (an atomic, or a snapshot
  through the completion lane), without changing what it counts.
- The file is inside the round-trip contract's pinned scope: reseal with a
  recorded review.

## Acceptance

- The counter is synchronized, `application_anonymous_public_preview_public_path`
  still passes on macOS, and the round-trip contract passes after a reviewed
  reseal.

## Fix (dsp-003, bpr-linux)

Commit `a621ce27a7534c8c4e9ded1b299f2cca25b15ce8` on `linux-lane`. It
changes only `src/app/src/github_application.cpp`.

- `Acquisitions` is now a `std::atomic_size_t`.
- It is still incremented as the first statement of `acquire()`, before the
  recorded-fixture branch and before any refusal. The increment is now
  `fetch_add(1U, std::memory_order_relaxed)`, so the set of counted
  acquisitions is unchanged.
- `acquisitions()` reads it with `load(std::memory_order_relaxed)`.
- `status()` and `PublicPreviewSmartAcquisitions` still take a by-value
  `std::size_t` snapshot.

**Why an atomic, and why relaxed ordering.** Epoch work acquires leases on
worker threads, through `epoch_application_host.cpp` ~4235, ~5786 and
~6282, by way of the window read host. `status()` reads on the Qt thread.
The count is one monotonic value, and nothing compares it with another field
of the snapshot. That leaves no cross-variable ordering for a stronger memory
order to protect. Each `fetch_add` and `load` is atomic whatever the order
argument. A reader that needs every import counted gets that from waiting
for the imports ([[BPR-0829]]), and a relaxed load observes every increment
that happens before it. The anonymous request counters above it in the same
file (`Requests`, `CapabilityProbes`, `CredentialedRefusals`) use the same
ordering. The factory is held by `std::unique_ptr` and never copied or moved,
so the atomic's deleted copy and move operations have no user. A snapshot
through the completion lane was not needed.

**The round-trip contract stays red on `linux-lane`.** The file is inside
its pinned scope. As instructed, this lane does not reseal, and main reseals
with a recorded review. `cmake -DBPR_PRINT_PINNED_DIGESTS=ON
-DBPR_SOURCE_ROOT=/home/mmarcin/bpr-linux -P
tests/end_to_end/application_review_round_trip_composition_contract.cmake`
prints these values for `a621ce27`:

| Pin | Pinned (`df235fa1`) | Printed at `a621ce27` |
|---|---|---|
| `full_application` | `ba30b1bad5c7eadf64305d0638894696d218d7ab52276c855c7e1aa4afcac834` | `4f41706511c2d40b95809247b1d7f92a49971c4e243699f781c71f4c7ea79b98` |
| `production_app_manifest` | `035a2c9b4167b116d0bf3b8143a4a4070ed1ca3776a1d3e500aa6dd68635f8ee` | `c1b3c219d4e84b53d0a2fbc01ea4b687fe5c25f6a6521708bf7cc69d85056a26` |
| `production_mutation_branch` | `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a` | unchanged |

The only change inside the pinned scope since the pins last matched is
`a621ce27`. Its review is recorded below.

## Verification (Linux, bpr-linux)

The runs below are at `a621ce27`, on a clean tree, on JaegAW (WSL2). Logs
are in `/home/mmarcin/bpr-station-logs/task-20260917-lnx-portable-p3/`
(`s3-*`, `build0838.*`, `tidy-*`, `pinned-digests-0838.log`). None is
committed.

- **Build.** `cmake --build --preset dev` on the stock `dev` configure: rc 0,
  47.6 s, 0 warnings. It compiled `github_application.cpp` and `main.cpp`.
- **Gates.** `/usr/bin/clang-format` 22.1.8 `--dry-run --Werror`: clean.
  `git diff --check`: clean. `python3 scripts/verify_documentation.py --root .`:
  `DOCUMENTATION_OK`.
- **clang-tidy.** The station has only clang-tidy 21.1.8.
  `_coding-standard/scripts/tidy.py --paths src/app/src/github_application.cpp
  --warnings-as-errors` reports the same 2 findings before and after the
  change: `portability-avoid-pragma-once` in `dif_runtime.hpp`, and
  `readability-identifier-naming` for `exact_current` (~7767). Both come from
  code this card does not touch. The change adds no finding.
- **Rows.** Each ran as its own serial `ctest --preset dev -j1 -V -R '^<name>$'`.
  The Secret Service `login` collection read `(<false>,)` before each run.
  The 1-minute load average was 0.8 to 1.1.

| Row | Result | Time | Marker |
|---|---|---|---|
| `application_anonymous_public_preview_public_path` run 1 | Pass | 1.66 s | `PUBLIC_PREVIEW_OCCURRENCES:4`, `PUBLIC_PREVIEW_SMART_ACQUISITIONS:4`, `PUBLIC_PREVIEW_ENABLED_OK` |
| run 2 | Pass | 1.67 s | the same three lines |
| run 3 | Pass | 1.67 s | the same three lines |
| run 4 | Pass | 1.67 s | the same three lines |
| run 5 | Pass | 1.64 s | the same three lines |
| `application_epoch_persistence_github-com` | Pass | 3.88 s | `EPOCH_PERSISTENCE_CONTRACT_OK:github.com` |
| `application_epoch_persistence_ghes-3_18` | Pass | 3.89 s | `EPOCH_PERSISTENCE_CONTRACT_OK:ghes-3.18` |
| `application_epoch_portable_occurrence_two_catalogues` | Pass | 12.55 s | `EPOCH_PORTABLE_TWO_CATALOGUES_OK:3` |
| `application_epoch_maintenance_public_path` | Pass | 5.88 s | `EPOCH_MAINTENANCE_PUBLIC_PATH_CONTRACT_OK` |
| `application_epoch_ordinary_update_recreated` | Pass | 4.33 s | `EPOCH_ORDINARY_RECREATED_OK:1` |
| `architecture_boundaries` | Pass | 8.79 s | `Application, provider, and Qt architecture boundaries verified` |
| `application_review_round_trip_composition_contract` | **Fail, expected** | 0.01 s | `full LF-normalized github_application.cpp digest drifted: expected ba30b1ba…, found 4f417065…` |

- The preview course's pass expression requires its whole block in order,
  from `PUBLIC_PREVIEW_COMPOSED:true` to `PUBLIC_PREVIEW_ENABLED_OK`,
  including `PUBLIC_PREVIEW_SMART_ACQUISITIONS:4`.
  `PUBLIC_PREVIEW_WAITING_FOR` was not printed.
- `ctest -N -R 'epoch|public_preview'` names two persistence rows, and both
  ran. The three other application-level epoch courses ran too.
- **Offscreen by registration.** Every application row above runs
  `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`.
- **No TSan run.** No preset builds with ThreadSanitizer. A TSan build would
  need flags beyond the stock preset and a full rebuild, and it would report
  on the uninstrumented Qt. This card's scope does not cover that.
- **Independent review** (rfc-reviewer, read-only, on `a621ce27`): no
  blocking findings. The reviewer confirmed these points:
  - every access to the counter is covered, and the increment point and
    counted set are unchanged;
  - relaxed ordering is justified, because no field is compared with the
    count;
  - no copy or move of the factory exists;
  - `production_mutation_branch` is out of scope and the other two pins
    drift;
  - no architecture-gate pattern matches the change;
  - the change follows the coding standard.

  One nit is not acted on: "the counters above" belong to a different class
  in the same file. The review overlapped preview runs 1 to 5. During
  that time the reviewer ran only read-only `git show`, `grep` and `sed`,
  and the load average stayed near 1.

## Still open

- `application_anonymous_public_preview_public_path` on macOS, which the
  acceptance names.
- The reviewed reseal on main, using the values above, and the round-trip
  contract passing after it.
