---
schema: "implement-rfc.card.v1"
id: "BPR-1602"
title: "Settings › Storage & privacy › Troubleshooting resets this profile's local data"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"owner-request","origin":"Owner request relayed by the coordinator, 2026-10-18","governing_rfc":"RFC-BPR-0014","related_rfcs":["RFC-BPR-0048","RFC-BPR-0084"],"related_cards":["BPR-0953","BPR-0954","BPR-0955","BPR-0956","BPR-1217","BPR-1555","BPR-1597"],"reservation":"docs/issues/_reservations/BPR-1602--BPR-1602--48cdf9547216.json","acceptance":["BPR-1602:design","BPR-1602:scopes","BPR-1602:credentials","BPR-1602:lifecycle","BPR-1602:recovery","BPR-1602:copy","BPR-1602:gates","BPR-1602:owner"]}
---
# BPR-1602: Settings › Storage & privacy › Troubleshooting resets this profile's local data

## Owner request

2026-10-18, through the coordinator: "We need a button in Settings to clear
our data, just in case something happens and the db gets into a bad state; I
want users to have an easier path to self-serve and unblock themselves."

The owner proposed four options (clear caches; reset review state; reset
settings; reset everything), a strong confirmation for the destructive ones
with an offer to keep drafts and notes first, a safe open-database lifecycle
(core decides, host performs, Qt presents; never a half-deleted database), and
a recovery path when the profile cannot open at all (a startup dialog or a
`--reset-profile-data` flag named in the error).

## Boundaries

- Reuse the RFC-BPR-0014 maintenance host, its exact native inventory,
  `.bpr-maintenance-v1` journal and startup recovery
  ([release maintenance](../../architecture/release-maintenance.md)). No
  second deletion path and no recursive delete.
- Never read or delete credentials: no vault call, and `device-bindings` and
  `remembered-connections` stay. Signing out stays in Accounts & hosts; the
  existing two-step profile purge (which does remove credential bindings)
  stays as it is under Profile maintenance.
- Never touch other profiles, the shared registry or updater state, agent
  workspaces, diagnostic logs (they have their own Clear), exported files, or
  any Git checkout outside the app's own caches.
- Whole files only: a store is removed with its sidecars or not at all, so a
  damaged database is never opened to be reset.

## Design

Pending the owner's pick in a fresh attended session (mockups in Light and
Dark): A, four rows (Clear caches, Reset settings, Reset review data, Reset
everything), or B, one Reset local data… with a choice of scopes. Both share
the scopes, the Removes/Keeps confirmation, the optional copy of removed data,
and the startup recovery dialog.

## Acceptance

- **BPR-1602:design** The owner picks the design to build.
- **BPR-1602:scopes** On a populated profile root, each reset removes exactly
  its files and sidecars and leaves every other file byte-identical; Reset
  everything removes their union (negative control: a file outside the scope
  is unchanged).
- **BPR-1602:credentials** No reset calls the credential vault; device
  bindings and remembered connections are unchanged; the profile is still
  signed in after reopening.
- **BPR-1602:lifecycle** A reset retires the profile's windows and closes its
  stores before inventory, journals before deleting, and reopens the profile
  on success; a reset interrupted after the journal is finished by startup
  recovery; an unknown entry refuses and deletes nothing.
- **BPR-1602:recovery** A profile whose store is damaged opens to the recovery
  dialog instead of exiting silently; each recovery action leaves the profile
  open; `--reset-profile-data` opens the same dialog; a healthy profile shows
  no dialog.
- **BPR-1602:copy** The optional copy holds exactly the removed store files in
  a new folder and never overwrites one.
- **BPR-1602:gates** Focused tests, clang-format 22, the documentation gate and
  the composition contracts.
- **BPR-1602:owner** The owner accepts the frozen build in Light and Dark.
