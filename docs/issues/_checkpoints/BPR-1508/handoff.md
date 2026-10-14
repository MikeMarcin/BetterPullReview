# BPR-1508 milestone checkpoint

Assignment: BPR-1508 only, isolated worktree
`/Users/jaeger/.codex/worktrees/bpr1508-search-markers/BetterPullReview`, branch
`agent/bpr1508-search-markers`. Mandate: `owner-note-2026-10-14-bpr1508`.
Original baseline: `294395bf3`. Claim: `61603a3ddf5ee0dacd55bdfc0480ff1d4633d214`.
Product candidate: `df00148b686bc2561b59ca257dadb14d035e2b28`.
The coordinator integrates the scoped claim, product and acceptance commits;
the interleaved testing-note import commits already exist on main and must not
be integrated again.

## Outcome and design

The required `frontend-design:frontend-design` skill was read from
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`.
The existing inherited fonts, colors and fixed L/R slots remain. Match rows now
use the full viewport, with the normal four-pixel content inset; native file
rows retain their disclosure gutter. Matching, corpus identity, source reveal,
query state, effects, persistence and review policy are unchanged.

The local tree overrides match item rectangles so repaint/accessibility geometry
includes the marker area, suppresses match-row branch painting, and leaves
native file disclosure and keyboard handling with QTreeView. The delegate uses
that same full-width match rectangle. This avoids removing the group's native
arrow merely to remove its children's indentation.

## Changed paths

- `src/adapters/qt/src/source_search_panel.cpp`
- `tests/adapters_qt/source_search_panel_tests.cpp`
- `tests/end_to_end/source_pr_find_public_path_tests.cpp`
- `docs/architecture/source-search.md`
- BPR-1508's canonical card, this checkpoint, `selection.json` and `binding.json`
- `docs/rfcs/plans/bpr1508-search-markers/milestone-plan.json` and `source.json`

## Verification

macOS 26.6.2, arm64, Qt 6.11.2, Debug, offscreen; no owner desktop was driven or
captured. All fixtures are the committed sanitized source-search fixtures.

- Dedicated `cmake --preset dev` configure passed, using the shared verified
  dependency source cache at
  `/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache`.
- Built `test_better_pull_review_qt_source_search_panel_tests` and
  `test_source_pr_find_public_path` with `cmake --build --preset dev --target … -j 3`.
- `ctest --test-dir build/dev -R '^qt_source_search_panel_scale_(1|2)$' --output-on-failure`:
  both passed. Raster checks locate both colored side tags at their fixed slots;
  all match rectangles start at zero and the tag column at four. Marker clicks,
  Up/Enter, native group disclosure, side filters, counts, selection, highlights
  and narrow/wide text-column alignment pass.
- The same panel executable with `-style Windows` passed offscreen at 1x,
  exercising the native branch-style difference.
- `ctest --test-dir build/dev -R '^application_source_pr_find(_scale_(1_25|2_0))?$' --output-on-failure`:
  all three passed (31.02 seconds total). The shipped application opens the
  fixture through its installed-Git/cache composition, renders both result
  sides, clicks the left marker to load/reveal the exact left source span, and
  verifies Enter/F4 exact-side activation and focus, filtering and no-rescan traversal.
- Inspected the full-window `pr-search-left-aligned-markers.png` and narrow
  light panel raster: tags begin at the left inset, both slots remain distinct,
  line numbers/snippets align and native file arrows remain visible.
- Negative control: restored only the original baseline's panel source, rebuilt
  the panel target and ran its new 1x regression. It failed with CTest exit 8 at
  `side markers start at the viewport's left content inset`. Restored the exact
  candidate bytes, rebuilt both targets and the 1x panel passed again.
- `architecture_boundaries` passed. Documentation verifier tests: 67 passed.
- Exact clang-format 22.1.8 checks and `git diff --check` passed.
- `python3 scripts/verify_documentation.py --root .` passed after synchronizing
  the coordinator's import-only main history through `9b551b738` (NEXT_ID 1520).
- A post-commit rebuild and public-path receipt on exact candidate
  `df00148b686bc2561b59ca257dadb14d035e2b28` passed the 1x rendered panel and
  composed application course (12.53 seconds total). The milestone checkpoint
  reported `stop` with every bound obligation and exit recorded passing;
  its mechanical check explicitly does not supply engineering acceptance.

Evidence root (untracked, owner-private):
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1508-g9q41o71`.
It retains build/negative logs, panel renders, full application renders and the
post-commit candidate public-path receipt. Screenshots and logs are not committed.

Source blob: `43b6b83d6286758619543da39dddaa91e2345918`.
Panel test blob: `7025cdb4cdfd97a7e41c3713c10e98dce925aa75`.
Composed test blob: `c313172d3a8f884060ce76089669300f4e2101a5`.

## Workflow and continuation

Resolved/preserved coordinator-implementer recipe revision 2; one leaf owns this
worktree. MASK registration was repaired through its reviewed register plan.
Preflight is ready for the configured Codex gpt-6.1-sol/xhigh implementer;
filesystem readiness does not prove host loading. Initial lock-busy/stale-plan
refusals were reconciled without manual registry edits. Global issue-counter
advances during note intake required coordinator-authorized import-only commits;
no issue ID was independently allocated or counter value manually edited.

No human-only criterion or native acceptance was added by this correction, so
there is no pending attended decision or review server. Offscreen evidence does
not claim native system-appearance, VoiceOver, Windows or Linux qualification.
No source writer or task-owned background command is handed off. Build/test
commands must all be completed before final return; the runtime evidence is
retained and there is no temporary alternate source file to restore.

Next action: coordinator integrates the scoped BPR-1508 commits and composes the
adjacent `source_pr_find_public_path_tests.cpp` hunk with BPR-1509's separate tab
interaction work. There are no remaining BPR-1508 product obligations.
