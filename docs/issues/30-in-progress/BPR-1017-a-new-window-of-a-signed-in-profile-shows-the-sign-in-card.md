---
schema: "implement-rfc.card.v1"
id: "BPR-1017"
title: "A new window of a signed-in profile shows the sign-in card"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P2"
metadata: {"origin":"BPR-0990 composed discovery course, 2026-09-23","found_by":"BPR-0990"}
---

# A new window of a signed-in profile shows the sign-in card

## Finding

In `application_personal_discovery_public_path` ([[BPR-0990]]) the first window
signs in, and then **New Window** opens a second window of the same profile.
The second window is attached to the profile's accepted session
(`github_application_host::attach_window` adopts it into the window's read
host), but its Qt window is never told: its session panel reports
`no_profile`, its Home shows the first-launch sign-in card
(`landing_state::sign_in`), the queue is hidden (`home_list()->queue_shown()`
is false) and Home's strip controls are absent, although its queue model holds
the profile's rows. It learns the session only from the next transition,
which in practice is its first pull request read. The session presentation is
published on transitions only; nothing replays the current one to a window
that attaches after it.

Observed at `3e12138a` plus the uncommitted BPR-0990 candidate; the course
works around it by opening a pull request in the second window before it looks
at Home.

## Outcome

A window attached to a profile whose account session is accepted presents that
session from its first frame: signed-in Home (bar, queue, strip controls),
the account on the strip, and the same enablement a window that signed in
itself would have. A public-preview session presents as public preview. No
read, vault or provider effect is added to produce it.

## Acceptance

- A composed course opens **New Window** after sign-in and asserts the second
  window's Home is the signed-in Home with the profile's queue, before it reads
  anything, with zero additional account reads and vault operations.
- The public-preview and signed-out variants present as they do today.
- `application_profile_multi_window_public_path` and
  `application_personal_discovery_public_path` still pass; the latter drops its
  workaround.

## Boundaries

RFC-BPR-0024 (profiles and windows) owns window/session presentation; this is a
post-closeout defect, not a reopening of its accepted criteria. No GitHub or
GHES mutation, credential change or new preference.
