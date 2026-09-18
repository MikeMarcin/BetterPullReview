---
schema: "implement-rfc.card.v1"
id: "BPR-0902"
title: "Windows: the device refresh course rotates once instead of twice, and the flow requires re-enrolment instead of scheduling"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win, exposed once BPR-0877's lock skip removed the sentinel that was masking it","found_at":"d6dfca2b","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0902: Windows: the device refresh course rotates once instead of twice, and the flow requires re-enrolment instead of scheduling

## Defect

`application_device_authorization_refresh_course` was one of [[BPR-0877]]'s four
rows. Three of the four now pass. This one got further and stopped on a
**second** fault that the first was hiding.

BPR-0877's cause was that the durable-copy scan answered
`size_t::max()` — its "unavailable" sentinel — because it could not read the
lease lock it holds open itself. With that fixed the row reaches
`DEVICE_FLOW_DURABLE_COPIES:0`, which is correct, and then fails on this:

| Marker | Expected | Actual |
|---|---|---|
| `ROTATIONS` | 2 | **1** |
| `REFRESH_STATE` | `scheduled` | **`reenrollment_required`** |
| `VAULT_STORES` | 3 | **2** |
| `REFRESH_STORES` | 3 | **2** |
| `REFRESH_STORED_TOKEN` | — | differs accordingly |

So the second rotation never happens, and the flow answers that the device must
re-enrol rather than that a refresh is scheduled.

**It is not a timeout.** The row ends at 6.8 s of a 12 s budget.

This is why BPR-0877's row-level `PASS_REGULAR_EXPRESSION` being one multi-line
literal matters: the first mismatched line fails the whole thing and CTest
reports the regular expression rather than the line, which is why the original
sweep read this cluster as "`ENROLLMENTS:1` never printed" when it did print.

## Scope

- Establish whether the second rotation is refused or never attempted. Those are
  different faults and the markers above do not distinguish them.
- `reenrollment_required` is a real product answer, not a test artefact. Find
  what makes the device look un-enrolled to the second rotation when the first
  one succeeded and stored.
- Check whether this is Windows-only. The three sibling rows pass on Windows and
  all four pass on macOS and Linux today, which is suggestive but not a
  measurement of *this* assertion on the other platforms.
- Consider splitting the row's pass expression, or at least recording that a
  single multi-line literal reports the regular expression rather than the
  failing line. That cost this run a wrong reading once already.

## Acceptance

- The row passes on Windows, or the remaining difference is a named, understood
  cause with its own record.
- The card says whether the second rotation was refused or never attempted, and
  why the device looked un-enrolled.
- macOS and Linux are unaffected, checked rather than assumed.

## Read-only analysis, dsp-007 — done on the Mac so the station can fix rather than bisect

This machine cannot reproduce the row. What the station was missing is the
reading, so here it is. Everything below is from source; nothing was run.

**It is one bug, not three, and that is decidable from the code.** The three
counters are strictly downstream of each other on one path. A second rotation
would store its refresh material (`RefreshStores` 2 → 3), then in
`on_refresh_stored` enrol its access token (`VaultStores` 2 → 3), and only then
`++Refresh.Rotations`
(`src/application/src/device_authorization_application_host.cpp:947`), in that
order in that one function. A rotation that never happens subtracts exactly one
from each, and `REFRESH_STORED_TOKEN` keeps rotation 1's digest — which is
exactly what the row reports. The single reading that would falsify this is
`RefreshStoreAttempts == 3` with `RefreshStores == 2`.

**There is no rotation loop.** Rotations are a timer chain: each renewal arms
the next through `schedule_refresh()` (host:970-995). The course only sees a
second one because a test clock makes an ~8 h timer due in ~1 s
(`src/app/src/github_application.cpp:3728-3760`, composed at 5159-5174). The
clock's step is unlocked by the **published** rotation count reaching 1, not by
the internal one.

**The card's vault hypothesis is wrong, and the station should not spend time
on it.** Under `--github-fixture` the host is handed the *fixture's* vault, not
the platform one: `credential_vault()` returns `Fixture->credential_vault()`
when a fixture is composed (`github_application.cpp:6162-6170`), and both store
counters are read from fixture evidence (`:6940-6947`, `src/app/main.cpp:3658`).
`make_platform_vault` (`:2196-2214`) — Credential Manager, Keychain, Secret
Service — is never reached. So "the Windows Credential Manager refused a store"
is not supported: those counters are in-process and deterministic. The fixture
transport cannot refuse either; `refresh_access_token`
(`src/app/src/github_public_path_fixture.cpp:2003-2077`) refuses only when
`RejectRefresh` is set, and this course does not set it. **The fault is
upstream of both the transport and the vault.**

**Eleven places can produce `reenrollment_required`**, all funnelling through
`report_reenrollment` (host:1228-1238). Ranked by fit to "all three counters
short by exactly one":

1. **host:1032**, `begin_refresh`'s elapsed guard
   (`elapsed >= RenewalWindowSeconds`) — produces `refresh_rejected` with **no
   vault and no transport traffic**, so every counter stays where rotation 1
   left it. Best fit.
2. host:1103, the vault answered *absent* (`refresh_absent`) — also leaves the
   stores at 2, but would raise the retrieve attempts.
3. host:990 / host:1348, the timer port refused to arm.
4. host:968, `storage_failed` — needs a third store attempt that failed.

**One line decides between them, and the number it prints already exists.** The
fixture counts retrieve attempts (`github_public_path_fixture.cpp:2721`,
`:2746`, surfaced at `:3307`/`:3314`, carried on
`src/app/src/github_application_fixture_access.hpp:231`/`:281`) and never
prints them. Add beside the existing chain at `src/app/main.cpp:3889`:

```
<< "\nDEVICE_FLOW_REFRESH_RETRIEVES:" << static_cast<unsigned long long>(fixtureEvidence.RefreshRetrieveAttempts)
```

`2` means the second renewal ran and was refused downstream of the vault read.
`1` means it never got past the guard or was never armed. That is the card's
"refused or never attempted" question, answered by one run.

**Leading hypothesis for why Windows, labelled as a hypothesis.** It is a
one-second margin, not a platform API. `schedule_refresh` computes
`due = 28800 - 300 = 28500` and compares it against an elapsed value whose two
clock reads straddle the whole off-lane refresh store: `AuthorizedAt` is
captured in `observe_lifetime` (host:~902) at clock step 1, and `now()` is read
in `schedule_refresh` after `++Refresh.Rotations` at step 2. So
`elapsed = 28499 + δ`, truncated (host:57-70). With `δ < 1 s` the design works
and `wait = 1`; with `δ >= 1 s`, `wait = 0` and the renewal fires in the same
instant as rotation 1's supersession — the race the deliberate one second in
[[BPR-0547]] exists to prevent. Windows is the documented slow machine for this
exact hop: `main.cpp:5725-5733` records the access-token store landing ~138 ms
late there. The hypothesis needs roughly seven times that, which is its weakest
link and **cannot be settled by reading**.

If `wait` collapses to 0, rotation 1's in-flight supersession re-enters
`bind_device_refresh_authority` (`github_application.cpp:6172-6230`), which can
take the `!Host->accepted_device_lifetime()` branch (`:6205-6209`) and call
`release_refresh_authority()` → `release_refresh_binding()` (host:1748-1753).
Depending on ordering that either clears the binding so `schedule_refresh`
returns silently (host:975), or leaves the elapsed guard at host:1031 to fire.
**The next read, not yet done:** the body of `release_refresh_binding` — does it
clear `RefreshDurable`/`RefreshAuthority`? — and `accepted_device_lifetime()`.
That distinguishes "never armed" from "armed and rejected" before any run.

**Correction to this card's fourth scope item: do not split the pass
expression.** CTest treats multiple `PASS_REGULAR_EXPRESSION` entries as a
**disjunction** — any one matching passes the row — so splitting the literal at
`tests/end_to_end/CMakeLists.txt:395` into a list would weaken the row to
almost nothing, which is the opposite of the intent. The observation that a
single multi-line literal reports the expression rather than the failing line
stands and is worth recording; the remedy does not. Whether this tree has a
helper that composes several *required* expressions was not established —
reading `bpr_bound_automated_run` (`:398`) is the next step there.

**Also worth one cheap check nobody has done:** whether [[BPR-0877]]'s lock-skip
fix altered timing on this path. It is what made this row get far enough to
fail here, and it may also be what made it fail.
