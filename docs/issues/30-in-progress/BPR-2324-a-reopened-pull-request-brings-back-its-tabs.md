---
schema: "implement-rfc.card.v1"
id: "BPR-2324"
title: "A pull request opened again brings back the tabs it had"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","found_by":"owner request, 2026-11-20: \"It would be nice to remember what tabs we had open when we go back to a PR.\"","reservation":"docs/issues/_reservations/BPR-2324--BPR-2324--6174a9a6ce71.json","related_cards":["BPR-1131","BPR-1132","BPR-1475","BPR-1573","BPR-2166"],"related_rfcs":["RFC-BPR-0052","RFC-BPR-0025","RFC-BPR-0128"],"frontend_design":"loaded: frontend-design:frontend-design; no new surface, the existing tab strip shows what comes back"}
---

# BPR-2324: A pull request opened again brings back the tabs it had

## Outcome

The owner, 2026-11-20: "It would be nice to remember what tabs we had open
when we go back to a PR."

Today only Resume on a Recently viewed row brings a pull request's tabs back
([[BPR-1131]], [[BPR-1132]]). Every other way back (the bar, a link, Open on a
Recently viewed row, opening it again after Close review) starts with no tabs,
and the first comparison the window accepts then saves that empty set over
the remembered one, so the tabs are lost for Resume too.

With this card, each pull request remembers its tabs. When the reviewer comes
back to it by any path, in the same session or after a restart, its tabs
return in their order, with the same tab active.

## Acceptance

- AC1 Per pull request, the window remembers its open document tabs: every
  file tab, in order, which one is active, each tab's comparison when it has
  its own, and each file tab's reading position (the line it was on). Report
  tabs stay out, as RFC-BPR-0128 Q10 has it: a report comes back through its
  card. Overview, Discuss and Threads are pages, not tabs, and the drawer's
  tabs are not per pull request, so neither is remembered here.
- AC2 Opening the pull request again by any path, in the same session or
  after a restart, brings the remembered tabs back in their order with the
  same tab active. A tab that followed the window's comparison takes the
  comparison the reopened pull request accepts. It keeps its reading position
  only when that comparison is the one it was remembered with. A tab with its
  own pair keeps that pair and its position. A file the reviewer opens before
  the tabs return is not lost: it becomes the active tab.
- AC3 A remembered tab whose file is not in the pull request's current
  comparison is dropped without a message. The active tab then falls to its
  next surviving neighbour, or the previous one when it was last. The drop is
  logged.
- AC4 The tabs are stored with the pull request's local state: the profile's
  window restoration store, on its Recently viewed entry. That is bounded:
  sixteen tabs a pull request and the ten most recently viewed pull requests.
  Opening the pull request does not overwrite the remembered tabs with an
  empty set before they are brought back.
- AC5 Each return of remembered tabs writes one Standard diagnostic line,
  `workbench.tabs` with `action=tabs_restore`. It carries the counts restored
  and dropped and nothing else: no path, file, repository or comparison. It
  therefore reaches an anonymized export as counts only.
- AC6 Tests: the core policy (order, active tab, dropped tabs, comparison and
  position rules, a file chosen first, refusals); the host (the restore, the
  diagnostic line, no load while the window is not in Review); persistence
  across a restart through the composed application; and a Qt course checking
  the strip's order and active tab after the pull request is opened again,
  including a dropped tab. `architecture_boundaries`, the format check and
  `python3 scripts/verify_documentation.py --root .` pass.

## Boundaries

The core decides which tabs come back (`src/core`). The application host
restores them and writes the line (`src/application`). The composition holds
the remembered set until the file list is known and leaves it unchanged in
the store until then (`src/app`). Qt shows what the core published. Resume
keeps its exact course ([[BPR-1131]]), which restores the remembered comparison
itself. No new store, table or schema version is needed.
