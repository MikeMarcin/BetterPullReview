---
schema: "implement-rfc.card.v1"
id: "BPR-0877"
title: "Windows: four device-authorization rows print appearance and notification markers instead of their flow markers"
status: "Ready"
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
