---
schema: "implement-rfc.card.v1"
id: "BPR-1509"
title: "Style Files and Search tabs like Comments and Notes tabs"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Lets makes files and search tabs more like comments notes tabs style","milestone_plan":{"id":"bpr1509-drawer-tabs","path":"docs/rfcs/plans/bpr1509-drawer-tabs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8f11e6083ad1545f97e3b0b741e0dd3f42006ae6","evidence":{"kind":"observation","detail":"Full-window offscreen native document-tab renders inspected at 1x, 1.25x and 2x; both drawer headers share the same edge.","root":"/tmp/bpr1509-20261014"}},{"criterion":"AC2","outcome":"pass","revision":"8f11e6083ad1545f97e3b0b741e0dd3f42006ae6","evidence":{"kind":"observation","detail":"Actual mouse/window-system key path, native page names and selected accessibility state, disabled keyboard skipping, focus/hover/disabled renders verified.","root":"/tmp/bpr1509-20261014"}},{"criterion":"AC3","outcome":"pass","revision":"8f11e6083ad1545f97e3b0b741e0dd3f42006ae6","evidence":{"kind":"observation","detail":"Exact selected file/comparison, Files filter, Search query/results/current occurrence/count, drawer width and existing Close/Escape behavior retained; collapse/rail/Hide course passes.","root":"/tmp/bpr1509-20261014"}},{"criterion":"AC4","outcome":"pass","revision":"8f11e6083ad1545f97e3b0b741e0dd3f42006ae6","evidence":{"kind":"observation","detail":"Real composed application through installed Git/cache; old-header negative fails and restored candidate passes. No explicit human criterion exists; coordinator confirmed no new gate.","root":"/tmp/bpr1509-20261014"}}]
---

# BPR-1509: Style Files and Search tabs like Comments and Notes tabs

Use the existing Comments and Notes tabs as the visual reference for Files and Search tabs. Match the established tab treatment, spacing and selected/hover/focus states while retaining the Files and Search content and behavior.

## Acceptance

- AC1: Files and Search tabs use a coherent visual treatment matching the existing Comments and Notes tabs.
- AC2: Selected, hover, focus and disabled states remain clear and keyboard/accessibility behavior is preserved.
- AC3: Switching tabs preserves existing file selection, search state and drawer behavior.
- AC4: Verify actual offscreen rendered states and the composed tab-switch interaction, and prepare any required owner visual acceptance on an exact frozen candidate.

## Coordination

Load frontend-design:frontend-design before UX work as required by AGENTS.md. BPR-1508 owns L/R result marker indentation inside Search; coordinate shared files without taking over that scope. Never operate or capture the owner’s live desktop.

## Milestone plan and design

Mandate `bpr1509-owner-testing-note-20261014-r1`; baseline
`2f4aed1b4`. This isolated worktree owns only BPR-1509. No prior handoff
exists for this new note. The adopted one-card plan is
`docs/rfcs/plans/bpr1509-drawer-tabs/milestone-plan.json`.

Required `frontend-design:frontend-design` was read from
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`.
The brief fixes the design reference: Comments/Notes, so Files/Search takes
its native document mode, content-sized labels, base line and keyboard focus.
Color tokens remain the inherited QPalette Window, WindowText, Base, Highlight,
HighlightedText and Disabled roles; the native style supplies their actual
colors. Type remains the inherited UI font. No new palette or font is needed.
The left-aligned row stays `Files | Search` above the existing page. Existing
page widgets retain their selection, filters, query, results and scroll state.
The plan review rejects decorative changes because they would depart from the
owner's named reference. BPR-1508's result marker indentation is independent.

MASK registration was initially missing; reviewed register plan
`sha256:e05dd2f7868b3b1769aeccc6724c1ecdb97dbb3e696e1579f902cf1b622f8cc2`
created declared local links and native role files. Preflight is ready; host
loading is not certified by it. Runtime evidence: `/tmp/bpr1509-20261014`.
Canonical claim first refused the stale counter, then the plan-edited card.
Coordinator authorized importing main's four testing-note commits through
`9c80010a0`; their allocation bytes were cherry-picked unchanged. This setup
commit preserves the plan before canonical claim retries.

## Execution record

Verified implementation on 2026-10-14.

Candidate `8f11e6083` on `bpr1509-drawer-tabs` uses the same native document
mode, compact tab labels, base and StrongFocus setting as Comments/Notes.
Files/Search now forms the drawer header; the redundant Files heading is
hidden while the `Files drawer` accessible name remains. Existing page
widgets and all application/core search policy remain unchanged.

The baseline composed source-search course passed (11.65 s). The revised
course drives mouse clicks and window-system Left/Right keys, checks native
page names and selected accessibility state, disabled-tab skipping, file and
filter retention, exact query/result/current-match retention and drawer width.
Search entry retains its existing query focus. The old-header negative control
fails the header-edge assertion; restoring this candidate passes (9.09 s).

- AC1: Full-window offscreen renders at 1.0×, 1.25× and 2.0× show compact
  Files/Search and Comments/Notes at the same header edge, with Files and
  Search selected in turn.
- AC2: Header keyboard reachability, Left/Right selection, accessible tab names
  and selected state, and disabled Search navigation pass. Focus, hover and
  disabled rasters were captured and inspected. Stock Qt's per-tab disabled
  accessible bit was not asserted: it stayed false in the native interface;
  availability, skipped keyboard navigation and grey disabled text are verified.
- AC3: The composed course retains selected comparison/file, Files filter,
  Search query, immutable results/current occurrence/count and drawer width.
  Existing Close, Escape and narrow-drawer courses also pass. The workbench
  collapse/rail/Hide-button/width course passes after updating its obsolete
  visible-heading expectations (42.16 s).
- AC4: Actual production composition runs through the installed-Git/cache actor.
  No governing criterion requires a human verdict; coordinator confirmed on
  2026-10-14 that no new human gate should be added. No attended session or
  frozen build is required for this slice.

Evidence root: `/tmp/bpr1509-20261014`. Passing logs: `baseline-public.log`,
`public-native-states.log`, `restored-public.log`, `drawer-regression-final.log`,
`integration-scales.log` (source scale rows and composed GitHub row pass),
`documentation-tests.log` (67 tests), and `tidy.log` (product and search-course
translation units). `negative-public.log` is the deliberate old-header failure.
Initial focus and disabled-accessibility assertions were corrected to test the
existing native behavior; their actual failures remain in `candidate-public.log`,
`focus-diagnosis.log` and `final-public.log`.

The initial integration batch failed only its obsolete visible-heading
expectation; its focused corrected recheck passed. Full workbench-test tidy
reports six existing diagnostics on unchanged lines (four local naming cases
and two copied command locals), recorded in `tidy-drawer-tests.log` and
mechanically compared to baseline in `tidy-preexisting.json`. No diagnostics
point to this slice's changed assertions. Canonical clang-format 22.1.8 and
`git diff --check` pass for all three touched C++ files.

Render roots (uncommitted): `build/dev/source-pr-find-renders`,
`build/dev/source-pr-find-renders-1_25`, `build/dev/source-pr-find-renders-2_0`.
Each contains `drawer-tabs-files.png`, `drawer-tabs-files-focus.png`,
`drawer-tabs-search-focus.png`, `drawer-tabs-files-hover.png`,
`drawer-tabs-search-disabled.png` and `drawer-tabs-search.png`.
Rasters were inspected at all three scales. Context: macOS 26.6.2 arm64,
Qt 6.11.2, offscreen, inherited default style, 1280×860 logical window plus
the existing 900×760 and 860×760 narrow courses. These verify deterministic
layout and interaction, not native desktop appearance integration.

Documentation impact: `docs/ux/workbench-specification.md` states the shared
header treatment and retained state. No API, authority, persistence, identity,
provider, launch, packaging or security contract changed. BPR-1508 retains the
independent Search marker indentation scope.

Closeout remains In Progress only for tracker synchronization and the final
structure gate/canonical finish. The gate currently refuses local NEXT_ID
BPR-1515 against shared historical high water BPR-1520. Coordinator's explicit
tracker import authorization currently ends at `a1130fccb`; later main import
commits `fa776388e` through `9b551b738` require coordinator reconciliation.
No allocation bytes were edited or independently allocated by this implementer.
The prepared acceptance is `docs/issues/_checkpoints/BPR-1509/acceptance.json`.
All task-owned build/test/tidy commands have ended; no review server exists.

### Final tracker reconciliation

Coordinator resumed this implementer and authorized existing metadata-only
main commits `a1130fccb..3e8793add`. All nine commits were inspected and
cherry-picked sequentially, preserving BPR-1520's P0 and owner amendments.
NEXT_ID is now BPR-1521; no allocation bytes were independently edited.
`documentation-final.log` reports `DOCUMENTATION_OK`. Exact product, test and
specification paths are byte-identical to verified candidate `8f11e6083`, so
all unchanged passing evidence is reused. Canonical acceptance is prepared
in the recorded `acceptance.json`; no product work or acceptance is pending.
