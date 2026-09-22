---
schema: "implement-rfc.card.v1"
id: "BPR-0977"
title: "Persist independent native window geometry and verify focus lifecycle"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0707", "BPR-0708", "BPR-0711", "BPR-0932"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0024-closeout-r2"}
---

# Persist independent native window geometry and verify focus lifecycle

## Scope

Complete RFC-0024 AC-03/06/07/09's per-window geometry and native focus/lifetime
obligations. Retain bounded window-local geometry through Quit/restart while
explicit Close removes only that window. Restore usable geometry on changed
displays and keep the MRU focus target distinct from stable identity.
Native presentation remains Qt-owned; host ports own durable persistence.

Owned surfaces: window presentation/geometry contracts, profile window
restoration/layout storage and composition, focused Qt/storage/public courses,
and candidate-bound native evidence. Do not change RFC-0018 follow-ups
BPR-0974/0975 or claim offscreen evidence as native acceptance.

## Acceptance

- Two same-profile windows retain distinct usable native geometry across
  Quit/restart; corrupt/unavailable or obsolete-display geometry has a bounded
  visible/reachable fallback without borrowing another window's record.
- Explicit Close versus Quit preserves the intended restoration set; repeated
  activation selects the MRU window and closing one leaves the sibling usable.
- Focus, geometry and lifecycle are observed on the required available native
  surfaces with exact candidate identity; unperformed native observations
  remain pending rather than being replaced by offscreen assertions.
- Focused persistence, Qt and composed restart checks pass.

## Evidence

### Unattended implementation — 2026-09-21

Candidate working tree over claim `afecca5e`. A bounded five-field native
placement joins only the window layout row: normal x/y, width/height and
maximized state. Zero size means no saved placement. Schema nine migrates
versions one through eight transactionally; version-eight Home and drawer
state survive exactly. Geometry is not a profile preference. Qt owns screen
observation and one-time fitting to current available displays; host effects
own coalesced persistence and orderly Quit drains. No native move/resize echo
is projected back during ordinary interaction. Before load, initial OS
placement events cannot overwrite the unread record. Malformed or unavailable
storage retains the existing visible failure and usable default window.

Direct evidence: `build/bpr0977-geometry-public.json`, **4/4 pass, 25.531 s**:
`sqlite_workbench_settings_storage`, `application_workbench_host`,
`qt_native_window_geometry`, and
`application_profile_window_layout_public_path`. The composed course records
two distinct observed native rectangles, checks exact durable rows, restarts
before opening a review, checks both rectangles, explicitly closes A, retains
B, creates a fresh window and exercises future-schema fallback. These are
offscreen native-widget observations, not native window-manager evidence.

The first integration pass caught the explicit old migration admission bound;
it now admits version eight and preserves Home. A failed settings load still
publishes no invented layout. The composed course now waits for startup
placement before its deliberate review-test resize; every existing exact
drawer-width assertion remains. The targeted Qt course passes obsolete-display
fitting, post-restore observations, one-time application and maximized/normal
geometry. Source/self-review, composition and final native evidence are
recorded in the continuation below.

Native acceptance remains pending. Windows quarantine and Linux WSLg focus
limitations recorded for RFC-0018 are not passing RFC-0024 evidence, and its
owner amendment does not transfer. BPR-0974/0975 and retained station roots
remain unchanged.

Self-review covered zero/partial/out-of-bound geometry, negative coordinates,
changed displays, maximized normal placement, startup unread records,
reentrant projection/observation, serial/coalesced writes, orderly shutdown,
profile-default rejection, window-row cleanup and v8 Home migration. The
code-simplifier pass kept geometry in the existing bounded layout contract and
effect lane rather than adding a parallel store or lifecycle authority.

Composition pin review from `8d072d14` covers exactly one new six-line callback:
Qt's bounded geometry observation calls the invoking window's WorkbenchHost
only while open. There are no credential/provider/mutation changes. Full app
digest `a9b02743a1c5483c38229caf1df5d3c2e6eccec931f555bab5772649149e0c5f`;
production manifest `88087e6b5ec141b78e8c7d6b05e9bb2eb8d72e4035a86fc13fe1899bb63a765b`;
mutation branch remains `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.

Final unattended pass: **5/5, 37.032 s**, including the real pinned mutation
composition, strengthened profile-default refusal, exact geometry storage and
restart (`build/bpr0977-final.json`). Boundary and documentation gates pass;
changed-line formatting and diff checks pass. Native criteria are deliberately
not marked Done. No task-owned command remains after the recorded tests.
