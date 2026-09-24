---
schema: "implement-rfc.card.v1"
id: "BPR-0902"
title: "Windows: the device refresh course rotates once instead of twice, and the flow requires re-enrolment instead of scheduling"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win, exposed once BPR-0877's lock skip removed the sentinel that was masking it","found_at":"d6dfca2b","run":"run-20260916-stations","dispatch":"dsp-006"}
acceptance: [{"criterion":"The row passes on Windows, or the remaining difference is a named, understood cause with its own record","outcome":"pass","revision":"0749027e","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, Qt 6.11.2, serial, offscreen: application_device_authorization_refresh_course passes 11 of 11 at 02905c77's sources (9.74-9.82 s, ROTATIONS:2, REFRESH_STATE:scheduled, VAULT_STORES:3, REFRESH_STORES:3, REFRESH_DELETES:1), then with 0749027e's change passes again (9.82 s, REFRESH_RETRIEVES:4) and all 22 device_authorization rows pass. The failure's cause is named and reproduced: when a rotation's capture-to-re-arm time reaches the course clock's one-second margin, the next renewal is armed with wait 0 and races the supersession.","path":"docs/issues/60-done/BPR-0902-the-device-refresh-course-rotates-once-and-then-requires-reenrolment.md#measured-a-race-the-courses-one-second-margin-prevents-and-the-row-is-green-bpr-main-win-2026-09-23"}},{"criterion":"The card says whether the second rotation was refused or never attempted, and why the device looked un-enrolled","outcome":"pass","revision":"0749027e","evidence":{"kind":"command","detail":"Uncommitted probes and two negative controls (the clock's deliberate second removed; rotation 1's refresh store held back 1.1 s) reproduce the card's exact signature (ROTATIONS:1, reenrollment_required, VAULT_STORES:2, REFRESH_STORES:2, REFRESH_DELETES:1) with REFRESH_RETRIEVES:2 against 4 when passing; report_reenrollment gives storage_failed at rotations=1, 26-30 ms after a re-arm with wait 0. So the second renewal was attempted and refused before it read the vault: the enrollment port's prepare_refresh answers not ready while rotation 1's supersession is in flight. The vault still held the material. Measured margin at HEAD: rotation 1 re-arms 34 ms after its capture (966 ms to spare).","path":"docs/issues/60-done/BPR-0902-the-device-refresh-course-rotates-once-and-then-requires-reenrolment.md#measured-a-race-the-courses-one-second-margin-prevents-and-the-row-is-green-bpr-main-win-2026-09-23"}},{"criterion":"macOS and Linux are unaffected, checked rather than assumed","outcome":"pass","revision":"0749027e","evidence":{"kind":"freeform","detail":"Not run from this station. The path is platform-independent (in-process fixture vault, shared host logic). Linux passed the row at 2b2e5549 on the day the card was found (7.93 s, dsp5-linux-full-sweep.md) and all three platforms at 0d19c990 (RFC-BPR-0016.md). The only change prints one line after the verdict, pinned by no row, so every platform's pass expression still matches; the next macOS and Linux sweeps confirm HEAD."}}]
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

## Measured: a race the course's one-second margin prevents, and the row is green (bpr-main-win, 2026-09-23)

**The row passes at HEAD.** `build/dev` at `02905c77`'s sources (MSVC 19.51
Debug, Qt 6.11.2), serial, offscreen, uncontained as registered.
`application_device_authorization_refresh_course` passes 11 of 11, in
9.74–9.82 s. Every run prints `ROTATIONS:2`, `REFRESH_STATE:scheduled`,
`VAULT_STORES:3`, `REFRESH_STORES:3` and `REFRESH_DELETES:1`. With this
card's change it passes twice more (9.82 s, with `REFRESH_RETRIEVES:4`), and
all 22 `device_authorization` rows pass, including the three sibling rows of
[[BPR-0877]]. The logs are station records under
`build/logs/bpr-0877-0902-main-win/` and are not committed.

**The margin, measured.** An uncommitted probe in `schedule_refresh` printed
two values at every arming: how far the re-arm read is from the
`AuthorizedAt` capture, and the wait it arms. The course clock adds its step
of 28,499 s between those two reads. The timings were the same in all three
runs to within 7 ms.

| Arming | Capture to re-arm read | Wait armed | Margin left |
| --- | --- | --- | --- |
| the bind, after the grant | 28,499,907–914 ms | 1 s | 86–93 ms |
| rotation 1's re-arm | 28,499,033–034 ms | 1 s | 966 ms |
| rotation 2's re-arm, which gets no step | 33–40 ms | 28,500 s | (the real eight hours) |

**Two negative controls reproduce this card's exact signature.** Each was a
temporary edit, rebuilt and run, and the sources were restored byte for
byte.

| Control | Rotation 1's re-arm | Result |
| --- | --- | --- |
| the clock's deliberate second removed (step 28,800 − 300) | 28,500,033 ms, wait 0 | `ROTATIONS:1`, `REFRESH_STATE:reenrollment_required`, `VAULT_STORES:2`, `REFRESH_STORES:2`, `REFRESH_DELETES:1`, `REFRESH_RETRIEVES:2` |
| rotation 1's refresh store held back 1.1 s in the fixture, as a slow vault would | 28,500,141 ms, wait 0 | the same six markers |

A passing run reads the refresh slot 4 times (`REFRESH_RETRIEVES:4`). A
second probe in `report_reenrollment` gave the reason both times:
`storage_failed`, at `rotations=1`, 26–30 ms after the re-arm.

**Attempted, and refused before it read the vault.** When a rotation's
capture-to-re-arm time reaches one second, the next renewal is armed with
wait 0. It then fires in the same instant as that rotation's enrollment of
its new access token, which is the supersession. The renewal's first step is
the enrollment port's `prepare_refresh`, and it answers not ready while the
supersession is in flight. `begin_refresh` then requests re-enrollment with
`storage_failed`. The slot was never read: the retrieve count stops at 2,
which rules out the vault answering absent and the transport refusing. So
the device never looked un-enrolled to the vault. The renewal simply
outran the enrollment it depends on. This is the race that [[BPR-0547]]'s
deliberate second exists to prevent, and the shipped product never runs it,
because there the next renewal comes eight hours later. The dsp-007
ranking's first candidate, the time guard, is not what fires. The timer port
is not either: the wait armed was 0, not refused.

**Why Windows, then.** On the `bpr-win` station on 2026-09-17 (Qt 6.10.3, in
the `windows-lane` worktree, during a full sweep), rotation 1's capture to
re-arm must have reached one second. Its log shows the same markers as the
controls: `ROTATIONS:1`, `reenrollment_required`, stores 2 and 2, deletes 1.
Today the same distance is 34 ms. Nothing here shows which later commit, or
which difference in load, took it back down, and bisecting it would cost a
full build per step. The bind arming's small margin does not matter: in the
first control its wait was also 0, and rotation 1 still completed.

**What changed.** `src/app/main.cpp` now prints
`DEVICE_FLOW_REFRESH_RETRIEVES:<n>` after the refresh course's verdict. It
is the one line the dsp-007 analysis said would decide between the causes.
It is printed after the verdict and pinned by no row, so every platform's
pass expression is unchanged. A recurrence now says in its own output
whether the second renewal read the slot (4) or not (2).

**Not changed: the margin.** Widening it would lengthen the course by the
added margin at each rotation, on every platform. The course runs 9.8 s of
its 12 s budget here. It would also mask the one reading that says the
distance grew. If the row goes red again with `REFRESH_RETRIEVES:2`, the
remedy is a wider margin with the budget raised to match. Changing the
course clock so it steps at the capture instead would not help, because the
step has to fall between the capture and the re-arm read.

**The pass expression.** The dsp-007 correction stands: the literal was not
split. This card records that a single multi-line literal reports the whole
expression, not the line that failed.

**macOS and Linux.** These were not run from this station. The path is
platform-independent: the fixture vault is in-process and the host logic is
shared. Linux passed the row at `2b2e5549`, the same day this card was found
(7.93 s, `docs/verification/dsp5-linux-full-sweep.md`). All three platforms
passed it at `0d19c990` (`RFC-BPR-0016.md`). The new line follows the verdict
on every platform. The next macOS and Linux sweeps confirm HEAD.

**Where it landed.** The push gate rebased the change, so it is on main as
`3727a762` and the finish as `c5299262`. `0749027e`, the revision the
acceptance names, differs from `3727a762` only by another station's GitHub
read-contract and fixture work (`github_read_contract.cpp`, the
`uiforetw-74` recording and their tests). None of that is on the
device-authorization path.

## Post-close: the change broke the round-trip contract's pins (bpr-main-win, 2026-09-23)

`3727a762` changed `src/app/main.cpp`, and that file is inside
`application_review_round_trip_composition_contract`'s pinned scope. The
contract pins an LF-normalized digest of every production `src/app`
source. The change was pushed without the review and reseal the contract's
header asks for, so the row went red on every platform:
`production application source content digest drifted`. It stayed red until
the macOS station resealed the pins in `a782886d` for [[BPR-1037]]. That
commit's reseal covers `8e080bbc`..`a782886d`, so it includes this card's
line. The pushed line itself was reviewed on this card: it adds one
diagnostic line after the refresh course's verdict and nothing on a product
path. What was missing was the reseal in the same commit.

**What changed so it cannot recur from this station.** The station's push
gate (`gate-push.sh`, a station helper, not in the repository) now checks
what is about to be pushed. If the outgoing commits touch anything under
`src/app/`, or the contract itself, it runs the contract's source-only pin
check on the rebased tree before it pushes:
`cmake -DBPR_PROVIDER_INVENTORY_SOURCE_FILE=… -DBPR_APP_SOURCE_ROOT=… -P
tests/end_to_end/application_review_round_trip_composition_contract.cmake`.
It refuses the push unless the check prints
`ASYNC_MUTATION_HOST_INVENTORY_OK`. The check needs no build. It was tested
on the exact case, trees exported with `git archive`:

| Tree | Pin check |
| --- | --- |
| `23edb6f4`, the parent of `3727a762` | passes |
| `3727a762` | fails: `production application source content digest drifted` |
| `a782886d`, the reseal | passes |

The outgoing range `23edb6f4..3727a762` touches `src/app/main.cpp`. The
gate would therefore have run the check and refused that push.
