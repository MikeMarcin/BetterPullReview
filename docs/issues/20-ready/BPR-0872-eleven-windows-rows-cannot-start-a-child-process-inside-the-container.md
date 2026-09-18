---
schema: "implement-rfc.card.v1"
id: "BPR-0872"
title: "Windows: eleven contained rows fail because they cannot start a child process"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs, the run's first full Windows sweep","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0872: Windows: eleven contained rows fail because they cannot start a child process

## Defect

Eleven rows fail on the Windows station, all sharing one measured cause: a row
running inside the AppContainer cannot start the child process it needs.

- `issue_id_allocation_contract`
- `github_recorded_fixture_contract`
- `large_review_fixture_contract`
- `attended_review_verdict_contract`
- `project_source_dump_contract`
- `dif_assets_contract`
- `rfc0017_public_preview_decision_contract`
- `test_network_isolation_child`
- `rfc0016_relocated_operator_contract`
- `rfc0016_production_identity_security`
- `application_agent_finding_protocol`

Row-by-row first failing lines are in
`docs/verification/dsp4-windows-merged-head-sweep.md`.

This is the run's **first full Windows sweep**, so these are newly *observed*
rather than newly broken. Nothing in this run's changes touches them. Their
age is unknown and worth establishing before anyone treats them as a
regression.

Two related observations from the same sweep suggest the container's process
and filesystem boundary is the common theme rather than eleven separate
defects:

- `git rev-parse` run inside the container exits 128 with "fatal: Unable to
  read current working directory: Permission denied" ([[BPR-0873]]).
- Seeding executables exit 3, this station's known contained abort for
  `std::filesystem::canonical` ([[BPR-0833]], [[BPR-0845]], and
  [[BPR-0876]]).

## Scope

- Establish for each row whether it is meant to run contained at all. A row
  that needs a child process, a git invocation or the real filesystem may
  simply want `WINDOWS_UNCONTAINED`, which is an existing, deliberate
  registration and not a weakening — several rows already carry it for exactly
  this reason.
- Where a row genuinely should stay contained, name what it needs from the
  container and whether that is grantable.
- Say how old these failures are: check the same rows at an earlier commit so
  nobody later reads this card as "the September merges broke Windows".
- Group the answer. Eleven rows with one cause should take one decision, not
  eleven.

## Acceptance

- Every row above either passes on Windows or carries a recorded, reasoned
  registration change, with the reason on this card.
- The card states whether the failures predate this run.
- macOS and Linux are unaffected: all eleven pass there today and must
  continue to.

## What `WINDOWS_UNCONTAINED` actually costs (r5 critical review)

This card's central question is which rows should run contained at all, so the
price of the alternative belongs on it.

`cmake/WindowsTestIsolationSetup.cmake:140` grants the AppContainer
`(OI)(CI)(M)` on the whole `CMAKE_BINARY_DIR`, and
`cmake/BprTestIsolation.cmake:698-704` drops the launcher entirely for the
forty-odd `WINDOWS_UNCONTAINED` rows. So for those rows the effective boundary
is **the user's own token, with no filesystem or network restriction**, and
the socket-level `--network-denied` refusal (`src/app/main.cpp:7800`) is the
only stand-in.

That is a documented limitation rather than a defect — the manifest gate pins
the uncontained set (`scripts/verify_test_network_isolation.py:69-270`), so it
cannot grow quietly. But it means "register it uncontained" is a real
reduction in isolation and not a free move, and this card should say so when
it decides each row.

## The containment rule, and why this group is the anchor for it (dsp-006)

Five Windows cards were asking one question in five shapes. A read-only scout
answered it against the code; this is the record, and [[BPR-0873]],
[[BPR-0874]], [[BPR-0876]] and [[BPR-0877]] point here rather than repeat it.

**What containment actually is.** One fixed Windows AppContainer
(`BprAutomatedTest`) entered with **zero capabilities**, applied as a command
prefix at CTest registration: `cmake/BprTestIsolation.cmake:614`
(`bpr_add_network_isolated_test`), prefix chosen at `:676-700`, backend
selected at `:486-517`, launcher `tests/network_isolation/windows_network_sandbox.cpp`.
Configure-time grants are only `(OI)(CI)(RX)` on the source tree and the Qt
prefix and `(OI)(CI)(M)` on the build tree
(`cmake/WindowsTestIsolationSetup.cmake:110-149`).

It restricts three things, and the third is the one that matters here:

- **network** — no `internetClient`, so WFP refuses every external connect;
- **filesystem** by ACL — `%TEMP%`, the `.mask` store and the user profile are
  all outside the three grants;
- **the kernel-object namespace** — `\GLOBAL??\C:`, the mount-point manager,
  the volume device and the pipe namespace carry no application-package grant,
  and **no ACL can reach them**. That is why a child process cannot start and
  why DOS-path resolution fails.

**Containment is default-on with an exact opt-out list.** The keyword is
`WINDOWS_UNCONTAINED` (118 uses under `tests/`); the authoritative pin is
`WINDOWS_UNCONTAINED_TESTS` in `scripts/verify_test_network_isolation.py:69`,
**129 entries**, each valued with a measured cause from a five-term vocabulary:
`dos-volume-name`, `batch-stub`, `ninja-named-pipe`, `qprocess-named-pipe`,
`credential-manager`.

**The rule, in this repository's own terms.**

> A row runs contained unless it needs a kernel object an AppContainer token
> cannot reach. Those failures are already enumerated by the five-term
> vocabulary, and none is grantable by a directory ACL. **A row whose subject
> is the repository's own files, tooling or build metadata, and which reaches
> no network, buys nothing from the container and should be pinned
> `WINDOWS_UNCONTAINED` with its measured cause.** A row whose subject is the
> product's network behaviour stays contained; if it cannot, the fix belongs
> in the product or its data-root arrangement, not in the registration.

**What the rule settles.** Outright: this card's eleven rows — all are Python
meta/tooling contract rows that spawn children (`tests/CMakeLists.txt:299-497`,
`tests/application/CMakeLists.txt:573`), and their structural twins in the same
files are *already* pinned uncontained (`:340`, `:389`, `:418`, `:447`, and the
two inventory rows). Also [[BPR-0873]], both documentation rows. Partially:
[[BPR-0876]]'s four exit-3 seed courses. It does **not** settle [[BPR-0874]],
[[BPR-0877]] or [[BPR-0875]] — those fail uncontained too and are product or
test defects the sweep found alongside the containment ones.

**The sequencing constraint, which is the important operational fact.** The
isolation gate is exact in **both** directions (`scripts/verify_test_network_isolation.py:65-68`):
an uncontained row missing from the dict fails, and a dict member found
contained fails. The keywords live in `tests/**`, which a station owns; the
dict lives in `scripts/**`, which **no station may edit**. So a station cannot
land a containment decision by itself, and splitting the change across two
commits leaves `test_network_isolation_manifest` red in between. Land the
`tests/**` keywords and the `scripts/**` pins in **one commit on main**, after
the station has measured the uncontained re-runs. Do not pin a row that has
not been measured: the gate's exactness means a wrong pin fails the manifest
row rather than passing quietly.
