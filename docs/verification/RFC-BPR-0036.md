# RFC-BPR-0036 verification

RFC base: `7ad0e43eb92c217ac6cd01fc478bde2968d217a0`.
Mandate r1: implement RFC-BPR-0036 in a worktree. Mandate r2 adds an explicit
md4qt/native-Qt GFM comparison before renderer selection. This ledger records
component evidence; composed RFC acceptance remains pending milestone 2.

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
its files. Only `/root/markdown_milestone_1` owns repository engineering.

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

## Remaining RFC acceptance

All AC-01 through AC-06 remain open for composed evidence. Milestone 2 must adopt
both initial surfaces, preserve publication bodies and RFC0037 geometry, apply
current-context link admission and host browser dispatch, test identity replacement
through the real composition, and supply native input/accessibility/system
appearance evidence. No required human/native decision is self-certified here.
