---
schema: "implement-rfc.card.v1"
id: "BPR-0644"
title: "Renew expired device grants at launch instead of forgetting them"
status: "Ready"
rfcs: ["RFC-BPR-0016"]
depends_on: []
priority: "P1"
---

# Renew expired device grants at launch instead of forgetting them

Observed on the Mac private alpha candidate (`50f364b8`): a GitHub.com device
sign-in at 01:57 local produced a durable binding with an 8-hour access
lifetime and a 6-month refresh lifetime. The reviewer closed the app at 12:18
and relaunched at 12:44. The launch showed `Not authenticated`, Connect opened
the pasted-credential prompt, and the device-binding row was tombstoned at
launch. The owner's expectation: a launch either resumes the stored login or
offers the OAuth device sign-in; it never asks for a pasted credential when a
grant is stored.

Mechanism, from `github_application_host.cpp` `restore_device_profile`: when
`remaining_access_lifetime` is zero the binding is erased and the function
returns; a tombstoned row also returns without `start_deferred_session()`.
The renewal timer in `device_authorization_application_host.cpp`
(`schedule_refresh`, `begin_refresh`) only runs while the process is up, and
`begin_refresh` itself requests re-enrollment once the access lifetime has
elapsed, so any restart after the access token expires loses the session even
though the refresh token is valid for months.

## Scope

At launch, an access-expired binding whose refresh lifetime remains must run
the existing rotation path (refresh token to new access and refresh material,
stored through the same vault boundary) before the deferred session starts;
only a refresh failure, a refresh-expired binding, or a server rejection
tombstones the row. A tombstoned or absent binding must still start the
deferred session so the device panel's Begin command is offered. Connect on a
device-capable profile with no stored credential should lead to the device
flow, not the pasted-credential prompt. Owned paths: the application host's
device restore and renewal, the device authorization host, their focused
tests, and the RFC-BPR-0016 course inventory.

## Acceptance

- A fixture course restores a binding whose access lifetime elapsed while the
  app was closed by rotating with the refresh token, without a prompt.
- A refresh-expired or rejected binding tombstones and offers Begin.
- The tests-disabled Mac app relaunched more than eight hours after a device
  sign-in resumes the session with no credential prompt (attended).

Further observation, 2026-09-06 20:34 to 20:37 UTC: a launch restored the
session from a durable binding (viewer and capability succeeded), the process
was then ended with SIGTERM, and the next launch found the row tombstoned
although its access lifetime had hours left. So restoration is also lost
across an ordinary restart before expiry, through the shutdown or the next
launch's cleanup path; reproduce with an orderly quit and with SIGTERM and
include both in this card's acceptance.
