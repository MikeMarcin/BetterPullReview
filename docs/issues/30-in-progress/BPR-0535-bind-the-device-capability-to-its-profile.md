---
id: BPR-0535
title: Bind the device-authorization capability to the profile it was composed for
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0526]
---

# Bind the device-authorization capability to the profile it was composed for

## Why

The independent capability-critical review of the RFC-BPR-0016 closeout head
`28fa0741` ([[BPR-0285]], 2026-09-03) falsified the acceptance claim that
every GHES profile returns `device_flow_unavailable` with zero requests.
`shipped_device_authorization_capability()` is per build, true on all three
platforms, and carries no profile; composition composes one device host
with a wire bound to `Profiles.front()`; [[BPR-0526]] mounts the panel bound
to whichever profile the picker selects and its begin dispatches
`begin_device_authorization_intent{Profile}` straight to the host; the core
copies the profile unchecked; `perform(begin)` checks only
`Capability.Registered` and calls `acquire_device_code`; and success calls
`Enrollment->enroll(aContext.Profile, token)`. So a reviewer who selects a
GHES profile and presses "Get a code" starts the GitHub.com device flow
with the GitHub.com client and, on authorization, enrolls a GitHub.com
token under the GHES profile. The documented "structural" guarantee — route
admission refusing non-GitHub.com contracts — binds the transport's
profile, not the intent's. `begin_public_preview_sign_in` is a second route
into the same host. No selector drives the case: the dev-tree gate's
`--ghes-host` run begins the flow for `Profiles.front()`.

## Required final state

- `application::device_authorization_capability` carries the profile it was
  composed for; composition sets it to the GitHub.com profile's identity.
- `perform(begin)` refuses a begin whose context profile is not the
  capability's with a typed `device_authorization_unavailable_event`
  before any transport call, for both the panel route and the
  public-preview sign-in route.
- A host test proves a begin for a foreign profile makes zero transport
  calls (today it reaches the transport), and a composed course under
  `--ghes-host` drives the mounted panel bound to the GHES profile and
  proves `device_flow_unavailable` with zero requests — the GHES negative the
  `unavailable` selector's `--ghes-host` argument was always meant to be.
- The RFC's GHES acceptance row, the ledger's platform table, and the two
  documents that state the guarantee describe what the composed product
  does.

## Non-goals

- Renaming the `unavailable` selector — decided on [[BPR-0285]].
- Any change to enrollment for the profile the capability was composed for.

## Lane — 2026-09-03

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, its one dependency terminal; a worker starts in a
worktree at `f39b6243`. It runs first among the three closure cards because
it is the security defect: until it lands, the mounted panel can begin the
GitHub.com flow for a GHES profile.
