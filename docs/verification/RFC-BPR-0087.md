# RFC-BPR-0087 verification and continuation

Current frontier: the app-wide source is complete at `81a132674`, with technical
AC01–AC05 evidence reconciled below. BPR-1661 is verified; BPR-1662's separate
integration authorization remains pending under the explicit no-merge/push/release
mandate. The RFC stays **In Progress**. The initial disk pause and first-slice
pending rows below are historical; the final proposal supersedes that frontier.

## Initial candidate and disk pause

- RFC import: `6fcdd1537`, based on immutable RFC_BASE_SHA
  `74ef7f4fb643b2fbbcd4753efcd1fe29c38bbddc`.
- First source slice: `2660e4c63096479bb5d78090017e9f0e8d898e28` on
  `codex/shared-action-buttons`.
- Isolated worktree:
  `/Users/jaeger/Documents/Codex/2026-10-19/task-4/shared-action-buttons`.
- The delegated implementer used `author-rfc`, `implement-rfc` and the required
  `frontend-design:frontend-design` skill. The design follows BPR-1577's accepted
  v002 chip; no new visual direction was substituted.
- At the initial handoff BPR-1659 was Blocked on `disk-space`. The user's disk-space steering paused
  large builds/downloads before any were started. `df -h .` observed about
  1.0–1.1 GiB available on the Data volume. This task started no build process,
  download, frozen candidate or large generated output. No task-owned command
  remains active at handoff. The source worktree must be preserved.
- Root README belongs to the separate README task and was excluded from all
  commits. No existing release worktree, release artifact, merge, push or
  release was changed by this implementation.

## Initial source checkpoint

`button_surface` now owns the palette-derived chip, hover/press and keyboard
focus treatment. Overview retains its mark/content and measured geometry.
`quiet_tool_button` adopts the same surface for Review/Comments option and Reset
factories and Appearance tab-width choices. Native menu, arrow, icon-only and
stacked icon/text variants retain their own painting. QPushButton migration is
future work, not a tool-button substitution.

The [inventory](../ux/shared-action-buttons.md) covers the 46 translation units
with direct push/tool-button construction or page-local subclasses and names
the separately declared specialized classes. Pending entries remain BPR-1660's
work. The RFC and first milestone remain open.

The existing `qt_feedback_label_settings` course now includes a composed
four-theme check of the new Settings choices: selected/bare/Reset pixels,
hover and press, keyboard focus and Space, exclusive dispatch, disabled
rendering and refused disabled clicks. At that handoff this test source had compiled in
syntax-only mode but had not run. The resume evidence below now verifies its
pixel thresholds and runtime behavior. The existing Overview switch test remains
the regression check on the accepted reference.

## Lightweight checks actually performed

On macOS arm64 with the existing Qt 6.11.2 dependency headers:

| Check | Actual result |
|---|---|
| Syntax-only compiler check of `button_surface.cpp`, `quiet_tool_button.cpp`, `pull_request_overview_page.cpp`, `appearance_settings_panel.cpp`, `workbench_window.cpp` and `feedback_label_settings_tests.cpp` | All six exit 0 with the existing dev compile flags, including warnings as errors. Commands were derived from the main dev compilation database, remapped to this worktree, with `-c`/object output removed and `-fsyntax-only` added. This is not a build/link/runtime pass. |
| `python3 _coding-standard/scripts/format.py --dry-run` on all eight touched C++ headers/sources | Pass, clang-format 22.1.8. |
| `python3 _coding-standard/scripts/tidy.py --root . --compile-commands /Users/jaeger/Documents/Codex/2026-10-19/task-4/syntax-checks/compile_commands.json --paths src/adapters/qt/src/button_surface.cpp src/adapters/qt/src/quiet_tool_button.cpp --warnings-as-errors` | Pass, clang-tidy 22. Initial pragma-once findings were corrected to include guards; final run has no first-party findings. |
| `cmake -DBPR_ROOT=/Users/jaeger/Documents/Codex/2026-10-19/task-4/shared-action-buttons -P cmake/VerifyArchitectureBoundaries.cmake` | Pass: application, provider and Qt architecture boundaries verified. |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK`. |
| `git diff --check` | Pass. |
| Shared milestone-plan check and checkpoint binding | Structurally valid; bound to `shared-settings`. Two missing later-milestone boundary fields were corrected without changing scope or exits. Structural validity does not certify semantic acceptance. |

## Initial acceptance frontier (superseded below)

| Criterion | State |
|---|---|
| AC01 | Source extraction and first Settings adoption written; actual rendered behavior pending. |
| AC02 | Initial inventory written; app-wide adoption explicitly incomplete. |
| AC03 | Existing Qt interaction preserved by design and new regression source written; runtime interaction and accessibility checks pending. |
| AC04 | No new runtime renders inspected. Four themes, three scales and style variants remain pending. Native Windows/Linux and macOS appearance integration are unobserved. |
| AC05 | Lightweight checks above pass; focused build, runtime tests and composition checks remain pending. |

## Resume after disk headroom is confirmed

Keep using this worktree. Do not rebuild inside another task's tree or use a
previous executable as evidence of this source. Configure an isolated dev
build using the already available dependency cache where appropriate; do not
start another large download on the full disk.

Build the affected test targets, then run focused tests serially:

```sh
cmake --build --preset dev --target test_better_pull_review_qt_feedback_label_settings_tests test_better_pull_review_qt_pull_request_overview_page_tests test_better_pull_review_qt_appearance_settings_panel_tests test_better_pull_review_qt_settings_window_tests test_better_pull_review_qt_workbench_window_tests
ctest --preset dev --output-on-failure -R '^(qt_feedback_label_settings|qt_pull_request_overview_page|qt_appearance_settings_panel|qt_settings_window(_scale_(125|200))?|qt_workbench_window_scale_(100|125|200))$'
```

Also build/run `qt_composed_github_scale_100` as the first composition-focused
integration check. Inspect the real Settings Review/Comments/Appearance and
Overview captures through the test render hooks, including the new shared
choice test. Run the relevant courses at scale factors 1, 1.25 and 2 and with
available style overrides. Treat offscreen style renders as layout evidence,
not native OS appearance acceptance. Fix any runtime failures before finishing
BPR-1659, then continue BPR-1660's pending inventory. No merge, push or release
is authorized by this handoff.

## Resume evidence — BPR-1659

Candidate `9dc946997350ab23866ce75f39bbebf32b140be8` verifies the first
Settings milestone. The owner's cleanup restored approximately 54 GiB free;
this task used one isolated build tree (about 892 MiB), the existing shared
dependency cache, offline acquisition and two compiler jobs. No duplicate Qt
dependency tree, live service mutation, desktop capture, merge, push or release.
The owner subsequently explicitly authorized continuing the RFC's app-wide
rollout. Diagnostics task `01a15617-5e85-77ec-a6ca-fa8dad92701f` received disk/load
coordination; this task keeps using the existing build tree.

Configuration: `cmake --preset dev -DBPR_DEPENDENCY_OFFLINE=ON
-DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache
-DCMAKE_CXX_FLAGS_DEBUG=-O0\ -g0 -DCMAKE_C_FLAGS_DEBUG=-O0\ -g0`.
The six affected Qt test targets built and linked successfully. The registered
network-deny test wrappers ran serially on macOS arm64, Qt 6.11.2, offscreen.

- The focused CTest selector from the initial continuation, plus
  `qt_composed_github_scale_100`, passes **10/10** (109.00 s): Overview,
  Appearance, Settings at 1x/1.25x/2x, workbench at 1x/1.25x/2x, composed
  GitHub and the new Settings state check.
- The serial style matrix passes **18/18**: Fusion and Windows styles at
  1, 1.25 and 2; each runs the composed Settings course (all four themes),
  Overview `--switch` (all four themes), and Appearance `--shared-buttons`
  (tab-choice interaction and four-theme Type-card captures). Settings logs
  record actual Qt version, platform, style and device-pixel ratio.
- Render root: `/Users/jaeger/Documents/Codex/2026-10-19/task-4/button-verification/`.
  Each style/scale directory includes complete Review/Comments windows,
  Review hover/press/focus/disabled states, full Overview and both switch
  states, and Appearance Type cards. `matrix-results.json` records the exact
  registered wrapper commands and exit codes. The local runner is
  `/Users/jaeger/Documents/Codex/2026-10-19/task-4/run-button-matrix.py`.
- Inspected Light/Dark Review and High Contrast Comments at 1x, keyboard-focus
  and disabled Dark at 1x, High Contrast Light Review and Dark Overview at
  1.25x, Windows High Contrast Dark Review at 2x, Windows Dark Type at 1.25x,
  and Fusion Light pressed state at 2x. Labels fit, selected choices have the
  shared quiet chip, unselected choices stay bare, the inset focus ring is
  distinct, and disabled text is visibly muted. Existing links, icon-only
  tools, fields and primary/specialized controls retain their roles.
- clang-tidy 22 passes on both new implementation units using this build's
  real compilation database. clang-format 22, documentation and whitespace
  checks pass on the capture additions.

### Retained unrelated scale limitation

The *full* Appearance suite at Fusion 2x fails its existing info-popover
placement assertion: a glyph at `(101,412)` lies beyond the offscreen logical
screen and the tooltip is constrained above/left instead of immediately below.
Rebuilding the exact candidate with only the tab-width construction restored to
its original `QToolButton` reproduces the identical failure/coordinates. That
control is logged in `appearance-native-choice-control.log`; the original
source was restored byte-for-byte and all six targets relinked afterward.
The normal-scale full Appearance suite passes. The matrix's explicit
`--shared-buttons` entrypoint checks the relevant tab choices/captures, with
this unrelated full-suite 2x failure retained rather than called a pass.
Native macOS appearance integration and real Windows/Linux desktops remain
unobserved; Windows here means Qt's offscreen Windows **style** on macOS.

### First milestone acceptance and continuation

BPR-1659's three acceptance bullets pass from the shared reference/component,
10 focused tests, 18 matrix courses and inspected renders. This accepts the
first milestone only. The app-wide inventory still has Pending entries owned
by BPR-1660; RFC-wide acceptance is incomplete. Continue the authorized rollout
in this worktree with one fresh implementer under the preserved
coordinator-implementer recipe. Registered main consumer preflight is ready
for Codex `rfc-implementer` at `gpt-6.1-sol` / `xhigh`; it does not itself prove
host loading. The worktree's standalone registration is absent, so the
registered main consumer supplies role readiness while all edits/builds stay
in this isolated lane. No task-owned command remains running at this milestone
boundary.

## App rollout — BPR-1660

Source candidate `81a132674` on `codex/shared-action-buttons` completes the
`app-rollout` implementation milestone. The fresh non-delegating implementer
used the shared engineering contract, coordinator-implementer leaf procedure
and frontend-design skill; BPR-1577 v002 remains the design authority. Native
host loading was not observed. Ready move `db3eee5a7` and claim `e5de4d357`
preceded product edits. `milestone_checkpoint.py bind` bound `app-rollout` with
`shared-settings` accepted, run `shared-action-buttons-r1`, segment `app-rollout`.

`quiet_push_button` now shares the existing surface painter beneath
QPushButton. It preserves push/dialog input, defaults, mnemonics, accessibility,
icons and translated sizing. Default, menu and icon-only buttons retain native
painting; deliberately flat secondary actions remain bare at rest. The
[inventory](../ux/shared-action-buttons.md) resolves all 46 initial translation
units, including products of mixed factories. It records the precise controls
migrated and why primary/default, destructive, navigation, menu, hyperlink,
icon-only, drawer and other semantic variants remain. No migration is deferred.
There are no core, host, settings-schema or command-authority changes.

### Actual verification

All commands used this worktree's existing offline `build/dev` with `-O0 -g0`,
the existing main CPM cache and at most two compiler jobs. Final build size is
about 1.2 GiB and the Data volume has about 53 GiB available. No dependencies
were downloaded or duplicated. The Qt adapter and 21 selected test executables
built and linked with warnings as errors. The first build invocation named a
nonexistent composed-target alias and stopped before compilation; it was
corrected to `test_better_pull_review_qt_github_tests`.

- `python3 /Users/jaeger/Documents/Codex/2026-10-19/task-4/run-app-button-courses.py`
  passes **23/23** focused tests in **87.07 s**. It invokes CTest serially with
  the registered macOS network-deny wrappers. Exact CTest arguments, selected
  names and results are retained in `button-verification/app-rollout/` as
  `focused-command.json`, `focused.log` and `focused-results.xml` (all local,
  outside the source tree). The courses cover composed GitHub, workbench,
  Home refusal/list, inline drawers/resident filters, device authorization,
  conversation finder, epoch comparison, agent launch, Profiles, Markdown,
  private review/composition, suggestions, review cycles, Settings, source
  search, pending/refresh feedback, drawer parts and profile reset views.
- The new `qt_quiet_buttons` course checks Space activation, dialog Return and
  primary default behavior, accessibility name/description/role, disabled
  input, translated/font sizing and text/icon space. Exact rasters match a
  native QPushButton for default/menu/icon-only variants. Actual diagnostic
  choice dispatch remains exclusive and retains the accepted value until the
  host publishes its replacement; busy controls refuse dispatch. Profiles
  retain primary Open/Save and working Rename/Cancel behavior.
- Two initial assertions in that new test were corrected, with no corresponding
  product defect: macOS native QPushButton does not expose the assumed Alt+C
  shortcut in this environment, so the test now checks platform-native mnemonic
  parity; diagnostics intentionally restore the accepted selection before
  dispatch, so the test now observes publication before expecting a new check.
- `python3 /Users/jaeger/Documents/Codex/2026-10-19/task-4/run-app-button-matrix.py`
  passes **6/6** four-theme courses: Fusion and Windows styles at scale factors
  **1, 1.25 and 2**. Measured on macOS arm64; facts in each `quiet-buttons.log` verify
  Qt **6.11.2**, `platform=offscreen`, actual style and DPR. These are style
  variants on this Mac, not native Windows or Linux measurements.
- The same matrix runner's extra Windows-style 1x `qt_review_cycle_view_1_0`
  course passes. `ctest --preset dev --output-on-failure -j 1 -R
  '^qt_scoped_prose_editor$' --output-junit
  /Users/jaeger/Documents/Codex/2026-10-19/task-4/button-verification/app-rollout/tour-writing-results.xml`
  also passes (0.67 s), exercising real tour writing buttons, captured selection,
  dispatch and cancellation.
- clang-format **22.1.8** passes on all 39 touched C++ files. clang-tidy **22**
  passes on `quiet_push_button.cpp` using the real dev compilation database.
  `VerifyArchitectureBoundaries.cmake`, `git diff --check` and
  `python3 scripts/verify_documentation.py --root .` pass.

### Render inspection and limits

Render root:
`/Users/jaeger/Documents/Codex/2026-10-19/task-4/button-verification/app-rollout/`.
The six `fusion-*`/`windows-*` directories hold **192 PNGs**: all four themes,
Diagnostics rest/pressed/focus/disabled, Profiles and full Settings Home & queue,
Notifications and Agents & context windows. Settings filenames use sidebar
indices 3, 7 and 8 respectively. `matrix-results.json` records exact wrapper
commands, exit codes, style/scale facts and render counts. The additional
`cycle-windows-100` directory has **27 PNGs** of the real review-cycle window,
share sheet, compact sheet, code/detail views and focus states.

Inspected Light Diagnostics and Dark Profiles at Fusion 1x; High Contrast Dark
Diagnostics keyboard focus at Windows 1.25x; Dark Notifications at Fusion 1.25x;
High Contrast Light Home & queue at Windows 2x; Dark disabled Diagnostics at
Fusion 2x; and the Windows 1x share sheet. Labels fit and remain readable,
selected/action controls use the quiet shared surface, unselected choices stay
bare, focus is visible and disabled controls are muted. The share sheet keeps
its primary Copy/Save and specialized note-code tools distinct. This is actual
widget/window layout evidence, not live desktop capture or human acceptance.

The earlier full Appearance Fusion-2x tooltip-placement failure and its
byte-identical native-choice control reproduction remain unchanged and recorded
above. No new focused test failed on the final candidate. Native macOS
appearance integration, native Windows/Linux desktop behavior and human visual
judgment remain unobserved. No merge, push, release, live-service mutation,
owner desktop control/capture, credential work or root README edit occurred.

### Durable handoff

BPR-1660's three acceptance bullets are satisfied by the completed inventory,
shared push/tool adoption, preserved variants, 23 focused checks, tour check,
6 style/scale courses and inspected composed renders. This accepts
`app-rollout` only. A fresh implementer must take `verification-closeout`
(BPR-1661/BPR-1662), reconcile RFC AC01–AC05 and the inherited platform limits,
and prepare the closeout proposal. The existing closeout cards' earlier
checkpoint text is historical and belongs to that successor's reconciliation.
Separate integration authorization is required; none is inferred here.
All task-owned builds/tests have exited and writer ownership returns to the
coordinator at this boundary. The source tree, build and local evidence remain
available for the successor.

## Final verification and closeout proposal

The fresh non-delegating `verification-closeout` implementer used the shared
engineering contract and selected coordinator-implementer recipe. The required
frontend-design skill was read from
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`;
the accepted BPR-1577 v002 treatment remains the reference. There is no visual
redesign or product-source change in this milestone. Generated route readiness
is configuration evidence only; native runtime host loading was not observed.

The reviewed source candidate is `81a13267446e883c12ac0f8c7b5a43927c2efc68`,
with first-slice runtime/capture additions at
`9dc946997350ab23866ce75f39bbebf32b140be8`. App-rollout acceptance and ledger
were committed at `29251e68d82fa87fa11b2299aec7c1262fb786fe`. Final verification
readiness `52662d1f2` and claim `bb64e195d` preceded this milestone's edits.
The complete source diff was assessed against immutable RFC_BASE_SHA
`74ef7f4fb643b2fbbcd4753efcd1fe29c38bbddc`: Qt presentation, its focused tests
and documentation only; no core/host command authority or settings schema changed.

### Criterion judgment

| Criterion | Judgment and direct evidence |
|---|---|
| AC01 | **Pass for the recorded offscreen scope.** Overview retains the accepted Open/All content and geometry over `button_surface`; Review/Comments choices and Reset plus Appearance tab choices use `quiet_tool_button`. First-slice 10/10 focused checks and 18/18 four-theme style/scale courses pass. Inspected composed captures show the shared quiet chip, bare unselected choice and unchanged labels. |
| AC02 | **Pass.** The [inventory](../ux/shared-action-buttons.md) resolves all 46 page/factory translation units and mixed-factory products as migrated or intentionally retained with concrete reasons; no migration is deferred. A broader baseline constructor/subclass scan additionally accounts for four dedicated specialized implementation units and one declaration header. |
| AC03 | **Pass for exercised Qt contracts.** The shared Settings and quiet-button courses exercise exclusive checked choices, accepted-value publication, one Space dispatch, disabled refusal, accessibility name/description/role, dialog Return/default preservation, native mnemonic parity, text/icon/menu fallbacks, translated sizing, font changes and four-theme palette changes. The 23/23 app-rollout composition courses, tour-writing course and final 4/4 smoke pass. Constructors retain object names, signal wiring, tooltips and focus policies; shared components retain their original Qt base class. |
| AC04 | **Pass for drawing/layout and interaction evidence, with native claims explicitly separate.** The 18 first-slice and 6 app-rollout courses span Fusion/Windows styles, four themes and 1x/1.25x/2x on macOS arm64, Qt 6.11.2, offscreen. Complete Overview, Review/Comments, Appearance, Diagnostics, Profiles and further Settings windows include hover/press/focus/disabled states; prior representative inspection and this milestone's six additional inspections are recorded below. |
| AC05 | **Pass for technical verification.** Prior 10/10 and 23/23 focused checks and composed integration are reused because source inputs are unchanged; the final serial public-path smoke passes 4/4. clang-format 22.1.8 passes all 47 changed C++ paths; architecture and documentation gates pass. Actual commands, revisions, failures, variants and platform limits remain recorded. Separate integration authorization is a remaining closeout-card obligation, not a technical test result. |

### Reused and final evidence

Local evidence root:
`/Users/jaeger/Documents/Codex/2026-10-19/task-4/button-verification/`.
Screenshots and logs remain outside Git. `matrix-results.json` contains the
18 first-slice command/result records; `app-rollout/matrix-results.json`
contains 6 push-button courses, each exit 0 with actual style/DPR facts. The
app-rollout's `focused-command.json`, `focused.log`, `focused-results.xml` and
`tour-writing-results.xml` retain the 23 focused checks and tour course.
The 192 push-course PNGs and 27 review-cycle PNGs remain available alongside
the first-slice captures. These are not native desktop captures.

This milestone directly inspected these additional complete widget/window
captures; all labels fit, selected/action surfaces are coherent, focus is
visible and disabled controls remain muted:

- `fusion-100/shared-buttons-review-light.png` and
  `fusion-100/shared-buttons-review-dark-hover.png`.
- `windows-125/shared-buttons-comments-high_contrast_dark.png`.
- `fusion-200/shared-buttons-review-light-pressed.png`.
- `app-rollout/windows-200/high_contrast_light-diagnostics-focus.png`.
- `app-rollout/fusion-200/dark-diagnostics-disabled.png`.

The final serial command at `bb64e195d93b2a46e35352659cd6e23686337ed5` was:

```sh
ctest --preset dev --output-on-failure -j 1 -R '^(qt_quiet_buttons|qt_feedback_label_settings|qt_appearance_settings_panel|qt_composed_github_scale_100)$' --output-junit /Users/jaeger/Documents/Codex/2026-10-19/task-4/button-verification/verification-closeout/focused-results.xml
```

It passes **4/4**, **7.39 s**, using this lane's existing offline build and
registered macOS network-deny wrappers. `verification-closeout/` retains the
exact argument array (`focused-command.json`), `focused.log`, XML and timing/
candidate (`focused-result.json`). No new build or dependency acquisition was
needed. All 47 `.cpp`/`.hpp` paths returned by `git diff --name-only
74ef7f4fb643b2fbbcd4753efcd1fe29c38bbddc -- src tests` passed
`python3 _coding-standard/scripts/format.py --dry-run` with those explicit paths
and clang-format **22.1.8**. The exact architecture command also passes:

```sh
cmake -DBPR_ROOT=/Users/jaeger/Documents/Codex/2026-10-19/task-4/shared-action-buttons -P cmake/VerifyArchitectureBoundaries.cmake
```

A concise simplification review found one shared surface painter under the two
native button classes, narrow opt-in/fallback guards and existing semantic
factory discriminators. No further refactor was needed. Baseline inventory
cross-check found 51 files: the 46 rows, four explicitly retained specialized
`.cpp` files and `discuss_header.hpp`; none is unaccounted for.

### Platform limits and retained failure

| Surface of evidence | Observed result |
|---|---|
| macOS arm64, Qt 6.11.2, offscreen, Fusion/Windows styles | Recorded layout and interaction courses pass at 1x/1.25x/2x in all four themes. |
| Native macOS appearance integration | Unobserved; offscreen forced styles cannot establish it. |
| Native Windows desktop/style integration | Unobserved; Qt Windows style running on macOS is style evidence only. |
| Native Linux desktop/style integration | Unobserved. |
| Owner visual judgment | Unobserved; no human criterion was introduced by this technical closeout. |

The existing full Appearance Fusion-2x popover placement failure remains a
**failure**, not a pass or skipped result. Its unchanged native-choice control
in `appearance-native-choice-control.log` reproduces glyph `(101,412)` and the
same assertion; the normal-scale full suite and focused shared-button 2x
courses pass. This is outside the migrated choice surface and does not block
the RFC's bounded technical criteria. No native-platform acceptance is inferred.

### Authorization and durable continuation

All authorized technical verification and proposal work is complete. BPR-1661's
evidence-recording criteria pass. BPR-1662 cannot be marked Done because its
unchanged second acceptance bullet also requires **separate integration
authorization**; that authorization has not been supplied and the user explicitly
prohibits merge, push and release. Its remaining blocker is
`integration-authorization`, with technical acceptance preserved. This is a
permission boundary, not a disk, build, visual-regression or native-test failure.
The RFC and index remain **In Progress**; Auto-Verified is only proposed after
that closeout obligation is resolved. No merge approval is requested here.

Binding and observation use run `shared-action-buttons-r1`, segment
`verification-closeout`, mandate `shared-action-buttons-r1`, the preserved
execution selection and both accepted prerequisite milestones. Local
`verification-closeout/checkpoint.json`, `observation.json` and `check.json`
retain that final continuation. The helper checks structure and fingerprints;
it does not establish truth of tests, host loading or authorization.

Preserve this branch, its single build and evidence for a future explicitly
authorized integration owner. Root README and unrelated work remain untouched.
At return, all task-owned test/gate commands are exited, writer ownership is
released and no task-owned background process remains.
