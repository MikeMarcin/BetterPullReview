---
schema: "implement-rfc.card.v1"
id: "BPR-1553"
title: "Updates GitHub recorded take the Updates line's handles again"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner chat, 2026-10-16","governing_rfc":"RFC-BPR-0029","related_rfcs":["RFC-BPR-0028"],"regressed_by":"0bfcbaa9a","latent_since":"1bd5c5e80","related_cards":["BPR-0646","BPR-0743","BPR-0829","BPR-1130","BPR-1340"],"reservation":"docs/issues/_reservations/BPR-1553--BPR-1553--9f29d2988e80.json","acceptance":["BPR-1553:import-kept","BPR-1553:handles","BPR-1553:live","BPR-1553:gates"]}
acceptance: []
---

# BPR-1553: Updates GitHub recorded take the Updates line's handles again

## Owner notes

Owner, chat, 2026-10-16, verbatim, on the installed app: "The updates epoch
timeline seems non-functional. It used to have a left and right head you could
move to choose which versions you were comparing but I cannot find those at
all anymore."

## What exists

- RFC-BPR-0028's Updates line draws the two compared updates as bracket
  handles; RFC-BPR-0029 puts on it every update GitHub recorded (a force push's
  event, "Event time …") beside the updates this install observed ("Commit
  date …", "Provisional · first seen here"). An update GitHub recorded has no
  epoch of this install until its head is imported, and its tooltip says
  "Comparison unavailable" until then.
- A review read with complete objects imports those heads in the background
  ([[BPR-0829]]). A review read with selected objects imports one only when the
  reviewer chooses it (`1bd5c5e80`): the occurrence history projection carries
  `AllowOccurrenceImport`, and the line makes each unimported force push a stop
  that a click, a drag or a key can choose.
- `epoch_comparison_control::apply_catalog` took `AllowOccurrenceImport` from
  every catalogue projection. Only the occurrence history projection sets it;
  the host's other catalogue projections (an accepted epoch, a history page)
  carry the default, `false`. The next one after the history turned the
  permission off, so the updates GitHub recorded were never stops.
- Until `0bfcbaa9a` (2026-09-26) only reviews above 100 files or commits were
  read with selected objects. Since then every review within the read limits
  on a sparse-capable connection is, so on every signed-in review the handles
  could reach only the updates this install observed. A pull request opened
  for the first time has one: the whole-PR pair puts both handles on it, seven
  pixels apart at the line's right end, and a drag snaps back to it.
- The tests did not see it. The composed drag course
  (`application_epoch_update_line_public_path`, [[BPR-1340]]) runs the
  anonymous public-preview fixture, whose smart-Git factory is not sparse, so
  its updates were imported in the background; the Qt occurrence course
  published the permission last.
- Measured live on `MikeMarcin/BPR-Test#71` (two force pushes and three
  ordinary pushes) through the shipped composition on `main` `4fe83a913`,
  signed in with the active `gh` account: both force-push nodes had Compare
  from here and Compare to here disabled; a drag of either handle onto the
  oldest update left the right handle where it was and moved the left one onto
  the current update's head (an empty pair). No historical acquisition was
  attempted in any of the three sessions.

## Scope

- **Import permission.** The control takes `AllowOccurrenceImport` only from
  the projection that carries the occurrence history, and clears it with that
  history (replacement, another authority, no authority).
- **Live course.** `test_live_second_account_course --course epoch` opens a
  BPR-Test pull request in Review, says which nodes each handle can take,
  drags both handles onto the oldest update and renders the line. Its sign-in
  now uses the active `gh` account's row when the card lists several accounts
  ([[BPR-1373]]), where it used to wait for an offer that no longer appears.

## Acceptance

- `BPR-1553:import-kept`: `qt_epoch_comparison_scale_{100,125,200}` publish an
  occurrence history granting the import, the whole-PR pair, and a later
  catalogue projection without the grant. The right and the left handle each
  drag onto an update GitHub recorded and dispatch its import to their own
  side, and Shift+Left steps onto one; it fails against the previous
  `apply_catalog`.
- `BPR-1553:handles`: in each built-in theme (Light, Dark, High Contrast Light,
  High Contrast Dark) at 100, 125 and 200 percent, both handles are painted in
  the accent, which stands off the line's ground, and the imported pair moves
  the handles apart with the accent span between them.
- `BPR-1553:live`: the live course on BPR-Test through the shipped composition
  shows the updates GitHub recorded as stops, and a drag of each handle onto
  one imports it and moves the comparison.
- `BPR-1553:gates`: the neighbouring epoch courses, the composition contracts,
  format, tidy and the documentation gate pass.

## Boundaries

The core decides, the host performs, Qt presents. No core, host, provider,
Git or storage behaviour changes; the host already publishes the permission
with the history. RFC-BPR-0029's product contract stands: an update GitHub
recorded stays "Comparison unavailable" until its head is imported.
