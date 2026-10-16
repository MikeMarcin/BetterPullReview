---
schema: "implement-rfc.card.v1"
id: "BPR-1553"
title: "Updates GitHub recorded take the Updates line's handles again"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner chat, 2026-10-16","governing_rfc":"RFC-BPR-0029","related_rfcs":["RFC-BPR-0028"],"regressed_by":"0bfcbaa9a","latent_since":"1bd5c5e80","related_cards":["BPR-0646","BPR-0743","BPR-0829","BPR-1130","BPR-1340"],"reservation":"docs/issues/_reservations/BPR-1553--BPR-1553--9f29d2988e80.json","acceptance":["BPR-1553:import-kept","BPR-1553:handles","BPR-1553:live","BPR-1553:gates"]}
acceptance: [{"criterion": "BPR-1553:import-kept", "outcome": "pass", "revision": "fa47d1bf7", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_epoch_comparison_scale_(100|125|200)$'", "detail": "3/3 pass with test_github_recorded_updates_stay_reachable_by_both_handles: after the occurrence history granting the import, the whole-PR pair and a later catalogue projection without the grant, the right handle drags onto the newer and the left onto the older GitHub-recorded update, each dispatching select_epoch_occurrence_intent to its own side with the projected catalogue, and Shift+Left steps onto one. With the previous apply_catalog all three fail at \"light: the right handle drags onto the newer GitHub-recorded update, which is imported to the right\"."}}, {"criterion": "BPR-1553:handles", "outcome": "pass", "revision": "fa47d1bf7", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_epoch_comparison_scale_(100|125|200)$'", "detail": "Light, Dark, High Contrast Light and High Contrast Dark at QT_SCALE_FACTOR 1.0, 1.25 and 2.0: the left handle's left side and the right handle's right side are within a third of the accent-to-ground distance of the accent (which is at least 60 sRGB units off the ground), a point between two nodes is not; after the import lands the handles sit on the imported and the current update and the span between them is the accent."}}, {"criterion": "BPR-1553:live", "outcome": "pass", "revision": "fa47d1bf7", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen build/dev/tests/end_to_end/live_second_account_course --course epoch --root <owner-private mkdtemp> --pull 71", "detail": "MikeMarcin/BPR-Test#71 (two force pushes, three ordinary pushes; closed and its branch deleted afterwards), signed in with the active gh account, shipped composition. Before (main 4fe83a913, three sessions on one root): both force-push nodes had Compare from here/to here disabled; a drag of the right handle onto the oldest update left it on the current head and a drag of the left one put it on the current head (an empty pair); the logs show no historical acquisition. After (fresh root): every node offered Compare from/to; the right-handle drag imported the oldest force push's head (node then offered Show this update alone) and the comparison became \"Comparing exact anchors, Oct 16 -> Oct 15\"; the left-handle drag followed onto it. X-RateLimit-Remaining stayed above 4700."}}, {"criterion": "BPR-1553:gates", "outcome": "pass", "revision": "c18cdeaf4", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_epoch_comparison_scale_(100|125|200)|application_epoch_update_line_public_path|application_anonymous_public_preview_public_path|application_epoch_ordinary_update_recreated)$'; ctest --preset dev -R composition_contract; format_check.py; tidy.py; verify_documentation.py", "detail": "On the tree merged with main c18f25662: the six epoch rows pass. Composition contracts on fa47d1bf7: application_composition_contract, workbench, review_ledger, review_round_trip and defect pass with no reseal (no src/app change); agent_chat, agent_finding, agent_invocation and queue did not run because their executables were not built in this partial build. clang-format 22.1.8 check and clang-tidy on the three changed files pass; DOCUMENTATION_OK. application_epoch_host was not run (its executable was not built; no application code changed)."}}]
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
