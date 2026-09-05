---
schema: "implement-rfc.card.v1"
id: "BPR-0079"
title: "Integrate the composed RFC-BPR-0004 workbench path"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0075","BPR-0077","BPR-0078"]
---

# Integrate the composed RFC-BPR-0004 workbench path

Connect accepted file-diff results to the real BetterPullReview composition or
closest deterministic rendered integration harness: both natural-layout top
panes, one shared scrollbar/controller, the center link map, previous/next
change, selected line/change, and lower full-width detail. Connect epoch/file/
options switches, loading/failure, cancellation, late completion, theme/DPI,
focus, and accessibility.

A shell widget, static screenshot, isolated painter, or view-model-only test
does not satisfy this card.

The BPR-0075 host seam deliberately stops at an exact blob-materialization
port. This card must select and compose the real installed-Git/cache path
without opening a competing lifetime-exclusive cache session. It must connect
epoch/file/options changes and repository-cache deletion to exact diff-host
invalidation, retain the configured materialization ceiling through the real
adapter, and prove that shutdown makes destroyed Qt state unreachable after
the host and cache actor quiesce. Fake-port host evidence is not evidence for
that selected application path.

## Required direct evidence

Drive wheel input over left and right panes and divider; insertion/deletion
pinning; unequal replacement; reversal; scrollbar/Page/Home/End/reveal; ribbon
click; detail/navigation/top identity agreement; resize/font/DPI; file/epoch/
options stale result; shutdown; and absence of fake rows/permanent crossing
move ribbons through the selected composition.

## Implemented surface

- The real GitHub application window owns the production `diff_workbench`:
  two natural-layout read-only source panes, the production link-map splitter,
  one shared comparison controller/scrollbar, core-owned semantic selection
  and group navigation, and one full-width lower detail pane.
- The diff host obtains exact bounded blobs through the accepted epoch host.
  Blob work is queued on that repository actor and is admitted only when its
  complete cache-session request equals the actor's already retained
  lifetime-exclusive session; a stale same-repository PR cannot switch it.
- Installed Git checks type and declared blob size before retaining content,
  and the cache session validates its owned repository boundary before and
  after the process call. The 16 MiB computation ceiling remains intact.
- Accepted semantic revision, comparison-anchor, and repository-cache changes
  cancel or invalidate the exact diff generation before clearing Qt geometry.
  A metadata-only accepted refresh with the same authority, base/head, branch,
  origin, identity generation, and current-review candidate preserves the live
  diff and semantic selection. Failure, invalidation, and shutdown close the
  selected context against late publication.
- Font, style, palette, screen, DPI, viewport, and splitter changes invalidate
  measured geometry while stable semantic selection remains core-owned.
- Loading, ready, failure, invalidation, and user selection changes produce one
  combined, polite, consecutive-duplicate-suppressed workbench announcement.

## Verification record

- `qt_diff_workbench_scale_{100,125,200}` drives the production composition at
  1.0x, 1.25x, and 2.0x. It covers both-pane/divider wheel routing, pane-local
  horizontal input, insertion/deletion pinning, unequal replacement, 1,000
  reversals, and scrollbar and routed Home/Page/End against the actual native
  pane scroll values and rendered viewports. It also covers reveal/ribbon/line
  selection, explicit focus order, polite deduplicated terminal announcements,
  internal detail-splitter viewport remeasurement and tail reachability,
  resize/font/DPI/theme invalidation, exact failure, and file/epoch/options
  stale publication.
- `application_diff_workbench_public_path` traverses the real GitHub fixture,
  epoch/catalog/cache actor, installed-Git blob reads, diff host, selected Qt
  workbench, and shutdown. It proves a same-semantic accepted refresh preserves
  the exact result/context/selection and a changed head clears them. Its saved
  full-window raster was directly inspected and visibly contains the source
  panes, insertion wedge, selected source line, shared scrollbar, and lower
  detail.
- `application_diff_workbench_shutdown_public_path` begins shutdown with the
  selected diff still pending; all hosts and both effect lanes quiesce without
  a late Qt publication. `application_epoch_maintenance_public_path` first
  renders a live selected diff, then proves whole-cache deletion clears its
  exact context and geometry.
- The native-appearance probe now requires a live selected diff, retained
  semantic selection, inherited palette/style/font, a post-System-theme
  geometry rebuild, and differing full-window pixels. The deterministic probe
  contract passes; the owner-driven Cocoa Light-to-Dark run and target-native
  Windows verification remain open without blocking later work. RFC-BPR-0005
  still owns the dedicated integrated presentation-polish pass.
- Independent closeout review found indirect native-motion evidence, stale
  internal-splitter geometry, over-eager refresh clearing, and duplicated or
  missing accessible announcements. Each finding received a production fix
  plus direct selected-composition coverage; follow-up review reported no
  remaining blocker. The corrected focused closeout matrix passed 12/12 in
  367.88 seconds, followed by 8/8 link-map/workbench/public-path checks after
  the final announcement correction.
- The final whole-graph audit then exposed an over-broad owning byte container
  on the installed-Git process port and three strict-analysis findings in the
  application fixture composition. The process contract now publishes its
  already bounded exact bytes synchronously through a narrow sink; the cache
  session remains the only owner. The corrected architecture contract, exact
  Git process materialization contract, affected strict-tidy set, formatting,
  documentation, whitespace, and immutable-RFC checks all pass.
