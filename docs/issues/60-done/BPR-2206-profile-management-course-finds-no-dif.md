---
schema: "implement-rfc.card.v1"
id: "BPR-2206"
title: "application_profile_management_public_path stops at its Dif catalog check"
status: "Done"
rfcs: []
metadata: {"reservation":"docs/issues/_reservations/BPR-2206--BPR-2207--fa4afcace75b.json","found_by":"BPR-2198","lane":"claude/v0-5-0-prerelease-targets-52265d","commit":"2185e9ab3","regressed_by":"118bd0b34"}
depends_on: []
---

# BPR-2206: the profile management course finds no Dif

## Scope and acceptance

`application_profile_management_public_path` fails on macOS (dev preset)
before any profile gesture:

```
FAILED: the desktop composes one complete accepted Dif catalog
```

At that point `initial->window().dif()` is null and the status reports 0
decoded clips and 0 load attempts; waiting up to 20 s changes nothing, so
it is not a race. Set aside, the later `work->window().dif()->cache_identity()`
dereferences null and crashes (EXC_BAD_ACCESS in
`shared_ptr<qt::dif_cache const>::get`).

It is **pre-existing**: it fails identically at `6de00c5b1`, the commit
before BPR-2198 (`304a77a08`), built in its own worktree. With its four Dif
checks set aside the rest of the course, BPR-2198's migrated Settings ›
Profiles steps included, passes. `application_profile_multi_window_public_path`
asserts the same 10 clips and passes, so the difference is in how this
course's desktop is composed (it starts through `start_desktop` with only
`ApplicationDataRoot` set).

Acceptance: find why this course's window has no Dif catalog, fix the
composition or the course (not by deleting the checks), and the course
passes whole on macOS.

## Cause

The course starts signed out. Since `118bd0b34` (RFC-BPR-0032 amendment r1,
the in-window account setup) a signed-out Home opens on the account setup,
which hides the landing card. `workbench_window::update_dif_presentation`
still treated Home in the `sign_in` state as the card slot, and gave Dif the
card only when the landing had 960 pixels of room. A hidden landing is never
laid out past Qt's default 640 pixels, so Dif had no card and, because the
card slot was taken, no corner either: no Dif widget and no catalog load
(`DifCatalogLoadAttempts` 0). Waiting changed nothing because nothing would
place it. Probe in the course: landing hidden at 640 px, account setup shown,
window 1342x820. The multi-window course passes because its fixture is signed
in, where Dif takes the corner.

## Fix (`2185e9ab3`)

- `workbench_window_home_landing.cpp`: the card slot is the landing's only
  while the account setup is not shown. With the setup shown, Dif rests in the
  status corner, as it does on every other surface ([[BPR-1209]]). The window
  places Dif again when the setup appears or goes.
- `profile_management_public_path_tests.cpp`: the course waits for Dif in the
  window's corner beside the setup, and for each new profile window's Dif,
  before its catalog checks. All four catalog checks are kept.

Visible change: a signed-out window that shows the account setup now has Dif
resting in its corner. Before this fix it had no Dif at all.

## Evidence

- `ctest --preset dev -R '^application_profile_management_public_path$'
  --repeat until-fail:3`: passes 3 of 3 (5 to 7 s each). Before the fix it
  failed at "the desktop composes one complete accepted Dif catalog".

## Owner's look, 2026-11-16

**Accepted.** The owner judged Dif resting in the corner beside the signed-out
account setup, in attended session `1924669e55ed8a79ae40e9320a64c32b`, at
15:34 UTC. Candidate `BPR-2206-setup-dif-v001`, revision
`frozen-72955fd96-v002`, decision `3757ac8b-1bdd-4894-a701-3a77709caa98`, no
note. The build was frozen from `72955fd96` with `--attended-account-setup-look`.
Its check reached `ATTENDED_ACCOUNT_SETUP_SHOWN`, and no real agent started.
The session host has been stopped and the frozen root removed.
