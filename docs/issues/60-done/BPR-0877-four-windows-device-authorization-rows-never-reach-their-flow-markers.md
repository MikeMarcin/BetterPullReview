---
schema: "implement-rfc.card.v1"
id: "BPR-0877"
title: "Windows: four device-authorization rows print appearance and notification markers instead of their flow markers"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"The four rows pass on Windows, or the card names exactly what each was running instead and why","outcome":"pass","revision":"f3399676","evidence":{"kind":"command","detail":"bpr-main-win, build/dev at 02905c77's sources (MSVC 19.51 Debug, Qt 6.11.2), run at f3399676 (differs only by two Git protocol test files), serial, offscreen, uncontained as registered: registered_course 6.85 s, refresh_course 9.76 s then 10/10 more, panel_course 6.88 s, panel_absent 6.83 s, all Passed with DURABLE_COPIES:0. Before f4816f98 they ran the right executable with their own flags (ctest -N -V at d3b5a3ef) and failed on DEVICE_FLOW_DURABLE_COPIES printing the size_t::max sentinel, because the scan could not open the profile's own zero-sharing lease lock; the single multi-line PASS expression made CTest report the expression, not that line.","path":"docs/issues/60-done/BPR-0877-four-windows-device-authorization-rows-never-reach-their-flow-markers.md#all-four-pass-on-windows-closed-bpr-main-win-2026-09-23"}},{"criterion":"The card states plainly whether the Windows device sign-in flow itself was affected, or only the courses","outcome":"pass","revision":"f3399676","evidence":{"kind":"observation","detail":"Only the courses: device_durable_copies() (src/app/src/github_application.cpp) is compiled under BPR_BUILD_TESTING only and reads the fixture token to count copies for course evidence; the shipped device flow never calls it.","path":"docs/issues/60-done/BPR-0877-four-windows-device-authorization-rows-never-reach-their-flow-markers.md#all-four-pass-on-windows-closed-bpr-main-win-2026-09-23"}},{"criterion":"macOS and Linux keep passing all four","outcome":"pass","revision":"f3399676","evidence":{"kind":"freeform","detail":"Not run from this station. The change is uniform and excludes only profile-runtime.lock, which on POSIX opens for reading and holds no token, so the count cannot change; Linux passed all four at 2b2e5549 (dsp5-linux-full-sweep.md) and the refresh course passed on all three platforms at 0d19c990 (RFC-BPR-0016.md). The next macOS and Linux sweeps confirm HEAD."}}]
---

# BPR-0877: Windows: four device-authorization rows print appearance and notification markers instead of their flow markers

## Defect

Four device-authorization rows fail on the Windows station, and they fail as a
group:

- `application_device_authorization_registered_course` and
  `application_device_authorization_refresh_course` want
  `DEVICE_FLOW_STAGE:authorized`
- `application_device_authorization_panel_course` and
  `application_device_authorization_panel_absent` want
  `DEVICE_FLOW_ENROLLMENTS:1`

None of those markers appears. What the runs print instead are the
**appearance and notification markers** — that is, the output of a different
course's marker set.

No `DEVICE_FLOW_CLEANUP_FAILED:true` appears anywhere in the sweep, so this is
not the cleanup cluster in [[BPR-0875]] wearing a different hat.

Four rows that ask for device-flow markers and receive appearance markers
points at the courses running something other than what their name says —
a wrong executable, a wrong argument, or a shared binary dispatching on a
mode that Windows resolves differently. That is a guess; the first job is to
find out which, and the run's own output should be able to say.

This is the run's first full Windows sweep, so the failure is newly observed.
Whether it is newly broken is unknown.

## Scope

- Determine what these four rows actually execute on Windows: the resolved
  command, its arguments, and which course's markers it is really producing.
  A row printing another course's markers is a registration or dispatch
  question, not an assertion question, so start there rather than at the
  assertions.
- Check whether the four share one executable and differ only by argument, and
  whether that argument survives the Windows registration.
- Establish whether they predate this run.
- If the product's device flow is genuinely broken on Windows, that outranks
  the harness question: a reviewer signing in on Windows would be affected,
  and that would deserve its own priority rather than living in a sweep card.

## Acceptance

- The four rows pass on Windows, or the card names exactly what each was
  running instead and why.
- The card states plainly whether the Windows device sign-in flow itself was
  affected, or only the courses.
- macOS and Linux keep passing all four.

## Correction: these four rows already run uncontained (dsp-006 scout)

**This card's containment framing is a dead end, and that is worth knowing
before anyone spends a run on it.** All four rows —
`application_device_authorization_registered_course`, `_refresh_course`,
`_panel_course` and `_panel_absent` — are already listed in
`WINDOWS_UNCONTAINED_TESTS` (`scripts/verify_test_network_isolation.py:69+`),
so they carry no isolation prefix and never entered the AppContainer in the
sweep that produced this card.

Whatever stops them reaching their flow markers is therefore a registration,
dispatch or product problem in the **uncontained** path. Classify it (c), a
genuine defect the sweep found alongside the containment ones; grouping the
53 failures by symptom hid that.

One observation on the station settles which of the three it is, in a single
run:

    ctest -N -V -R application_device_authorization_ 2>&1 | head -200

plus one full stdout capture of `application_device_authorization_panel_course`.
That distinguishes wrong-executable from wrong-argument from product breakage.

Caveat the scout flagged: this reads the pin list at `main`, and infers the
sweep's build had those pins applied. A configure predating the pins would
invalidate the inference. The same `ctest -N -V` settles that too, because it
prints the resolved command line with or without the isolation prefix.

## All four pass on Windows; closed (bpr-main-win, 2026-09-23)

**What the rows were running.** The Windows station already answered this in
`f4816f98`, merged as `d3b5a3ef` on 2026-09-17, but the card was never
closed. `ctest -N -V` showed all four rows running the application directly
(uncontained, as the scout said), each with its own flags. So the executable
and the arguments were both right. The title's reading was wrong: the runs
did not print another course's markers. Each row's `PASS_REGULAR_EXPRESSION`
is one multi-line literal, so the first line that does not match fails the
whole row, and CTest then reports the expression, not the line. The
appearance and notification markers were simply the output printed around
the real mismatch. That mismatch was
`DEVICE_FLOW_DURABLE_COPIES`, which printed `size_t::max()`, the scan's
"unavailable" sentinel. The durable-copy scan opens every regular file under
the data root, and on Windows it could not open one of them: the profile's
own lease lock, `profile-runtime.lock`, which this runtime holds with zero
sharing. `f4816f98` excludes exactly that path. Every other unreadable file
still answers unavailable.

**The Windows sign-in flow itself was not affected.** Only the courses were.
The scan is `device_durable_copies()` in `src/app/src/github_application.cpp`,
which is compiled only under `BPR_BUILD_TESTING`. It reads the fixture's
token to count copies for the course's evidence, and the shipped device flow
never calls it.

**At HEAD.** The build is `build/dev` at `02905c77`'s sources (MSVC 19.51
Debug, Qt 6.11.2), run at `f3399676`, which differs from those sources only
by two Git protocol test files. Rows ran serially and offscreen, as
registered:

| Row | Result | Markers |
| --- | --- | --- |
| `application_device_authorization_registered_course` | Passed, 6.85 s | `STAGE:authorized`, `ENROLLMENTS:1`, `VAULT_STORES:1`, `DURABLE_COPIES:0` |
| `application_device_authorization_refresh_course` | Passed, 9.76 s, then 10 of 10 more (9.74–9.81 s) | `ROTATIONS:2`, `REFRESH_STATE:scheduled`, `VAULT_STORES:3`, `REFRESH_STORES:3`, `REFRESH_DELETES:1`, `DURABLE_COPIES:0` |
| `application_device_authorization_panel_course` | Passed, 6.88 s | `STAGE:authorized`, `ENROLLMENTS:1`, `DURABLE_COPIES:0` |
| `application_device_authorization_panel_absent` | Passed, 6.83 s | `STAGE:authorized`, `ENROLLMENTS:1`, `DURABLE_COPIES:0` |

The refresh course's second fault, one rotation instead of two, is
[[BPR-0902]]'s, and it no longer reproduces here. That card records it. The
logs are station records under `build/logs/bpr-0877-0902-main-win/` and are
not committed.

**macOS and Linux.** These were not run from this station. The change is
uniform across platforms and excludes only the lease lock. On POSIX that file
opens for reading and holds no token, so excluding it cannot change the
count. All four rows passed on Linux at `2b2e5549`
(`docs/verification/dsp5-linux-full-sweep.md`), and the refresh course
passed on all three platforms at `0d19c990` (`RFC-BPR-0016.md`). The next
macOS and Linux sweeps confirm the rows at HEAD.
