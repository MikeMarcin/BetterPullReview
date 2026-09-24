---
schema: "implement-rfc.card.v1"
id: "BPR-0877"
title: "Windows: four device-authorization rows print appearance and notification markers instead of their flow markers"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
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
