# RFC-BPR-0036 verification

RFC base: `7ad0e43eb92c217ac6cd01fc478bde2968d217a0`.
Mandate r1: implement RFC-BPR-0036 in a worktree. Mandate r2 adds an explicit
md4qt/native-Qt GFM comparison before renderer selection. This ledger records
component evidence and the milestone-2 composed acceptance recorded below.

## Milestone 1 candidate and commands

Branch: `codex/rfc-bpr-0036-native-markdown`.
Plan/claim commits: `85a0184e`, `1322e913`.
Source candidate: `4812884e3670971b1b0bdc0b6e5d12fd81ddec48`.
Subsequent checkpoint edits change documentation only; the checked source remains
that candidate. Milestone 1's scoped obligation and all five exits pass. This is
not acceptance of the composed RFC or completion of BPR-0916.

Run `rfc0036-20260920`, segment `m1-comparison-r2`, mandate `r2`.
Selected recipe: coordinator-implementer revision 2; selection and binding are
`build/rfc0036-run/execution-strategy.json` and
`build/rfc0036-run/m1-checkpoint-r2.json` in this worktree. The installed skill
source is pinned by the launch assignment; runtime loading is not inferred from
its files. At that milestone, `/root/markdown_milestone_1` owned repository engineering.

Platform: macOS arm64, AppleClang 21.0.0, Qt 6.10.3, Debug developer graph.
Synthetic input only; no private PR body, live service call, publication or
receipt mutation. The component is not yet selected in shipped overview or
conversation composition.

Commands and local detailed logs under `build/rfc0036-run/`:

- `cmake --preset dev`; `cmake --build --preset dev --target
  test_better_pull_review_markdown_body_view_tests -j 6`: passed.
- `ctest --test-dir build/dev -R '^qt_markdown_body_' --output-on-failure`:
  native document corpus, source/copy, resources, keyboard links, bounds,
  refresh, forced-palette appearance and rasters at 1.0, 1.25 and 2.0 scale;
  3/3 passed in 2.04 seconds (`component-scales.log`).
- Architecture checks: `core_dependency_boundary`, `architecture_boundaries`,
  `architecture_boundary_contract` (including negative controls): 3/3 passed
  (`architecture.log`). Final source rerun of the first two: 2/2 passed
  (`architecture-final.log`).
- Touched-file format/tidy: passed (`format.log`, `tidy-final.log`).
- `python3 scripts/verify_documentation.py --root .`: passed;
  `python3 tests/scripts/verify_documentation_tests.py`: 64 tests passed.
- RFC board validation: valid. `git diff --check`: passed.

The component binary also passed directly with `QT_QPA_PLATFORM=cocoa`,
`BPR_AUTOMATED_TEST_BOUNDARY=1` and `BPR_MARKDOWN_RENDER_DIR` set to the absolute
`build/rfc0036-run/native-cocoa` directory in this worktree. The native 2x run
measured 61 ms for the 65,536-byte rendered body and 80 ms for the complete
262,144-byte source fallback (`native-cocoa.log`). These local observations include
component styling and stay below the test's one-second stress ceiling; they are
not a guaranteed frame budget on all hardware.

## Direct component evidence

The corpus checks native structures, including a real table and read-only task
markers, rather than searching a source string for Markdown tokens. It covers
headings, nested lists, emphasis/strong/strike, quote, inline/fenced code, tabs,
thematic break, links, image alternative text, raw HTML and final Unicode text.

Original UTF-8/CRLF copy is compared against the clipboard's raw text MIME data.
Unchanged identity/body retains the actual document pointer, selection offsets
and scrollbar. Mode round trips retain selection and separate reading positions;
replacement and clear discard prior body state. There is no asynchronous
preparation job whose late result could be published.

The resource test writes one synthetic PNG in a scoped temporary directory. A
plain Qt document successfully reads it as a negative control; the candidate
returns valid empty bytes for the same file, remote/data/resource references,
and never calls a counting global resource provider. Hostile source includes
script, iframe, stylesheet, form and image text. Rendering/resizing makes no
link dispatch. Explicit keyboard gestures emit an untrusted target only; host
admission, executable/local URL refusal and actual browser-path composition are
still milestone 2 obligations.

The 262 KiB single-line tail initially exceeded the local one-second stress
ceiling with source wrapping. Source now scrolls horizontally; no text is cut.
Qt expanded code tabs and propagated task markers beyond lists in the first
corpus run. The verified code-source repair and non-list marker clearing address
those observed failures. Tabs that cannot be mapped safely still have explicit
complete-source fallback, as specified in the
[reading contract](../architecture/native-markdown.md).

The native Cocoa run exposed a tab-chain loop, a bright alternate-base fill in
the dark code/quote palette, and an extra blank first table-header paragraph.
Explicit traversal to the source controls, window-palette code/quote fills and
bounded header-block cleanup correct those failures. The corrected native
capture was inspected: code, quotes, aligned bold headers, grid, tasks, image
placeholder and final Unicode text are readable. A separate wide-code capture
shows horizontal scrolling without forcing the component wider; the wide-table
case also passes the scrollbar/final-content assertions.

Rendered PNGs are runtime artifacts beneath
`build/dev/tests/adapters_qt/markdown-1_0`, `markdown-1_25`, and `markdown-2_0`.
They are layout evidence, not system appearance or VoiceOver certification.
Native Cocoa captures are under `build/rfc0036-run/native-cocoa`. No composed
application screenshot or attended physical-input decision is claimed here.

## md4qt comparison evidence

Canonical upstream revision: `34d059e066e0dac138f0bc8144fe1d3acbba3dc3`, version
5.1.4. A separate Release probe links that parser against the same Qt 6.10.3;
no md4qt code or package is linked into BetterPullReview. Probe source, CMake
harness, corpus and results remain under `build/rfc0036-run/comparison*` for
continuation. The task-owned downloaded source and comparison build were removed
after collecting results; the pinned canonical source can be fetched again.
The canonical revision and decision are durable here and in the reading contract.

The 777-byte component corpus is reused exactly. AST counts: two tasks, one
heading, one table, two code items (inline/fenced), one image, three links.
The ordinary exported-HTML/native-document bridge has zero task markers, whereas
native Qt Markdown has two. The AST's correct tasks distinguish parser fidelity
from the bridge's presentation loss. Exported HTML retains the synthetic raw
script and raw HTML image. A before/after existing-file probe demonstrates
filesystem-dependent relative-link rewriting from the stream entry.

| Probe (same local machine) | md4qt AST | md4qt HTML + native layout | Native Qt Markdown + layout |
| --- | --- | --- | --- |
| 777-byte corpus, warmed | 0.14 ms | 0.49 ms | 0.39 ms |
| 65,536-byte prose | 2.89 ms | 19.92 ms | 18.76 ms |
| 1,000 nested quote characters (outside product bound) | 4.52 ms | 9.31 ms | 0.06 ms |

These are individual observations, not a statistical benchmark or guaranteed
frame budget. Cold font discovery cost the first HTML layout about 157 ms, so
that number is not used to rank parsers. A real direct AST visitor could differ.
The source-aware native component adds styling and resource suppression beyond
this parser/layout probe. The product refuses the pathological nesting before
parsing. Neither timing alone establishes malicious-input safety.

Decision and integration/security/license tradeoffs:
[Native Markdown reading](../architecture/native-markdown.md#renderer-comparison-requested-by-the-owner).

## Milestone 2 composed acceptance

Run `rfc0036-20260920`, segment `m2-composed-r2`, mandate r2. The sole engineering
and bookkeeping owner is `/root/markdown_milestone_2`; the assignment adopts M1
and completes `m2-composed-integration` from `7509d70d`. The comparison above is
reused unchanged. Current source changes also correct imported cursor placement
and pixel-wheel/focus handling, so the shared reader corpus is rechecked.

The producing source candidate is `a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f`.
Later bookkeeping commits do not change that product candidate. This work uses synthetic public and private-host
metadata only; it performs no live provider mutation, publication or receipt edit.

### Public path and authority

The composed course types a PR address into the Home Open bar and presses Return.
The submitted destination reaches the real per-window read host with a manually
completed synthetic provider response. Its normal projection mounts the complete
PR description and six public body kinds: review summary, GraphQL root/reply,
REST root/reply and issue comment. Native document structure assertions check
headings, lists, tasks, fenced code and a real table. Explicit conversation
activation and Tab enter the visible full reader; summaries remain navigation.

Keyboard link activation traverses the real window dispatch, core admission and
read-host browser capability. Relative paths use the accepted full head OID;
provider-specific URL construction stays in the host. Local/executable/userinfo
and credential query destinations, absent bodies, missing capability, sibling
window and replaced window generation are refused. A fresh publication revision
invalidates earlier gestures even when the provider repeats its snapshot id.
Images use identical admission. A replaced PR clears both readers before the new
completion; a late prior completion cannot revive authority. Host shutdown drains.
The platform opener is separately checked with a recording launch seam for both
HTTP and HTTPS, while device authorization retains its own HTTPS-only policy.

The test compares raw clipboard MIME bytes for Unicode, CRLF and tabbed source,
and checks the host's accepted body remains exact across reading operations.
Unchanged refresh keeps the same native document, directed selection and scroll.
Malformed, empty and pathological bodies remain readable with explicit fallback.
Wide code and tables scroll locally and cannot force the workbench wider.

### Corrections and known baseline failures

Direct document import left Qt's attached cursor at the final character; new
bodies now start at the first character. Pixel-wheel input stays in the full-body
reader and is consumed at its boundary. The post-list focus route explicitly
enters the reader rather than returning to a scope control. Overview checks now
measure the content viewport independently of the new source/copy controls and
fallback explanation, retain source anchors, directed selection, Return/Escape,
file focus and the uncapped expanded allocation. These are exercised at multiple
font sizes, widths and display scales. Native capture inspection exposed a weak
end-of-document assertion: text presence alone did not prove the final content
was visible. The replacement uses Qt's platform shortcut, exact final cursor and
viewport containment. It caught absent source keyboard-selection flags and a
queued overview layout restoring the position from before a reading gesture;
both are corrected. The queued-layout failure is retained in
`m2-reading-timing.log` (cursor 1027 immediately after the gesture, then 0).
Wide-table capture inspection also found Qt squeezing cells into character-wide
columns. Cells now keep their text lines and use local horizontal overflow; the
final test requires the two-row table to remain shorter than the viewport and
its last cell to be reachable. No broad account/layout assertions were weakened.

Before M2 source changes, `qt_composed_github_scale_100` failed its existing
account-card assertion: “with GitHub CLI chosen the card speaks for it: the kind
line says so, Connect is its retry, the warning stays and Sign in is not filled”.
`m2-initial-composed.log` records that baseline. Diagnosis found the current
shared CLI setup flow hides the native Sign in and card warning. A temporary
assertion adjustment then reached a second unrelated setup-card relocation
assumption. That adjustment was discarded; account/setup code and assertions
remain unchanged. This failure is distinct from the new Markdown course and is
not represented as a pass.

An additional `qt_anonymous_public_preview_scale_100` run stopped before its
conversation test at “GitHub.com's Connect, while it is not chosen, activates
exactly its native HTTPS profile” (`m2-focused-eighth.log`). This wider family is
not claimed passing, and that failure was not independently reproduced on the
starting checkout. The added `qt_public_preview_conversation`
selector exercises the changed accepted-window conversation path independently;
profile-wide metadata alone cannot replace its content. The broader result is
recorded explicitly; no account-activation repair is part of this candidate.

The broader integration also failed `qt_workbench_window_scale_100`, `_125` and
`_200` at “the epoch row is the first row of the client area and as wide as it”.
A separate clean checkout of starting revision `7509d70d` was configured and
built, and `ctest --test-dir build/rfc0036-run/m2-baseline-src/build/dev -R
'^qt_workbench_window_scale_100$' --output-on-failure` reproduced the exact failure
in 2.81 seconds (`m2-baseline-workbench.log`). That disposable checkout/build is
removed. Both established baseline families are already tracked by [[BPR-0940]].
The initial missing `setup_destination_public_path` executable was a build
omission, not a baseline failure; building `test_setup_destination_public_path`
and running `application_setup_destination_public_path` corrected it.

### Final commands and observations

All paths below are relative to this worktree. Full command output, earlier
failures and native artifacts remain under `build/rfc0036-run/`.

| Check | Result and retained log |
| --- | --- |
| `cmake --build --preset dev --target app_better_pull_review` plus the focused test targets | Pass; `m2-candidate-build.log`, final reader/app relink in `m2-table-build.log` |
| `ctest --preset dev -R '^(core_markdown_navigation\|application_github_window_read_host\|qt_pull_request_overview_page\|qt_github_conversation_panel\|qt_conversation_cards_view\|qt_markdown_body_.*\|qt_composed_markdown_.*\|qt_public_preview_conversation\|application_device_authorization_browser_address\|application_setup_destination_public_path)$' --output-on-failure` | 14/14 pass in 48.65 s; `m2-final-checks.log`; complete setup/draft/source/zero-publication course 39.26 s |
| After the final table correction: `ctest --preset dev -R '^(qt_markdown_body_.*\|qt_composed_markdown_.*\|qt_pull_request_overview_page\|qt_github_conversation_panel)$' --output-on-failure` | 8/8 pass in 8.09 s; `m2-table-checks.log` |
| Final composed corpus/capture assertions: `ctest --preset dev -R '^qt_composed_markdown_' --output-on-failure` | 3/3 pass at 1.0, 1.25 and 2.0 scales in 3.53 s; `m2-composed-final.log` |
| Native Cocoa course with `BPR_AUTOMATED_TEST_BOUNDARY=1` | Pass; `m2-native-final.log`; 50,742-byte / 650-paragraph layout 32 ms, explained 1,027-byte pathological fallback 14 ms at native DPR 2 |
| `core_markdown_navigation`, `core_dependency_boundary`, `architecture_boundaries`, `architecture_boundary_contract` in core-only graph | 4/4 pass, including architecture negative controls; `m2-core-checks-final.log`; final navigation rebuild/rerun also passes in `m2-final-core-navigation.log` |
| LLVM 23.1.1 touched-file format and changed-line tidy | Pass; `m2-final-format-check.log`, `m2-table-format-check.log`, `m2-final-composed-format.log`, `m2-tidy-changed-lines.log`, `m2-final-tidy.log`, `m2-table-tidy.log`, `m2-composed-final-tidy.log` |
| Both documentation commands and scoped board validation | `DOCUMENTATION_OK`, 64/64 script tests, scoped board valid; final logs recorded with closeout below |

The unfiltered touched-translation-unit tidy run is **not** green: retained
pre-existing diagnostics are `home_view_state.hpp:35` (signed bitwise operands),
`dif_runtime.hpp:1` (pragma once), `github_application.cpp`'s unchanged
`exact_current` variable and widening casts, and unchanged parameter names in
`github_window_read_host_tests.cpp`. `m2-tidy.log` retains them along with the
new null-capability analyzer warning, single-character append and static fixture
initialization that were corrected. Changed-line filtering includes every new
file and changed source/header range; no global suppression was added.

The native command was run from this checkout:

```sh
QT_QPA_PLATFORM=cocoa BPR_AUTOMATED_TEST_BOUNDARY=1 BPR_MARKDOWN_RENDER_DIR=/Users/jaeger/.codex/worktrees/14a8/BetterPullReview/build/rfc0036-run/m2-native-cocoa /Users/jaeger/.codex/worktrees/14a8/BetterPullReview/build/dev/tests/adapters_qt/better_pull_review_composed_markdown_tests
```

`m2-candidate-manifest.json` binds the source candidate, app and test binary
SHA-256 hashes, and ten native captures. The app hash is
`2a0024477d20ad99801f1c6b0a455103c2a49aa2fb77a7cb9c6238e52a58aaec`;
the composed-test hash is
`ee34f149a5660a659e917537e9b187083f956530b1f97f7fd97cc2ded3a22e08`.
The native run uses the actual Qt workbench/read-host composition with fixtures;
it does not assert a notarized installed-release run.

Inspected captures in `build/rfc0036-run/m2-native-cocoa/`: `overview-native.png`,
`overview-long-final.png`, `overview-long-final-wide.png`, `conversation-1.png`,
`conversation-2.png`, `conversation-4.png`, `conversation-5.png`, `wide-code.png`,
`wide-table.png`, and `source-fallback.png`. The four conversation palettes show
readable headings, tasks, code, links and table borders. The long captures show
the exact final paragraph with different reader widths. Wide captures expose
`END OF CODE` and the selected `END OF TABLE` cell through local scrolling;
fallback shows its explanation, complete final line and keyboard selection.
Accessibility is checked through the native Qt text interface and named controls.
This is deterministic Cocoa input and inspected raster evidence, not an attended
VoiceOver or physical-trackpad claim.

| Criterion | Direct accepted evidence at `a3ab46ee` |
| --- | --- |
| AC-01 | Actual Open gesture, real accepted read host and six complete public post/reply types; native structural corpus in all three scales |
| AC-02 | Original CRLF/tab/Unicode clipboard bytes and accepted host body unchanged; source, malformed, empty and explicit complete fallback; unchanged draft/publication public course reused after integration |
| AC-03 | M1 resource negative control rechecked; core/host refusal matrix, exact accepted revision links, direct/image parity, no effects without explicit admission |
| AC-04 | Native Cocoa keyboard selection/document movement, post-to-reader Tab, source controls and link Enter; accessible text; four appearance modes and three scales; readable local wide overflow |
| AC-05 | Measured long/pathological bodies; unchanged document/selection/scroll; stale body/window/publication rejection, sibling preview isolation and shutdown |
| AC-06 | Final 650-paragraph content visibly reached with uncapped expansion; source-anchor, Return/Escape and file-focus checks; delivered policy/bounds/image documentation |

Counter reconciliation preserved the existing reservation authority: concurrent
commit `d3d707ac0785b7f0c55187b2d58f91caa4511d16` reserved BPR-0951–0965
and advanced the historical floor to BPR-0966. The local untouched counter was
synchronized from BPR-0950 to that exact committed value under the board lock,
using its atomic `apply_changes` and fresh `inspect_counters` validation. No ID
was allocated and no external card/source was imported. The record is
`m2-counter-reconciliation.json`. The scoped final diff against the immutable RFC
base is retained in `m2-rfc-base-scoped-diff.log`; inherited RFC0037 behavior is
preserved and exercised rather than attributed to this change.

### Formal RFC status

Technical implementation and acceptance are distinct from `Auto-Verified` RFC
status. RFC-BPR-0005 and RFC-BPR-0006 are Auto-Verified; direct dependency
RFC-BPR-0008 remains Proposed, and canonical predecessors remain open. The RFC
and index retain Proposed. No required human criterion exists in this RFC; native
Cocoa rendered/input/accessibility evidence satisfies the available-platform
criteria without claiming VoiceOver certification or physical-input attendance.
Windows/Linux and live-provider behavior remain unclaimed.

Technical card acceptance is revision-bound to source `a3ab46ee`; BPR-0916's
finish commit is `efa43883`. Guarded BPR-0917 readiness and claim follow only
after that implementation dependency is Done. Final gate artifacts are
`m2-docs-final.log`, `m2-docs-tests-final.log` (64 tests), and
`m2-board-final.json` (valid). The completion observation/check result under
`build/rfc0036-run/` records the final documentation HEAD and quiescent ownership.
The unrelated `.mask.lock.json` change is excluded from every producer commit.

## Status-only close under BPR-1299: stopped at AC-01 (2026-10-06)

Mandate of 2026-10-06: promote and claim [[BPR-1299]] and check RFC-BPR-0036's
acceptance against [[BPR-0917]]'s evidence and its dependencies. If they hold,
close the RFC; otherwise record exactly what is missing and stop. The
dependencies RFC-BPR-0005, 0006 and 0008 are Auto-Verified, and BPR-0917 is
Done. BPR-1299 was promoted at `5cbac5a6b` and claimed at `c012afeaa`, over
`origin/main` `34c4b9d49`.

Since `a3ab46ee`, the shared reader has changed: BPR-1083's label marks,
BPR-0970's fuzz fixes, review-tour reconstruction and Qt 6.11.2. The overview
and conversation composition have changed much more (about 2,700 lines across
`conversation_cards_view.cpp`, `github_conversation_panel.cpp` and
`pull_request_overview_page.cpp`). So BPR-0917's focused course was run again
rather than reused.

Fresh at `c012afeaa` (product tree `34c4b9d49`), macOS 26.6.2 arm64,
`build/dev` (Debug):

- `ctest --test-dir build/dev -j 6 --output-on-failure -R
  '^(core_markdown_navigation|application_github_window_read_host|qt_pull_request_overview_page|qt_github_conversation_panel|qt_conversation_cards_view|qt_markdown_body_.*|qt_composed_markdown_.*|qt_public_preview_conversation|application_device_authorization_browser_address|application_setup_destination_public_path|core_dependency_boundary|architecture_boundaries|architecture_boundary_contract)$'`
  passes 17 of 17.
- The native Cocoa course (`QT_QPA_PLATFORM=cocoa BPR_AUTOMATED_TEST_BOUNDARY=1`,
  `better_pull_review_composed_markdown_tests`) passes. A 50,742-byte body lays
  out in 31 ms, and the 1,027-byte pathological fallback takes 10 ms.

Inspecting the ten captures found a failure that the passing tests do not
report. The overview still renders the declared subset (`overview-native.png`
shows headings, emphasis, quote, nested lists, read-only tasks, code, a
bordered table and links), and the long-description and wide-block captures
still reach their final content. The four conversation captures
(`conversation-1`, `-2`, `-4`, `-5`) show every public post as raw Markdown:
`- [x] Complete`, a backtick fence, `| --- | --- |`, `**Readable**` and
`[Web link](https://…)` as typed.

The cause is [[BPR-1290]] at `201c96f21` (2026-10-06), on the owner's verdict
on the inline drawer prototype ("I like it"). That card records the removal of
"the post list and the Markdown body pane (each post is read in place, as
written)". Posts are now a `threadPostBody` `QTextEdit` holding the body as
written, with links admitted through the host. The same commit narrowed
`qt_composed_markdown`'s conversation half. It no longer calls `structures()`,
compares `original_body()`, Tabs into a reader or switches source for
conversations. It checks only that each post reaches its final content and
that a link gesture carries the post's identity. The rows therefore pass while
the behaviour they used to check is gone.

| Criterion | At `34c4b9d49` | Result |
|---|---|---|
| AC-01 | Overview renders the subset through the shipped composition. Public conversations show Markdown as written, not rendered | **Fails** for public conversations |
| AC-02 | The overview's source, copy and fallback hold. Conversation posts show their bytes as written; no rendered preview remains in them to compare | Overview pass; conversation half moot while AC-01 fails |
| AC-03 | Link admission and refusals through the host, for the overview and for conversation post links (the course's link gesture per post). No resource effects | Pass |
| AC-04 | Overview reader keyboard, source switch, appearance and scales hold. The conversation reader that the criterion's keyboard, source-switching and accessibility half was proven on has been removed | Overview pass; conversation half not demonstrated |
| AC-05 | Bounds, measured long and pathological bodies, unchanged refresh and stale-completion rejection | Pass |
| AC-06 | Uncapped long description reaches its final paragraph; `docs/architecture/native-markdown.md` still describes subset, fallback, bounds and image policy | Pass |

The RFC stays Proposed. BPR-1299 is parked on `human-review` with one owner
question. The first choice is to restore rendering of the declared subset in
each open thread's posts, keeping BPR-1083's marks and the link host, and to
restore the course's conversation structure assertions, in BPR-1290's next
candidate (BPR-1290 is itself parked for the owner). The second is to amend
AC-01 and AC-04 to the overview and close. Nothing in the product changed
here.

Published after a rebase over `06a3c530b`: promotion `ceb7c9e6c` (recorded
`5cbac5a6b`), claim `d60d83399` (recorded `c012afeaa`) and this record
`03d814ec8`.
