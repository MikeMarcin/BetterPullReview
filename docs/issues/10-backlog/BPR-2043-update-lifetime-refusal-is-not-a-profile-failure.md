---
schema: "implement-rfc.card.v1"
id: "BPR-2043"
title: "Report a refused update-lifetime start as itself, not as an unopenable profile"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2043--BPR-2044--459846f19aa3.json","origin":"live-freeze diagnosis, 2026-11-10, attended session 243efdc0c300f68944464fc350c097ed; evidence in docs/verification/RFC-BPR-0099.md and the v0-5-0-prerelease-targets-v1 handoff (Live-freeze correction)"}
depends_on: []
---

# Report a refused update-lifetime start as itself, not as an unopenable profile

## Scope and acceptance

When `release_update_installation::join` refuses at startup
(`begin_update_lifetime` in `src/app/src/profile_desktop.cpp`), the app
finishes the initial start as `github_application_startup_failure::unavailable`.
That is reported as `STARTUP_FAILED` and answered with the profile recovery
dialog, "A local database in this profile could not be opened", whose reset
offers cannot fix the cause and could reset a healthy profile for nothing.
Found when a frozen build sealed after its first launch left a read-only
`.bpr-update-<digest>/` beside the bundle: every start failed `admission_lock`
and showed that dialog (established with lldb on the frozen executable).

A refused update-lifetime start says what happened (the installation's update
control could not be joined, and why, as far as the reason says), names the
folder concerned where the reason allows, and offers no profile reset. The
profile's own database failures keep the existing dialog. A course starts a
tests-off bundle whose control folder refuses the join and checks the message
and the absence of the reset choices.

Not scheduled in any milestone; found during `v0-5-0-prerelease-targets`.
