---
id: "RFC-BPR-0036"
title: "Native Markdown Rendering for Review Content"
status: "Auto-Verified"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0005", "RFC-BPR-0006", "RFC-BPR-0008"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"rfc0036-native-markdown","path":"docs/rfcs/plans/rfc0036-native-markdown/milestone-plan.json"}}
---

# RFC-BPR-0036: Native Markdown Rendering for Review Content

## Request and outcome

After opening their first PR in the 0.1.3 patch build, the owner requested
Markdown rendering on 2026-09-18. That registration created a proposal and backlog.
Implementation is now authorized in a worktree, including an explicit comparison
of md4qt with native Qt for GFM (mandate r2). The
[shared reader design](../architecture/native-markdown.md) records the comparison
and the delivered component/composition boundary. Both milestones have technical
acceptance at `a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f`;
[the ledger](../verification/RFC-BPR-0036.md) records native macOS evidence and
limitations. RFC status was Proposed until RFC-BPR-0008 and canonical
predecessors were Auto-Verified; [[BPR-1299]] closed it on 2026-10-06 once
[[BPR-1290]] v005 restored rendering in open-thread posts.
Implementation is [[BPR-0916]]; verification and
closeout are [[BPR-0917]]. Registration baseline: `7ad0e43eb92c217ac6cd01fc478bde2968d217a0`.

PR descriptions and review conversations should display readable formatted
content instead of exposing Markdown punctuation as plain prose. Reviewers must
be able to read headings, lists, code, links and tables without leaving the app,
select and copy text, and recover the original source without loss.

At registration, the overview mounted `overviewBody` as a read-only
`QPlainTextEdit`, capped it at 110 pixels and applied `setPlainText` in
`src/adapters/qt/src/pull_request_overview_page.cpp`. Markdown alone does not
solve that small viewport. [[BPR-0918]] separately investigates long-description
layout under [RFC-BPR-0037](RFC-BPR-0037-first-pr-review-workflow.md); neither task must wait for the other to explore or
improve plain-text reading. Their final composed behavior must work together.

## Scope and reading contract

The first delivery covers PR descriptions and public review/comment bodies in
the existing overview and conversation surfaces. Reuse one rendering policy and
native presentation component. Inventory other Markdown-bearing surfaces,
including AI chat and local drafts, but do not silently expand the first slice.
Editors retain their original Markdown text; preview is read-only. Rendering
must not modify publication payloads, stored bodies, receipts or comment identity.

Support paragraphs, headings, emphasis, strong emphasis, block quotes, ordered
and unordered lists, links, inline code, fenced code blocks, thematic breaks,
strikethrough, task-list display and tables. Treat task checkboxes as read-only
content, not controls that mutate GitHub. Specify the supported dialect and test
corpus before selecting a renderer; do not claim complete GitHub visual parity.
Unsupported constructs remain readable through a source view. Math, Mermaid,
embedded video, interactive HTML and Markdown editing tools are outside this
initial delivery. Syntax highlighting may reuse an existing capability but is
not a prerequisite for displaying a code block correctly.

Provide a discoverable **View source**/**Rendered** switch and a way to copy the
original body. Switching views retains the body and reasonable reading position;
source is never reconstructed from rendered HTML. Empty descriptions have a
clear empty state. Malformed or unsupported input must not blank the whole body.
A processing limit produces an explicit explanation and access to source,
without silently truncating content or claiming missing provider text exists.

## Links, images and remote content

Treat provider-authored Markdown as untrusted display data. No script execution,
embedded browser, active HTML forms, local-file reads or automatic remote image
fetching. Raw HTML is escaped or omitted according to a documented policy with
source available; it must not activate a resource-loading path.

Links are keyboard reachable and show their destination. Activation is an
explicit gesture routed through the application-owned browser/navigation port.
Admit supported HTTP(S) destinations through the existing URL policy; refuse
executable and local-file schemes. Resolve repository-relative links only from
an explicit host/repository/revision context; ambiguous references remain
readable rather than guessed. No credential-bearing URL is manufactured by the
renderer and credentials never cross into Qt or an arbitrary link destination.

Initially render images as accessible placeholders with alt text and an
explicit open-link action when their destination is admissible. Inline remote
image retrieval, private attachment authentication and image caching require a
later scoped extension with host-owned effects and size limits. Rendering or
resizing a body must make no network request.

## Architecture and ownership

Preserve “the core decides, the host performs, Qt presents.”

- Core owns any new semantic navigation decisions and deterministic policy;
  it receives no Qt document, provider object, filesystem or network capability.
- Application owns body/revision context, admitted navigation effects and any
  asynchronous preparation, cancellation and stale-completion rejection.
- Qt owns native text layout, selection, focus, accessibility, painting and
  incremental projection. It performs no provider or resource-fetching effects.
- Composition binds the existing public read paths to the shared presentation.
  Adapters continue supplying the original body through existing contracts.

Choose the parser/document strategy in the implementation design slice after
checking dialect support, dependency policy, resource-loading behavior and
measured layout cost. Qt types may remain in the presentation adapter; do not
move visual document layout into core. No WebEngine dependency is introduced.
Avoid duplicating parsing and style rules per widget.

Use existing appearance tokens and font settings for prose, links, quotes, code
and table borders. Keep code whitespace intact. Wide code and tables need an
explicit readable overflow treatment without forcing the whole workbench wider.
Support light, dark and high-contrast themes, keyboard selection and traversal,
and meaningful accessible names and reading order at supported display scales.

## Long content and update behavior

Set and justify finite input, nesting, table and layout-work bounds during the
design slice, coordinated with existing provider body limits. Measure large
synthetic documents and pathological nesting before choosing eager versus
incremental preparation. Keep the interaction lane responsive and retain source
for admitted bodies that cannot be rendered within the chosen bound.

A refresh for an unchanged body must preserve selection and scroll position.
Changed or replaced PRs must not display a late result belonging to another
body, revision or window. No Markdown operation triggers a publication or
receipt change. Overview geometry is owned by [[BPR-0918]]; integration must
expose the full admitted description without reintroducing the fixed small box.

## Delivery

1. Inventory the two initial surfaces, select the dialect/renderer and bounds,
   and implement a shared native component against synthetic fixture bodies.
2. Integrate overview and public conversations, exercise their composed paths,
   reconcile long-description layout and record evidence on [[BPR-0917]].

## Acceptance criteria

- AC-01: Overview and public conversations render the declared Markdown subset through the shipped composition; fixtures cover headings, nested lists, tasks, quotes, code, tables and links.
- AC-02: Source, copy and preview preserve original body bytes and publication payloads; empty, malformed and unsupported input stays readable with explicit fallback.
- AC-03: Hostile HTML, local-file URLs, executable schemes and image/resource references execute nothing and cause no automatic filesystem or network access; admitted link gestures traverse the host-owned path.
- AC-04: Keyboard reading, selection, links and source switching work with appropriate accessibility, light/dark/high-contrast appearance and representative display scales; wide blocks remain usable.
- AC-05: Declared bounds and measured long/pathological-body behavior keep the UI responsive; unchanged refresh preserves position and stale completions cannot replace another body's content.
- AC-06: Composed long-description reading reaches the final content without a fixed six-line viewport constraint; documentation describes the supported subset, fallback, limits and image policy.

Tests use synthetic public/private-host fixtures, never copied private PR bodies.
Capture and inspect representative native layouts; record actual target/platform
results rather than treating a helper test as proof of the full reader. Closeout
requires criterion evidence and eligible dependencies in canonical RFC order.

## Owner amendment 1 — 2026-10-16: GitHub parity

The owner decided on 2026-10-16 to close the gaps between this reader and
GitHub's rendering of pull request descriptions and comments, including a
sanitised subset of raw HTML. This changes the requirements above where they
conflict; it does not reopen the original acceptance, which stays
Auto-Verified for its scope.

**What changes.** "Do not claim complete GitHub visual parity" and "raw HTML is
escaped or omitted" give way, for the constructs below, to GitHub's behaviour
as observed through its Markdown API in gfm mode. Footnote presentation, which
the [reader design](../architecture/native-markdown.md) excluded, is now in
scope. Images stay with [RFC-BPR-0084](RFC-BPR-0084-inline-markdown-images.md),
which supersedes this RFC's image rules for image references only; this
amendment leaves `<img>` and `<picture>` to it.

| Card | Constructs |
|---|---|
| [[BPR-1543]] | HTML comments hidden; each single newline of a paragraph a line break; `> [!NOTE]`, `[!TIP]`, `[!IMPORTANT]`, `[!WARNING]` and `[!CAUTION]` alerts; footnotes with in-body references and back references. Discovered while comparing: Qt's dialect draws `_text_` as underline where GitHub draws emphasis. |
| [[BPR-1544]] | `<details>`/`<summary>` as a collapsible section, closed by default and keyboard operable; GitHub's sanitised HTML subset (`br`, `sub`, `sup`, `kbd`, `b`, `strong`, `i`, `em`, `u`, `s`, `del`, `code`, `pre`, `p`/`div` with `align`, `a href`, `hr`, `blockquote`, lists, headings, tables); emoji shortcodes from a generated static table. |

**What is kept.** Every original criterion holds. In particular AC-03: no
script, style, event attribute, form, frame or embedded browser; no resource
load from HTML; an HTML link is an ordinary untrusted destination admitted only
through the host path. Source, copy and publication keep the original bytes
(AC-02). The preview budgets in the reader design are measured on the original
body and still bound the work (AC-05). Code, fenced or indented, and inline
code spans are never rewritten. HTML outside the subset stays literal text or
is dropped as the reader design documents.

### Amendment acceptance

These criteria belong to the amendment's cards, not to the original closeout.

- AM1-01: A body's HTML comments, single or multi-line, are hidden in the rendered view and kept in code and in source; an unclosed comment block hides what follows it, as on GitHub.
- AM1-02: A single newline inside a paragraph, list item, quote or footnote renders as a line break in the same block; code, tables, headings and the code-tab repair are unchanged.
- AM1-03: The five alert kinds render as a block with a coloured left bar and an icon-less coloured title, readable at 4.5:1 (7:1 in high contrast) in all four themes; a marker with text on its line, or inside a list, stays a plain quote.
- AM1-04: Footnote references render as raised numbers in GitHub's order; a footnotes section closes the body with back references; reference and back reference move to each other inside the body by Enter or click without an application link gesture.
- AM1-05: `<details>` renders closed with its summary visible, opens and closes from the keyboard and pointer, and is announced as a collapsible control.
- AM1-06: The listed HTML subset renders with its meaning; any other tag or attribute is shown literally or dropped as documented; hostile HTML loads nothing, runs nothing and gives no link outside host admission.
- AM1-07: GitHub's common emoji shortcodes render as their emoji from a generated static table; unknown or coded shortcodes stay literal.
- AM1-08: Each visible construct is accepted by the owner on a frozen build beside GitHub's rendering in Light and Dark, one question per construct; the reader design documents the dialect, fallbacks and limits.

## Owner amendment 2 — 2026-10-16: fenced code as GitHub draws it

The owner decided on 2026-10-16 that fenced code blocks in every body the
shared reader shows (descriptions, comments, review summaries) should read as
they do on GitHub: coloured by language, `diff` blocks coloured by line, and
suggestion blocks in review comments shown as GitHub's **Suggested change**.
"Syntax highlighting may reuse an existing capability but is not a
prerequisite" gives way to the behaviour below for fenced blocks. Indented
code blocks and inline code spans stay as they are, because GitHub does not
colour them either. Amendment 1 and every original criterion hold: source,
copy and publication keep the original bytes, and the work stays inside the
preview budgets.

GitHub's behaviour is observed, not assumed: its Markdown API in gfm mode for
which info strings it highlights and how a `diff` block's lines are marked,
and a review on MikeMarcin/BPR-Test#80 for the suggested-change view.

| Card | Construct |
|---|---|
| [[BPR-1562]] | **Language colouring.** The info string's first word, case-insensitive, cut at its first comma, names a language as GitHub's Linguist does: by name, alias or, with a leading dot, extension (`cpp`, `c++`, `py`, `js`, `ts`, `rust`, `go`, `json`, `yaml`, `sh`, `bash`, `cmake` and the rest of the observed table). The core decides the language; the application host's classification lane classifies the block's text with the packaged definitions on its effect lane, through a host of its own over the same classifier, definitions and limits as the review panes, so it neither waits behind nor reorders their classifications; Qt draws the spans in the review panes' role colours. Colour arrives after the text without moving the reading position, is cached by the block's exact text and definition, and works offline. An unknown language, a language the packaged definitions do not cover, no info string, and a block over the limits stay plain with no message. |
| [[BPR-1563]] | **Diff blocks.** `diff`, `patch` and `udiff` blocks mark their lines as GitHub's diff grammar does: added and removed lines (and their `+++`/`---` file headers) tinted behind the text, the `@@ … @@` range in the hunk-header colour, `diff --git` and `Index:` headers and separators in the constant colour. Every colour is the theme's: the diff roles' fills, inks held readable on them. |
| [[BPR-1564]] | **Suggestion blocks.** In a comment on lines of a pull request, each `suggestion` block is a bordered **Suggested change** box: the commented lines as removed rows, then the block's lines as added rows, numbered from the first commented line, with the changed words of equal-count row pairs marked and the code coloured by the file's language, as on GitHub. The commented lines come from the compared head document when the thread is current on that file; otherwise the box holds the added rows alone. Read only: no apply or commit action, as no native apply exists ([RFC-BPR-0048](RFC-BPR-0048-feedback-authoring-native-github-integration.md) keeps the GitHub handoff). Outside a line comment (a description, issue comment or review summary) a `suggestion` block is plain code, as on GitHub. |

### Amendment 2 acceptance

These criteria belong to the amendment's cards, not to the original closeout.

- AM2-01: Every fenced block GitHub highlights in the info-string fixture is coloured when the packaged definitions cover its language, and every block GitHub leaves plain stays plain; a coloured token has the review panes' colour for its role in all four themes; indented code, inline code and unknown languages are unchanged.
- AM2-02: Colour arrives asynchronously with no change to the scroll position, selection, height or text; the same block text and language is classified once and then served from the cache; a theme change recolours without classifying again; nothing is fetched, and a block outside the limits stays plain.
- AM2-03: Each line kind of a `diff`, `patch` or `udiff` block has GitHub's marking in theme colours readable at 4.5:1 (7:1 in high contrast); an indented diff and inline code are unchanged.
- AM2-04: A suggestion in a current line comment shows the removed and added rows with numbers and changed-word marks as GitHub does; without the commented lines it shows the added rows; a deletion shows only removed rows; outside a line comment it is plain code; there is no apply action.
- AM2-05: The owner accepts each feature (language colouring, diff blocks, suggestion blocks) on a frozen build beside GitHub's rendering in Light and Dark, one question each; the reader design documents the languages, limits and fallbacks.
