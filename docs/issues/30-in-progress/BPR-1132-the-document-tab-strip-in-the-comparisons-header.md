---
schema: "implement-rfc.card.v1"
id: "BPR-1132"
title: "The document tab strip in the comparison's header"
status: "In Progress"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129", "BPR-1131"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-05","AC-06","AC-07","AC-10"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1132: The document tab strip in the comparison's header

## Outcome

The comparison's header becomes the tab strip.

- Names elide from the left, where file names differ.
- A tab with its own pair shows it on its label.
- At a platform's width floor the strip shows the active tab and a count of
  the rest.
- The tab's menu offers Duplicate, Close, Close others, returning to the
  window's comparison, and making the tab's pair the one new tabs take.
- Cmd-click (Ctrl-click) in the Files tree and Open in New Tab open a tab.
- Ctrl+Tab and Ctrl+Shift+Tab move between tabs, and Cmd-W (Ctrl+W) closes
  one. If Cmd-W closes the window today, the window moves to Cmd-Shift-W
  (Ctrl+Shift+W).

The Updates line shows the active tab's pair, and the review bar's controls
act on the active tab. Overview and Discuss show the window's comparison. A
tab over an older pair places only its own revisions' threads, says how many
are on later updates, and offers to show them.

## Acceptance

- The composed course opens a file in a new tab, compares two pairs of one
  file side by side, closes, restarts and returns.
- The tab keys are unique in the assembled window.
- Tabs meet the focus, accessibility-name and 100 to 200 percent scale
  checks, at narrow and wide windows.
- The older-pair tray notice appears and routes to the later threads.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-05, AC-06, AC-07, AC-10 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.

## Historical checkpoint: stopped at the owner's wind-down (2026-09-25)

Claimed at `9d1b17a4`. At the wind-down no product code for this card was on
main. The work was saved as a patch against `c0fdd70a`; the resumed implementation
and verification below supersede that unfinished checkpoint:
[wip/README.md](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/wip/README.md)
(`BPR-1132-document-tab-strip.patch`, SHA-256
`830375e7239d9328585f0ba6dbbb3758200fa26288f6cb6c76fc37ba3f53eb0c`).

- **Built on the patch:** the tab strip in the comparison's header (in place of
  the diff status row), the tab menu and "+", the width floor's "+N", Cmd-click
  and Open in New Tab in the Files tree, the tab keys (Ctrl+Tab, Ctrl+Shift+Tab,
  Cmd-W; nothing had Cmd-W before, so no window command moves), the older-pair
  notice with Show them, Comment disabled off the current head, and the
  composition's per-window tab host (presenting loads through the epoch and
  workbench hosts, Overview and Discuss on the window's comparison,
  persistence, Resume, reset, shutdown).
- **Measured on the patch (macOS, 1280x1000, offscreen):** 872 folded and 838
  open, with the 28-pixel strip where the 15-pixel status and 6 of spacing were.
- **Passing on the patch:** the build, the window and focus-order courses at
  100 percent, the UIforETW capture at 872 and 838, and nine composed and
  end-to-end courses (listed in the README).
- **Not yet:** the Qt tests for the strip, keys, gestures, notice and Comment;
  the composed course (open in a new tab, two pairs of one file, close,
  restart, return); the reseal of the two `github_application.cpp` pins; format,
  tidy, the specification and this card's record.
- **Owner asks prepared for when it is done** (one question per criterion):
  1. The "+" at the strip's end opens the active tab's exact view in a new tab.
     Is that what the design's "+" should do?
  2. On a tab over an older pair the Comments tab shows "No threads on Feb 4 →
     Feb 9. Two threads are on later updates of this file." with Show them, above
     the thread list, where the later threads stay listed but unplaced. Should
     they be hidden until Show them?
- **Next step:** apply the patch at the current main, finish the list above,
  then the m2 integration pass.


## Resumed implementation, 2026-09-25

The existing claim is adopted under `rfc0052-20260925 / m2-resume`, mandate
`owner-20260925-m2-resume`, amended to prepare one fresh scoped attended session
after macOS verification. The saved patch is applied to main with unrelated
RFC import `fc4b39e8` preserved. Required Windows/Linux runs are deferred at the
owner's direction. Human and native acceptance is retained on [[BPR-1135]].

The strip receives a narrow application projection and sends core tab intents.
The application composes one tab host per window over its existing exact epoch,
file and semantic-selection restoration stages. New tests exercise two pairs of
one file, a duplicate with a separate reading position, background opening,
modality return, neighbour closure, process restart, Resume and closing the
last tab. The native shortcut delivery course exercises Control-Tab on macOS
and the platform close key, besides names, focus, overflow and file gestures.

The composed course found two faults in the saved patch: a return from Discuss
restored only the pair, and outgoing semantic-position observations could be
rejected after a switch. The composition now restores the complete active view
and queues tab gestures after their outgoing observation. Background operations
preserve the current load; unchanged file observations cannot recreate a closed
last tab. A file link entering Read supplies a new reading choice rather than
restoring the previous tab over the requested file. Explicit file navigation
immediately after a modality return also wins over its queued restoration;
the recorded-history setup course exposed and verifies that correction.

The integration pass also exposed a redundant diff request when a tab adopted
a file gesture already in flight, and a late tab write after Home cancelled
Resume. The composition retains the exact selected-diff request identity until
it is cleared or fails, so the tab driver reuses matching work instead of
superseding it. Tab persistence observes the same cancellation guard as reading
persistence. The workbench composition course keeps its exact single-request
assertion; the existing Home-during-Resume course compares the entire saved
record. Both failures are retained in the integration log. Self-review also
found that a plain click on the retained Files cursor emitted no selection
after the final tab closed. The tree now reports that explicit repeat click;
the restart course reopens the file with its press and release, and the Qt
course asserts exactly one ordinary selection dispatch. The composed restart
then caught a second cursor gate: the tree forwarded to the retained list, which
ignored the same current index. The window now explicitly dispatches that repeat
click, and the Qt course clicks again with both cursors already synchronized. The affected-path rerun
then exposed a cancelled diff for the Discuss window pair invalidating a tab
restoring another pair. Both diff failure callbacks now require the current
request and the tab target's exact file and comparison before failing that tab.
The composed course retains the exact pair and semantic-reading assertions.

### Composition-pin source review

The last matching pin is `fb4b8364`. `git log fb4b8364..fc4b39e8 --
src/app/src/github_application.cpp src/app/main.cpp` has no intervening commits;
this card's product commit is the complete changed pinned scope. Reviewed the
full diff, including the tab projection port, restoration adoption, exact
publication/epoch/file/selection checks, serialized outgoing observations,
modality and explicit link/file precedence, Resume entering Read for a saved
file, per-window persistence, exact in-flight request reuse and its failure
reset, failure ownership by request and tab target, the Home cancellation
guard, destination reset and shutdown/quiescence.
The test-only workbench probe now prints the fixed boolean/request-count facts
on an immediate selected-diff assertion failure, and the tab driver prints fixed
restoration-status facts on unavailability; their assertions are unchanged.
The UIforETW additions are confined
to the test-build capture, its schema and measured row budget. The mutation
branch has no change. No provider, credential, vault, transport or publication
capability is introduced. The source pins are resealed in the same commit as
this review record; passing the tripwire alone is not acceptance evidence.

## macOS verification — resumed slice

The resumed product commit is named in the acceptance records. Environment:
macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug. Interactive courses used the
offscreen platform and ran serially. Synthetic Qt key delivery and accessibility
names are automated evidence; they do not supply a native VoiceOver listen.

| Check | Result |
| --- | --- |
| `cmake --build --preset dev -j6` | Pass, including the final lifecycle corrections |
| `python3 _coding-standard/scripts/format.py --dry-run` on the 18 changed C++ files | Pass; the corrected application and final gesture files checked again |
| `python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths` on 10 changed translation units | Pass; the corrected application and final gesture files checked again; remaining diagnostics are pre-existing |
| Three `qt_document_tabs_scale_{100,125,200}` courses | Pass: path disambiguation, pair pill, projection stability, menus and plus, unique physical-key mapping, file menu and Cmd/Ctrl-click, focus/accessibility names, 1280/1100/platform-floor overflow, all Show them routes and disabled Comment off the current head |
| `application_document_tabs_restoration_public_path` | Pass: two pairs of one file, independent duplicate position, modality return, close neighbour, Quit, new process, Resume, exact order/pairs/positions, last-tab close and explicit reopen |
| 52-row m2 integration | 50 passed initially; the single selected-diff request and Home-during-Resume assertions exposed the two corrected lifecycle defects described above |
| 11 affected-path correction run | 10/11 passed, including tests-disabled Release composition and the full 557.70 s profile restoration course; tab return exposed failure ownership, corrected and verified below |
| Final failure-ownership and click-reopen correction | Final 9/9 pass in 179.47 s, including the complete tab/restart/reopen course in 94.10 s, exact-request workbench composition, all tab/window scales and Files tree |
| Final architecture and composition pins | 3/3 pass in 237.29 s: architecture boundaries, clean tests-disabled Release composition (208.98 s), and review round trip (11.87 s); mutation-branch pin remains unchanged |
| `application_large_review_history`, alone | Pass in 309.55 s; satisfies the prior handoff's idle rerun; no follow-up failure card needed |
| `application_uiforetw_selected_diff_contract` | Pass in all four appearance profiles: schema v7, tab strip `{x:0,y:78,width:1280,height:28}`, comparison 872 folded / 838 open at 1280×1000 |
| Documentation | `python3 scripts/verify_documentation.py --root .` passes; `python3 tests/scripts/verify_documentation_tests.py` passes all 64 tests |

Exact commands, test names, results and elapsed times are retained locally in
`/Users/jaeger/.codex/tmp/bpr-rfc0052-m2-yzl4rfoe/verification.json`,
`correction-verification.json`, `gesture-verification.json`,
`reopen-verification.json`, `pin-verification.json`, `integration-tests.json`
and `evidence/`.
The original failures remain there alongside the successful final correction run. The intermediate 5/6 run
passed tab switching and process restart but exposed the retained list cursor
suppressing the final reopen; the final 9/9 run covers that bridge as well.
Unchanged Qt and geometry evidence from the 52-row pass is reused; the final
corrections concern exact request reuse, request-qualified tab failures,
cancelled-restoration persistence and repeated file-click dispatch only.

### Owed elsewhere

The owner deferred Windows/Linux execution. Each station owes the earlier
m1/m2 card commands and, for this card:

```sh
ctest --test-dir <build> -j1 --output-on-failure -R "^(core_document_tabs|application_document_tab_host|qt_document_tabs_scale_(100|125|200)|qt_workbench_window_scale_(100|125|200)|qt_workbench_focus_order_scale_(100|125|200)|application_document_tabs_restoration_public_path|application_profile_window_review_restoration_public_path|application_workbench_composition_contract|application_uiforetw_selected_diff_contract|test_network_isolation_manifest|architecture_boundaries)$"
```

Measure Linux's folded/open heights (876/842 are derived), sweep the Windows
width floor and measure its 884/850 budget. A result below the owner's Windows
850 open budget needs the owner's decision, not a lowered assertion. Windows
native style/Narrator and macOS native VoiceOver and the owner's design answers
remain on [[BPR-1135]]. The fresh attended session contains the two questions
above; its decisions are not inferred from automated passes.
