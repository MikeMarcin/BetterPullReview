---
schema: "implement-rfc.card.v1"
id: "BPR-1576"
title: "Panes say when their data is not ready"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-17","owner_note":"We need a clearer visual signal in panes when data is not yet ready. Be that dimming what's there or showing loading spinners or whatever you think the base visual UX design is. This is especially true of the overview page when opening a PR.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0032","RFC-BPR-0031"],"related_cards":["BPR-0795","BPR-1200","BPR-1357","BPR-1539","BPR-1575"],"reservation":"docs/issues/_reservations/BPR-1576--BPR-1576--f97eb89c72ea.json"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"2f0850a2e","evidence":{"kind":"observation","detail":"core_pending_presentation (looks, reveal, slow, busy, announcements, next wake) and qt_pending_veil (placeholder/loading/slow/quiet/refreshing/partial/failed/failed-retained, no flash without time - the negative control, mouse transparency, mask, Retry) at 2f0850a2e."}},{"criterion":"AC2","outcome":"pass","revision":"2f0850a2e","evidence":{"kind":"observation","detail":"application_pending_states_public_path: the Overview opening #17 then #18 on a slow fixture (first answers 1.5 s, every page 1.2 s) - sections wait in place, title/description first, 'N files reported', slow words, ready together, one 'Overview ready' announcement; before/after frame sequences in Light and Dark."}},{"criterion":"AC3","outcome":"pass","revision":"2f0850a2e","evidence":{"kind":"observation","detail":"qt_pending_veil window course (drawers, conversations, review content under refresh), diff_workbench comparison and syntax veil, Updates line mark; application_pending_states_public_path Home first find; Markdown image mark."}},{"criterion":"AC4","outcome":"pass","revision":"2f0850a2e","evidence":{"kind":"observation","detail":"Reduced vs full motion, high contrast, busy state and announcements asserted in qt_pending_veil; layout unchanged under a veil; renders in Light and Dark."}},{"criterion":"AC5","outcome":"pass","revision":"2f0850a2e","evidence":{"kind":"observation","detail":"Courses above plus the full dev suite (see checkpoint), clang-format 22, clang-tidy 22, documentation gate; UX spec 'The loading-state pattern'."}},{"criterion":"AC6","outcome":"pending","revision":"2f0850a2e","evidence":{"kind":"freeform","detail":"Owner Accept/Reject: candidate BPR-1576-loading-states-v001, attended session 232e2c2eee171af499adac505a0e561e, frozen root bpr1575-1576-v001.wu26nlx6."}}]
---

# BPR-1576: Panes say when their data is not ready

The owner asked on 2026-10-17: "We need a clearer visual signal in panes when
data is not yet ready. Be that dimming what's there or showing loading
spinners or whatever you think the base visual UX design is." And: "This is
especially true of the overview page when opening a PR."

Opening a pull request, the Overview shows its title and description as soon
as GitHub names them ([[BPR-0795]]), but its file table is an empty header and
its side column a column of dashes until the accepted review lands, several
seconds later on a slow network. Nothing says what is still coming.

Governing design: the review workbench of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) and its
"Empty, loading, and failure states", recorded in the
[workbench specification](../../ux/workbench-specification.md); Home of
[RFC-BPR-0032](../../rfcs/RFC-BPR-0032-calm-home-and-accounts-surfaces.md);
motion preferences of
[RFC-BPR-0031](../../rfcs/RFC-BPR-0031-dif-workbench-animation-integration.md).
Refresh's own control is [[BPR-1575]], which uses this pattern.

## Acceptance

- AC1: One loading-state pattern, decided in the core from a pane's real facts
  (loading or failed, content shown, partial, elapsed by the presenter's
  monotonic clock): placeholders first, the turning mark and words after a
  400 ms reveal, what the read still waits on after 6 s, a light veil and a
  top rule over retained content being replaced, a plate saying what is still
  coming over partial content, and a failure with Retry in place of the content
  or as a strip over retained content. Nothing is shown from a timer that
  fakes progress; a load faster than the reveal never flashes.
- AC2: The Overview applies it section by section on opening and on switching
  pull requests in a window: the title, description, file table (as many rows
  as GitHub reported files), side column and comment counts each say their own
  wait and show as they arrive.
- AC3: The comparison panes (loading, and syntax colours still coming), the
  Files drawer, the Comments drawer, Threads, the repositories drawer, Home's list
  (restoring, discovering, refreshing, failed with Retry), the review while a
  refresh replaces it, the Updates line's readiness and Markdown images use
  the same pattern.
- AC4: Reduced motion stills every mark and the rule (a dashed rule instead of
  a sweep); the four themes draw it from the palette and the failure ink, and
  high contrast drops the veil's wash; a waiting pane reports busy to
  assistive technology from the first moment, and its revealed start, slow
  wait, end and any failure are announced once, politely; nothing under a veil
  moves.
- AC5: Core, Qt and composed courses cover AC1 to AC4 with a negative control
  (no time, no reveal), a frame sequence of the Overview opening before and
  after in Light and Dark, and the UX specification records the pattern as the
  application's standard. Format, tidy, architecture and documentation gates
  pass.
- AC6 (human): The owner accepts the frozen candidate, Light and Dark, with a
  slow pull request to open: "Panes say what they are waiting for, starting
  with the Overview".

## Implementation

Core decides: `core::decide_pending_look` and its announcements
(`bpr/core/pending_presentation.hpp`). Qt presents: `qt::pending_veil` laid
over each pane, `qt::pending_mark` on lines that already speak, wired to the
window's read status, read progress, queue and overview projections, the
comparison surface and its classification. The composition says when a
classification is awaited or failed and whether the theme is high contrast.
Fixtures can hold every page of a read, refreshes and discovery for the
frozen build. Never drive or capture the owner's desktop.
