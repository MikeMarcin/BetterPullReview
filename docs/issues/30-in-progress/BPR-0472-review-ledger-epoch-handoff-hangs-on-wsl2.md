---
id: BPR-0472
title: The review-ledger composition's epoch handoff never completes on WSL2
status: In Progress
rfc: RFC-BPR-0007
depends_on: []
---

# The review-ledger composition's epoch handoff never completes on WSL2

`application_review_ledger_composition_contract` fails on the Ubuntu 26.04
WSL2 host (kernel 6.18.33.2-microsoft-standard-WSL2 x86_64, GCC 15.2.0, Git
2.53.0, Qt 6.10.3, ext4 worktree) with `epoch_handoff_failed=1` on the
`unavailable` course (epoch phase `single`), `render=1`, and `elapsed_ms` at
budget+12..22 ms. The same contract passes on macOS arm64 and in the Windows
matrix-11 record, and every RFC-BPR-0003 epoch suite passes on the same WSL2
host in the same run.

[[BPR-0156]] recorded this row on 2026-08-17 as "Budget expired at
40012-40018 ms against a 40000 ms bound. A sub-0.05% overrun; this host is
slower on that path than BPR-0050's guest." The 2026-08-28 Linux closeout
run for [[BPR-0054]] disproved that characterization by measurement rather
than assertion:

| Head | Budget | Result |
|---|---|---|
| `17c8950d` | 40000 ms | expired at 40018 ms, `epoch_handoff_failed=1` |
| `17c8950d` | 150000 ms | expired at 150019 ms, `epoch_handoff_failed=1` |
| `82c7d1c1` | 40000 ms | expired at 40014 ms, `epoch_handoff_failed=1` |
| `82c7d1c1` | 150000 ms | expired at 150012 ms, `epoch_handoff_failed=1` |

A merely slow path completes when given 3.75x the budget; this one never
does — the budget is only what ends the wait. The `82c7d1c1` rows ran in a
throwaway worktree at [[BPR-0156]]'s own baseline, before `e355895f`
(BPR-0361), `4af48e02` (BPR-0390), `8666fb54` (BPR-0397), and `5cb734d4`
(BPR-0459) touched the epoch surfaces, so those commits neither caused nor
fixed it. The behavior is pre-existing, WSL2-specific as far as measured,
budget-independent, and stable across three isolated re-runs.

This is not a general epoch hang; it is the forced-obstruction path. The
failing course is the third and last `bpr_run_review_ledger_probe` call:
`seed` and `reopen` pass, and the `unavailable` course deliberately obstructs
the ledger by replacing `state/v1/review-ledger.sqlite3` with a directory and
narrowing permissions, then asserts the obstruction survived. Under exactly
that condition the handoff never returns. The failure signature is
`PUBLIC_PATH_VERDICT: presentation=0 shutdown_complete=1 cleanup_failed=0
epoch_projection_failed=0 epoch_handoff_failed=1` with `PUBLIC_PATH_GROUPS:
core=0 fixture=0 maintenance=0 render=1`, and
`REVIEW_LEDGER_COMPOSITION_EVIDENCE:00000000000000` against the course's
expected `11111101111111` — all-zero, so the course never gets going rather
than stalling midway. `WORKBENCH_HOST_COMPOSED_OK` and
`APPEARANCE_HOST_COMPOSED_OK` still print, so composition succeeds and the
hang is downstream. Start at the obstruction handling. The RFC-BPR-0003
closeout review located the flag's raise sites around
`deliver_epoch_handoff`/`Epochs->accept` in
`src/application/src/github_application_host.cpp` (~lines 912-944 at
`812487ec`), so the question this card must answer first is whether the
obstructed-ledger course blocks inside RFC-BPR-0003-owned epoch-host code or
in the RFC-BPR-0007 composition around it; the RFC-BPR-0003 ledger's Linux
record closes over that named question and reopens through this card if the
answer is epoch-host code.

Reproduce with `-DBPR_RUN_BUDGET_MS=150000` against
`tests/end_to_end/application_review_ledger_composition_contract.cmake` on the
WSL2 host. The 2026-08-28 run logs were transient session scratch and are not
checked in; the durable evidence is this card's tables and the
[[BPR-0054]]/RFC-BPR-0003 final-replay record. A distribution note for
whoever takes this, corrected 2026-08-28 by direct observation on the host:
the WSL2 side holds no origin credential of its own (its fetch fails
`Permission denied (publickey)`), but the shared JaegAW checkout is fetched
from origin by the Windows side or the owner — fetches landed at 14:19 and
14:59 that day and `origin/main` reached `7160c283` with this card present,
while the WSL2-side local `main` lagged behind it. A session there reads
this card from `origin/main` (catching local `main` up is a no-network
fast-forward, coordinated so no concurrent Windows-side operation is mid
flight on the shared checkout); what the WSL2 side still cannot do is fetch
or push under its own credentials, so its evidence returns through the
tracker on another host or through the owner.

This card is deliberately independent of the RFC-BPR-0003 closeout: no
RFC-BPR-0003 claim row cites this contract, the four epoch commits above are
proven non-causal at baseline, and the RFC-BPR-0003 Linux suites pass on the
same host. What it owns: root-cause the never-completing epoch handoff in the
review-ledger composition on WSL2 (product defect versus harness versus WSL2
environment), correct or confirm the fixture, and replace [[BPR-0156]]'s
"slower host" attribution with the measured truth. Whether it also reproduces
on a full-system kernel is an open question this card should answer first.

## Root-caused on the Linux station — 2026-09-02

`bpr-linux` (WSL2 Ubuntu, GCC 15.2.0, `linux-lane` at `6fff0a53`,
read-only-proof task `task-20260902-bpr0472-lin`) reproduced the card's
signature at the prescribed 150 000 ms budget — `elapsed_ms=150016`,
`REVIEW_LEDGER_COMPOSITION_EVIDENCE:00000000000000`, `user 0m2.462s` against
151 s of wall clock — and at 20 000 ms identically. Not a hang and not
slowness: a condition polled that can never become true.

**Named site.** `src/adapters/sqlite/src/review_ledger_database.cpp`,
`capture_preflight_file`. The `unavailable` course leaves a *directory* where
the ledger belongs. On POSIX `open(2)` on a directory succeeds, so
`if (!input)` does not fire; the failure arrives at the first read, and
libstdc++ raises it as `std::ios_base::failure` from
`basic_filebuf::underflow` regardless of the stream's exception mask, so
`input.bad()` is never reached and the throw crosses an `std::expected`
boundary. Confirmed by temporary instrumentation in `deliver_epoch_handoff`
(`St13__ios_failure | basic_filebuf::underflow error reading the file: Is a
directory`) and by a twelve-line standalone program under the host's `g++`.
The unwind leaves `accept()` before `EpochHost->accept` at
`github_application.cpp:4100`, the catch-all in `deliver_epoch_handoff` sets
`EpochHandoffDeliveryFailed`, no epoch is ever delivered, and the run loop
polls until the budget ends it. Obstruction-shape table on the same probe: a
directory (empty, non-empty, or mode 0500) fails; a regular file with
garbage, a symlink to `/dev/null`, or a stray directory elsewhere under
`state/v1` all pass with the expected `11111101111111`.

**Answers to the card's questions.** It blocks in neither the RFC-BPR-0003
epoch host nor the RFC-BPR-0007 composition; both merely report a failure
from one layer below. The RFC-BPR-0003 Linux record does not reopen. It is
not WSL2-specific: the mechanism is POSIX `open` plus libstdc++'s throwing
`underflow`, which is why libc++ (sets `badbit`, so the existing check
converts it) and Windows (opening a directory fails outright) both pass.
[[BPR-0156]]'s "slower host" attribution is disproved by measurement.
Product defect in the SQLite adapter; the fixture is correct as written.

**Fix.** Contain the read rather than pre-check the path (the adapter refuses
check-then-open races elsewhere): the `istreambuf_iterator` read is wrapped
so an `std::ios_base::failure` returns `failure::unavailable`, with a
regression test `test_a_directory_in_the_ledgers_place_is_a_typed_refusal`
in `sqlite_review_ledger_storage`. On the station, applied and then
reverted: the composition contract passed in 1.5 s
(`REVIEW_LEDGER_COMPOSITION_CONTRACT_OK`) against 2 m 31 s and a failure
before; the new unit test without the fix terminates with the uncaught
`std::__ios_failure` (exit 134) and with it passes in 0.78 s; the full `dev`
matrix with the fix showed the composition contract passing and five failures
all pre-existing — the digest-pin drift now [[BPR-0510]], and the Secret
Service fixtures carried by [[BPR-0310]].

**Correction to this card's distribution note.** The WSL2 checkout is now a
launcher-started station on its own `linux-lane` branch whose
`git fetch --dry-run origin` exits 0; the "no origin credential" premise for
routing its evidence through another host no longer holds for fetch. Its
push authority is still unconfirmed, which is why this task ran
read-only-proof and the integrator lands the fix on `main`.

## Landed on `main` from the integrator host — 2026-09-02

The station's diff was the starting point and changed in one respect before
landing. Its regression test failed on this macOS host with the containment
alone: libc++ neither throws nor sets `badbit` when the stream is a directory,
it hands back an empty image, and the schema preflight then reports
`schema_mismatch` for a path that holds no ledger at all. That is two typed
answers to one question — `unavailable` under libstdc++, `schema_mismatch`
under libc++ — so the landed shape decides on the file type first:
`capture_preflight_file` refuses anything that exists but is not a regular
file before opening it, and keeps the contained read as the backstop for
whatever fails underneath. The station's check-then-open concern is answered
the same way: a directory that appears between the type check and the open
still lands in the contained read, so nothing escapes; the check only makes
the common case uniform.

One correction on the way: `std::filesystem::status` leaves its error code set
for a path that does not exist, where `exists` clears it, and the first cut of
the type check refused every first open of a new ledger. The not-found case is
now decided before the error code is consulted.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The containment alone is not uniform: under libc++ the regression test sees `schema_mismatch` | `ctest --preset dev -R '^sqlite_review_ledger_storage$' --output-on-failure` with the contained read and no type check | `FAILED: a directory left in the ledger's place is refused as unavailable` | measured before this commit |
| A ledger that does not exist yet still opens | the same selector with the type check but `status`'s error code consulted before the not-found case | `FAILED: the ledger opens`, and the composition contract lost `REVIEW_LEDGER_COMPOSITION_CONTRACT_OK` | measured before this commit |
| The landed shape | `ctest --preset dev -R '^(sqlite_review_ledger_storage\|application_review_ledger_composition_contract\|application_review_ledger_host)$' --output-on-failure` | 3/3: storage 0.70 s, composition contract 43.52 s, host 0.07 s | this commit |
| Format | `clang-format --dry-run --Werror` on the adapter and the test | clean | this commit |
| The defect's own host | the same three selectors on the WSL2 station at this commit | **Pending** — the station measured its own diff, not the landed one; requested by envelope, and the card closes on that reply | Pending |
