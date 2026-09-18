# Conversation surface layout study

## Current frontier

Investigation complete against baseline `4384e20982f6ed736a3845f1fd8e6f346b8a9ae3`.
Owned scope: BPR-0920, BPR-0923 and BPR-0926 design evidence only. Source is
unchanged. The milestone integrator selects dedicated composition and PR-wide
Discuss, with BPR-0929 owning their coupled runtime implementation. Five focused Qt checks pass; the broader composed
test fails at an earlier CLI-card assertion, recorded below. A synthetic composed
probe reproduces empty-page space and compressed inline editors. The original
owner's build and two clipped-box identities remain unobserved. Runtime AC-03,
AC-06 and AC-09 remain implementation follow-ups, not passing claims here.

## Evidence and control identities

Line references below refer to the baseline. All data were synthetic; no live
GitHub/GHES requests, credentials or private PR content were used.

| Surface | Actual identity and behavior | Source evidence |
|---|---|---|
| Thread header | Availability label followed by `showReviewThreadsButton` | `src/adapters/qt/src/github_conversation_panel.cpp:169` |
| Space below header | `githubConversationState`, a stretched stack; page zero is an empty QWidget | same file, lines 206 and 385 |
| Whole-review composition | `wholeReviewEditor`: title, identity, type, summary, Submit/Discard, Check status/New review, status | `src/adapters/qt/src/review_submission_editor.cpp:22` |
| Identity field | `wholeReviewTarget`, read-only frameless QTextEdit; full host/repository/PR/head/actor text, not a PR-link input; wraps anywhere and follows document height | same file, lines 27–41; `src/app/src/review_submission_workflow.cpp:156` |
| Review type | `wholeReviewDisposition`, Comment/Approve/Request changes | editor lines 42–48 |
| Summary | `wholeReviewSummary`, editable plain text, 80–160 px vertical bounds | editor lines 49–56 |
| Quoted instruction | `wholeReviewStatus` is the LAST whole-review control; “Choose a review type…” comes from workflow `new_draft()` | editor lines 75–83; workflow line 130 |
| Next editor | `workbenchReviewDraftEditor`, separate inline/reply drafting widget, maximum height 220 | `src/adapters/qt/src/workbench_window.cpp:1289` and 1305 |
| Inline fields | `reviewPublishTarget`, `reviewReplyTarget`, `reviewMutationStatus`, then conflict actions, `reviewDraftBody`, workshop original/proposal and actions | `src/adapters/qt/src/review_draft_editor.cpp:66` and 120–151 |

The drawer stacks the conversation panel with stretch 1, whole-review editor
with stretch 0 and `AlignLeft`, inline editor, and additional AI/private workflow
tools (`workbench_window.cpp:1330`). Its scroll area allows horizontal scrolling
below the content minimum (`:1433`). This is several separate jobs competing for
one narrow column. Increasing the whole window width alone need not widen the
drawer or the left-aligned editor.

### Synthetic geometry observations

Probe date: 2026-09-18, macOS arm64, repository Qt 6.10.3, offscreen platform,
logical pixels, application font set to 13 and 20 points. These are deterministic
layout facts, not native appearance, assistive-technology or trackpad acceptance.
The probe reused `composed_github_tests.cpp`'s `profiles()`,
`publish_session_ready()` and `publish_pull_request()` in an actual
`workbench_window`. It published expanded drawers, a synthetic whole-review
target/status, resized to 900×900 and 1400×900, and processed eight event passes.

| Measurement | 13 pt, 1400×900 | 20 pt, 1400×900 |
|---|---:|---:|
| Drawer viewport | 339×693 | 339×645 |
| Conversation stack | 357×601 | 502×669 |
| Whole-review editor | 256×486 | 319×674 |
| Identity document / viewport height | 148 / 148 | 264 / 264 |
| Whole-review summary | 256×140 | 319×96 |
| Inline editor actual / minimum-hint height | 220 / 429 | 220 / 453 |
| Inline body, workshop original, workshop proposal heights | 23 each | 23 each |
| Each inline text box minimum-hint height | 90 | 90 |

At 900×900 the drawer viewport narrowed to 261 px (13 pt) and 258 px (20 pt),
while those editor dimensions remained the same. The content therefore needed
horizontal scrolling. `isVisible()` was true for all named controls, but that
does not mean every control was inside the scroll viewport at once.

With the card scope explicitly set to Whole PR and the conversation anchor set
to `absent.cpp`, Discuss had **zero model roots, one retained card, and stack
page zero**. That page had no widget children and measured 502×669 at 20 pt.
Clearing both file and line anchors produced three model roots, one card and
Discuss page two. This reproduces a large blank region despite known conversation
content. Its mechanism is the empty stack page retaining the loaded pages' size
requirements and stretched allocation; the exact original gap is unobserved.

Confirmed compression affects `reviewDraftBody`, `reviewWorkshopOriginal` and
`reviewWorkshopProposal`. The whole-review target wrapped but was not vertically
clipped in this probe. Neither the owner's two boxes nor their ordering can be
bound to a pair from the prose alone: the quoted instruction precedes the inline
editor, whereas `wholeReviewTarget` precedes the instruction. Do not label the
two boxes as PR links or claim the source report reproduced exactly.

## Reveal, loading and Discuss diagnosis

`ShowReviewThreadsAction` connects only to `focus_review_threads()`
(`workbench_window.cpp:2118`, `:6402`). It walks the current model's roots,
selects/expands/centers the first review-thread root, then focuses the tree.
It does not fetch, clear filters, change scope or open a collapsed drawer. Its
selection can trigger the existing conversation-to-code synchronization; it is
therefore more than a pure visibility toggle. With no matching root it only
attempts focus. The core enables it from accepted snapshot and capability, not
from the filtered result count (`src/core/src/github_transition.cpp:539`).
`composed_github_tests.cpp:1805` explicitly asserts focus/selection/expansion.

Conversation data arrive with the accepted PR projection, independently of this
action (`src/application/src/github_projection_mapping.cpp:337` and
`workbench_window.cpp:6003`). Availability text distinguishes supported-empty,
supported-count, unsupported and unknown capability, but does not describe the
active local filters (`github_presentation_text.hpp:357`). PR loading/failure
comes through the read host's status; a filtered-empty model is not that failure.

`apply_modality_surface()` changes Discuss arrangement and drawer width but keeps
the comparison as the center (`workbench_window.cpp:4776`). It does not clear the
file/line anchor. `conversation_anchor_file()` prefers retained refresh anchor,
then cursor, then comparison context (`:3955`). Thus an unavailable comparison
can still leave a selected-file filter. `anchored_to()` excludes pathless
top-level content under a file filter and excludes unavailable/outdated anchors
under an exact line filter (`github_conversation_panel.cpp:89`).

The tree/map use the filtered `Shown` vector, whereas cards adopt `Retained` and
have their own scope/filter choices (`:440`). `HasThreads = !Shown.empty()` gates
the entire loaded page (`:473`, `:506`). Changing cards to Whole PR cannot recover
a page hidden by the tree. Selecting an off-file card also resolves its identity
through that filtered model (`:311`), so it can fail to select a detail row.
With no file AND no line anchor the tree already accepts the complete PR
conversation; a selected comparison is not a data requirement. The existing
cards feed, however, admits only root `review_thread` records
(`conversation_cards_view.cpp:645`), omitting standalone review summaries and
PR comments from that feed. The tree retains those records when unfiltered.

The synthetic result proves these failure paths; it does not establish which
one occurred in the original session or whether provider data were available.

## Selected integrated design

Select a dedicated **Compose review** page within the existing workbench window,
opened explicitly from Review, Read or Discuss. It temporarily uses the main
content area, records the invoking place and restores it on Back. Review remains
the code-review surface. Keep the same editor/workflow instance alive across
placement changes. Opening, closing and switching pages publish nothing.

| Mode | Wide window | Narrow window / larger font |
|---|---|---|
| Read | Files / code / contextual conversation drawer; Compose review entry in toolbar | Code remains primary; drawer can be toggled; conversation detail uses available width; no always-present summary form |
| Review | Files / code and review controls / contextual conversation drawer; Compose review entry shows draft state | Same jobs, collapsible drawers; inline composer opens explicitly with usable height rather than all workshop boxes forced into 220 px |
| Discuss | Main area: PR-wide conversation index beside one selected-thread detail; optional compact graph | Index then detail as switchable pages with Back to conversations; filters wrap/stack; no forced 640 px drawer alongside an empty comparison |
| Compose review | Back; target and actor; review type; expanding summary; submission actions and status | Full available width; target uses complete selectable wrapping rows; action rows stack; one outer scroll fallback at short heights; summary retains an independently usable editing viewport |

This is a layout specification, not a rendered prototype of the proposed design.
At representative 900×900 and 1400×900 windows with 13/20 pt fonts, the follow-up
must demonstrate the actual post-change geometry. Use font metrics and available
space, not another fixed maximum height for an entire multi-control editor.
Keep identity fully inspectable/copyable; a compact target headline may disclose
the full head and actor details, and confirmation must still name the exact target.
Tab follows reading order, Back returns focus to the invoking control, and hidden
pages do not take keyboard focus. Do not add Markdown policy in this slice.

Discuss defaults to **All PR conversations**, regardless of the code cursor or
comparison availability. Keep independent, visibly named optional file scope;
selecting a file there filters discussion without requiring Git or selecting code.
Clear filters restores the PR-wide list. A file filter explicitly excludes
pathless PR comments/review summaries and says so. Unread and Unresolved are
additional explicit filters, off by default; resolved and outdated threads remain
readable by default. Unknown resolution in public preview stays unknown.

The index includes top-level PR comments, review summaries and inline threads
with replies, labels each kind and preserves stable identity. Shared provider
content and private drafts/notes retain separate provenance. The selected detail
owns full bodies; the graph summarizes/navigates structure and does not repeat
full text. BPR-0924/BPR-0925 own detailed body sizing, scroll routing and graph
integration; this study does not declare those defects solved.

Selecting a conversation reads it. **Open in code** is a separate action for
anchored content; it uses the existing semantic route and exact endpoint checks.
Current resolvable anchors return to the relevant file/line; outdated, removed or
unavailable comparisons give an explicit reason and leave discussion readable.
No guessing a nearby line or accepting stale source. Returning from code restores
the Discuss scope and selected conversation.

Remove Show review threads from the drawer. Initial load/refresh already obtains
the data; automatically show accepted results. The existing Discuss entry becomes
the clear PR-wide navigation route. If a separate shortcut is retained, label it
**Open discussions**, route to that same page and preserve an existing selection.
Do not repurpose a focus-only action as an implicit provider fetch.

| State | Visible result and recovery |
|---|---|
| Initial loading | “Loading conversations…”; no zero count before acceptance |
| Accepted empty | “No conversations on this pull request” only when all relevant admitted content kinds are empty |
| Filtered empty | “No conversations match these filters” plus active scope and Clear filters |
| Refreshing | Retain same-authority accepted content/selection and label it refreshing |
| Failed refresh | Retain allowed last accepted content with explicit failure and Retry/Refresh; do not relabel as empty |
| Unavailable capability | State exactly what cannot be observed; continue showing other available content kinds |
| PR/account replacement | Clear prior-authority content/selection and filters before replacement; reject stale results |

Before: one thread exists on another file, Discuss inherits a file/line filter,
blank stack appears, Show review threads merely focuses the empty tree.
After: entering Discuss shows All PR conversations and that thread; an explicit
file filter can produce a named empty result with Clear filters. Selecting the
thread reads it even when Open in code is unavailable.

### Alternatives rejected

Embedding the whole-review form permanently in central Review improves width
but competes with the code the reviewer needs to inspect, and forces a mode
change to compose from Discuss. Keeping the form in a wider drawer preserves
the multi-editor vertical squeeze and consumes code width. A modal submission
dialog interrupts reference reading and invites conflating opening with submitting.
A separate top-level window introduces unnecessary focus/lifecycle complexity.
The selected in-window dedicated page supplies space with a clear return path.

### Preserved publication and draft contracts

Relocation must not recreate or reset drafts, change review type/body semantics,
or trigger Submit. Preserve the workflow's same-authority refresh retention,
frozen-head refusal, original-scope recovery for admitted attempts, and new empty
draft for another PR/account (`review_submission_workflow.cpp:22–174`). This is
not a promise to add cross-PR persistence for unsent whole-review drafts.
Inline drafts retain their existing accepted/candidate/conflict and local/private
provenance. Discard remains explicit.

Submit still enters core policy and host admission. Preserve exact-draft identity
checks, configured confirmation and post-confirmation revalidation, one-use grants,
idempotency and uncertain-result reconciliation (`review_submission_workflow.cpp:198`).
Approve may have an empty summary; Comment/Request changes keep current nonblank
requirements and the 64 KiB bound. Check status remains distinct from retrying a
write. Authentication, CLI confirmation and public-preview refusal are unchanged.
The core decides; the host performs; Qt presents.

## Bounded canonical implementation follow-up

[[BPR-0929]], **Implement dedicated review composition and PR-wide Discuss**,
owns the coupled runtime result for BPR-0920/AC-03, BPR-0923/AC-06 and
BPR-0926/AC-09. It relocates the existing whole-review editor to the dedicated
in-window page, wires reversible entry/Back/focus, removes permanent drawer
placement, and replaces the inline editor's blanket 220-pixel squeeze with an
explicitly opened drafting surface. It also makes Discuss a PR-wide main-content
page, separates its scope from code anchors, includes all admitted conversation
kinds, replaces the reveal gesture with reachable navigation and presents the
distinct loading/filter/empty/failure states specified above.

One follow-up is intentional: composition placement, the replacement navigation
gesture and the PR-wide Discuss default all change the same workbench content
surface and must preserve one draft/selection/focus lifecycle. A rename-only or
drawer-only slice cannot meet the three criteria. BPR-0924/BPR-0925 remain
separate owners for complete thread-body sizing, input routing and graph/detail
integration. No independent human approval gate is introduced, and no runtime
criterion is closed here.

## Verification and reproducibility

Executed from `/Users/jaeger/Documents/GitHub/BetterPullReview`:

```sh
git rev-parse HEAD
ctest --preset dev --output-on-failure -j 1 -R '^(qt_github_conversation_panel|qt_conversation_cards_view|qt_review_submission_editor_scale_(100|125|200))$'
ctest --preset dev --output-on-failure -j 1 -R '^qt_composed_github_scale_100$'
python3 scripts/verify_documentation.py --root .
```

The five tests passed (1.81 s total). Editor tests verify semantic dispatch,
focus/cursor retention, identity text/wrapping, keyboard controls and pending
recovery state at 1×/1.25×/2×. They do not verify every composed text viewport.
Panel tests cover Discuss rearrangement, filters and stable selection. The
documentation gate returned `DOCUMENTATION_OK`.

The additional `qt_composed_github_scale_100` run failed in 0.23 s, before its
conversation acceptance checks, with: “choosing the native host leaves the
warning on the CLI card, where the opt-in is made, still without the host”.
No repository source/test was edited in this investigation. This is an observed
failure of the existing executable, not a newly diagnosed cause or a passing
composed regression result. The integrator must disposition it separately.

The integrator authorized a disposable probe directory created by
`mktemp -d /tmp/bpr-0920-conversation.XXXXXX`; actual path was
`/tmp/bpr-0920-conversation.jQqcIH`. The probe included the existing
`tests/adapters_qt/composed_github_tests.cpp` with `main` renamed, then supplied
its own main using the fixture helpers described above. Compilation reused
`DEFINES`/`INCLUDES` and `LINK_PATH`/`LINK_LIBRARIES` from the corresponding
`test_better_pull_review_qt_github_tests` blocks in `build/dev/build.ninja`, via
Ruby `Shellwords.split` and argument-array `system`, with `clang++ -std=c++23
-arch arm64`. Thus it linked the actual existing Qt/application/core libraries
and resource object, not a recreation of their layout.

Exact build/run commands were:

```sh
ruby /tmp/bpr-0920-conversation.jQqcIH/build.rb
QT_QPA_PLATFORM=offscreen BPR_AUTOMATED_TEST_BOUNDARY=1 /tmp/bpr-0920-conversation.jQqcIH/probe
```

To reproduce the decisive empty-page case in that fixture, perform these calls
after publishing its known PR, then process Qt events and inspect the named stack:

```cpp
auto* panel = window.conversation_panel();
panel->cards()->set_scope(qt::cards_scope::whole_pull_request);
panel->set_anchor_file(core::file_identity_of("absent.cpp"));
panel->set_discuss(true);
// model()->rowCount() == 0; cards()->cards().size() == 1;
// githubConversationState currentIndex() == 0, empty page has no children.
panel->set_anchor_file(std::nullopt);
panel->set_anchor_line(std::nullopt);
// model roots == 3; cards == 1; currentIndex() == 2 after events.
```

For geometry, measure named widgets' `size()`, `minimumSizeHint()` and
`mapTo(column->widget(), QPoint{})`; compare target document height with its
viewport height. Use the table's window/font matrix. The target text was
`github.example.test · synthetic-owner/synthetic-repository#17`, head
`abcdef0123456789abcdef0123456789abcdef01`, actor `synthetic-reviewer (account 4)`.
No native run, live service, wheel/trackpad timing or proposed-layout usability
acceptance is inferred from the probe. Its temporary sources/executable are
removed after recording these facts; no background process is retained.
