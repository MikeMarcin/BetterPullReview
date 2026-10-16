# Native Markdown reading

RFC-BPR-0036 provides `qt::markdown_body_view`, shared by the expanded PR
overview and every post of the open thread in the conversations drawer and
Discuss, where it reads in place (`set_in_place`, [[BPR-1290]] v005).
The original provider body remains the authority for source, copy and publishing.

## Reading policy

The renderer uses Qt's `QTextDocument::MarkdownDialectGitHub` with
`MarkdownNoHTML`. Its declared subset is paragraphs, headings, emphasis, strong
emphasis, quotes, ordered and unordered lists, fenced/indented code, inline code,
thematic breaks, strikethrough, read-only tasks, tables and links. The owner's
[amendment 1](../rfcs/RFC-BPR-0036-native-markdown-rendering.md#owner-amendment-1--2026-10-16-github-parity)
adds GitHub's HTML comments, line breaks, alerts, footnotes, `<details>`,
GitHub's sanitised HTML subset and emoji shortcodes
([GitHub parity](#github-parity-amendment-1) below). Math, Mermaid, video,
active HTML, editing tools and syntax highlighting are not part of this
delivery. Unsupported syntax can always be read in **View source**. Qt's
Markdown import still never reads HTML (`MarkdownNoHTML`); only HTML the
reader has sanitized to its allow-list reaches Qt's HTML import.

**View source** and **Rendered** retain separate reading positions and selections;
the first switch uses approximate scroll fraction. **Copy source** copies the
original UTF-8 bytes, including original line endings, via the text MIME payload.
Ordinary selection Copy in either reading mode writes only a text MIME
payload. Source-mode selection retains the original UTF-8 range and line
endings. A complete rendered selection copies the original body; a partial
rendered selection serializes the selected native document fragment as
Markdown, retaining source syntax such as link destinations. This partial
serialization can normalize formatting and is not an original-byte mapping.
Qt's rendered document remains a presentation, never the publication source.

Overview uses a compact reader: **View source** / **Rendered** is in the
native right-click menu, with no separate Copy source control or action rows
below the body. A native icon control at the reader's top-right enters or exits
full view. The same documents, source selection and reading positions remain
retained; local Escape returns to Files. Open conversation posts retain their
independent name-row controls.
An empty body has an explicit empty state. A refused rendering budget or an
unverified whitespace mapping shows the complete original source and an
explanation. No prefix is silently presented as the complete body.

Images are shown inline under [RFC-BPR-0084](../rfcs/RFC-BPR-0084-inline-markdown-images.md);
see [Images](#images) below. Qt's own resource loading is never used for them:
the private `markdown_document` still returns a valid empty byte array from
every resource request. Returning an invalid variant would permit Qt's
document/global resource-provider fallback; that is deliberately forbidden. The
architecture guard permits only the exact unnamed address parameter of this
unconditional override. No widget acquires an opener, filesystem capability or
network capability.

All destinations remain untrusted strings. Tab visits link text and shows its
destination; Enter or a deliberate click emits that string to application
admission. The widget performs no scheme parsing, relative resolution or browser
handoff. The window read host checks the exact window generation, session,
account generation, accepted snapshot and a monotonically increasing publication
revision before core admission. The selected post must still exist and not be
deleted. Description and image-placeholder links use this same path. The existing
application-owned browser port performs the effect; Qt receives no credentials.
A failed or unavailable handoff is stated in the window status bar.

Absolute HTTP(S) URLs admit DNS/IPv4 authorities and optional decimal ports,
without userinfo, control characters, backslashes, invalid escapes or
credential-bearing query keys. Other schemes are refused. Repository-root paths
such as `./docs/guide.md` resolve against the accepted full head OID, never a
default branch. A fragment addresses the current PR. Network-path, root-relative,
parent-escaping and encoded relative paths stay readable but are refused because
their interpretation is ambiguous here. IPv6 literals, raw Unicode URLs and other
unsupported address forms remain available in source. No private attachment
authentication or credential-bearing URL is manufactured.

Prose wraps in the reader; fenced code and table-cell text stay on their native
lines. Wide code/tables use the reader's horizontal scrollbar so narrow columns
do not turn readable words into tall stacks of single characters. Source has horizontal scrolling, which avoids
expensive wrapping of a provider-sized single line. Text, links, quote/code
backgrounds and table borders use the inherited palette. Composition supplies
code typography through `set_code_font`; the default is the native fixed font
with four-space tab stops. Qt handles native focus, selection and accessibility.

Qt expands code tabs during Markdown import. A bounded source-aware repair
retains literal tabs in top-level fenced code only after the complete sequence
of imported code lines equals the tab-expanded source lines. No partial repair
is published. Tab-bearing constructs outside that verified mapping (for example
a fence nested inside a quote, indented tabbed code or a prose tab) explicitly
fall back to source. This limitation is part of acceptance, not a claim of full
whitespace fidelity in every Markdown construct. Non-list blocks also have
inherited task markers removed; Qt otherwise carries the last task marker into
later code/table/paragraph blocks.

## Images

RFC-BPR-0084 shows images the way GitHub does. A reader whose holder gives it
an image dispatch (`set_image_dispatch`) replaces each image of the imported
document with one object character drawn by `detail::markdown_image_store`
(`src/adapters/qt/src/markdown_images.*`): Markdown `![alt](src)` and
reference images, and GitHub's `<img src alt width height>` in prose, lists,
quotes and table cells, found by the core's bounded `scan_html_images`. An
`<a href>` around exactly one image makes it a linked image; a `<picture>`
whose `<source>`s select only by `prefers-color-scheme` shows the source for the
reader's palette, and a theme change asks again. Qt's Markdown import drops a
link written around an image, so `core::markdown_linked_images` pairs each
`[![alt](src)](link)` in the source with its image. An `<img>` in code stays
text. Without a dispatch (tour previews, local drafts, dialogs) every image
stays the text placeholder `[Image: alt — open image link]` and nothing is
asked for.

The reader sends `{ordinal, source}` references, untrusted text in document
order; the holder adds its body identity and the window's Markdown context.
Answers (`application::markdown_image_projection`) carry the body identity,
ordinal, the source as sent, a state and shared decoded pixels, never an
address or credential; a reader ignores one that does not name an image of its
current body. While loading, an image is a quiet rounded box in a tint of the
palette's text over its base: `width` and `height` reserve its size, otherwise
it is one line as wide as its alternative text. When pixels arrive the body
reflows once; an image above the reading position moves the scroll by its
growth so the text being read stays put, and selections keep their positions.
Sizing follows GitHub (`core::markdown_image_display_size`): natural pixel size
in logical pixels, `width`/`height` in pixels or a width percentage of the
reader, aspect ratio kept, never wider than the text width (a table cell's
share of it). Frames draw at the screen's device pixel ratio from a cached
scaled pixmap; an uploaded attachment's corners are rounded by 6 px as on
GitHub. An animated GIF animates with its delays (a delay of 10 ms or less
shows for 100 ms, as in browsers) while the reader is visible. A failed,
blocked, too-large, unsupported (SVG, WebP) or over-limit image is replaced by
the placeholder text, with the reason in parentheses for the last three.

An image object carries its link (or its own source) as anchor, so Tab, Enter
and a click reach it through the same untrusted-destination path as links;
Tab sets the reader's accessible description to the placeholder text and the
destination. A partial rendered copy writes each image back as Markdown. The
object type is a `QTextObjectInterface` handler found through `qt_metacast`, so
the adapter needs no meta-object compiler and no `QUrl`.

## GitHub parity (amendment 1)

[[BPR-1543]] makes four constructs read as GitHub renders a pull request
description or comment, checked against GitHub's own Markdown API output for
the fixtures in `tests/fixtures/markdown/github-parity-v1`. The private
`markdown_github` unit works on both sides of Qt's import:

- **Preparation** rewrites the source line by line before `setMarkdown`. A
  line classifier finds fenced and indented code, erring toward code, and an
  inline scanner finds code spans as CommonMark pairs backtick runs. Code is
  never rewritten.
- **Presentation** turns the marks the preparation left into native document
  structure after the import and the code-tab repair.

Marks are U+2008 PUNCTUATION SPACE for a line break and the Unicode
noncharacters U+FDE1 to U+FDEC for alerts and footnotes (Qt reserves U+FDD0
and U+FDD1 for frames). A body that already holds U+2008 keeps its soft
breaks; one that holds any of U+FDE0 to U+FDEF is read without alerts and
footnotes. Authored text is therefore never taken for a mark, and no mark
reaches the rendered document.

| Construct | Rendered as |
| --- | --- |
| `<!-- … -->` | Hidden. A comment that starts a line is an HTML block and runs to its `-->` across blank lines, or to the end of the body when never closed, as on GitHub. Any other comment closes within its paragraph or stays literal. Comments in code stay. |
| A single newline in a paragraph, list item, quote or footnote | A line separator in the same block, as GitHub's `<br>`. A written hard break (two spaces or a backslash) is the same separator; Qt otherwise splits the block, which in a list drew a second bullet. Headings, rules, table rows, reference definitions, code spans that cross the line, and code keep the newline. |
| `> [!NOTE]`, `[!TIP]`, `[!IMPORTANT]`, `[!WARNING]`, `[!CAUTION]` | A top-level quote whose first line is the marker alone and that has a line after it. Its title is GitHub's word in the kind's colour, semibold, without an icon; a 4 px bar in that colour runs down the left of the alert, which has no quote fill. A heading or table inside the alert, which Qt does not mark as quoted, stays inside it: the preparation ends the alert with a mark paragraph after the quote. A marker with text on its line, or inside a list, is a plain quote. |
| `x[^label]` and `[^label]: text` | Definitions (top level, with indented or lazy continuation lines) move to a footnotes section after a rule at the end, numbered in the order of first reference; a definition nothing references is dropped and a reference with no definition stays literal. A reference is its raised number in brackets (`[1]`), as GitHub's stylesheet draws it; each footnote ends with GitHub's `↩` back references, the second and later numbered (`↩²`). |
| `_text_` | Emphasis. Qt's dialect draws it underlined, which also read as a link. |

Alert colours start from GitHub's own (`#0969da`, `#1a7f37`, `#8250df`,
`#9a6700`, `#d1242f` on a light base; `#4493f8`, `#3fb950`, `#ab7df8`,
`#d29922`, `#f85149` on a dark one) and move toward the text colour until they
read at 4.5:1 on the reader's base, or 7:1 where the text itself is at 18:1
or more (the high-contrast themes). The footnotes are set at 0.86 of the body
size in the text colour 32% of the way to the base. On any dark base, not only
in place, a link keeps its hue at a readable lightness: a footnote's raised
number is a link.

Footnote references and back references are in-body links. Their target is a
character property only presentation sets, so an authored link such as
`[x](#fn-1)` still goes to application admission. Enter or a click on one
selects the other and scrolls the reader to it, so Enter there leads back and
Tab continues from it; no gesture reaches the application. In place the reader
does not scroll: a holder that is a `QScrollArea` is asked to show the target,
and any other holder keeps its scroll position while the selection moves.
Copying a partial rendered selection serializes the presented document, so it
carries "Note" and the footnote numbers rather than the source syntax; a full
selection still copies the original bytes.

### `<details>`, the HTML subset and emoji ([[BPR-1544]])

GitHub's renderer passes raw HTML through an allow-list. The reader keeps the
same subset, compared with GitHub's own output for `details`, `html`, `emoji`,
`hostile-html` and `pr-html` in the same fixture directory:

| HTML | Rendered as |
| --- | --- |
| `<details>`, `<details open>` with `<summary>` | A section, closed unless `open`. Its summary leads with a disclosure triangle (▸ closed, ▾ open) in the text colour, and the whole summary is an in-body toggle: Tab reaches it, Enter or a click opens or closes it, and its accessible description says "Collapsed section" or "Expanded section". The section's blocks are hidden with Qt's block visibility, so its lists, code and tables keep their structure; a hidden table also gives up its borders and spacing. Tab skips links inside a closed section. A section without a summary is called "Details", as on GitHub. The Markdown between the tags reads, as GitHub's does when blank lines part it from them. Nested sections each keep their own state. Opening a section changes no text, so selection, links and the original bytes stay as they were. |
| `b`, `strong`, `i`, `em`, `s`, `strike`, `del`, `ins`, `sub`, `sup`, `kbd`, `code`, `tt`, `samp`, `mark`, `br` | Inline formatting over the Markdown between the tags (`ins` underlines, as GitHub's browser does; `mark` is the base 30% toward the theme's warning colour, GitHub's attention tint). `kbd` is a key cap: the code font at 0.85, a fill 6% from the base toward the text, a hairline rounded edge 70% from the text toward the base, and a thin space inside each end. An unclosed tag ends with its paragraph. |
| `p`, `div`, `h1`–`h6`, `hr`, `blockquote`, `ul`, `ol`, `li`, table tags, `pre`, `dl`, `dt`, `dd` | A top-level HTML block (to its blank line, or to `</pre>`) is sanitized and placed through Qt's HTML import: only `align` (on `p`, `div`, headings and cells) and `colspan`/`rowspan` (on cells) survive. A lone `<div align>` or `<p align>` followed by a blank line aligns the Markdown up to its lone closing tag, as GitHub's centered README headers do. `pre` reads as a code block. |
| `a href` | A link only when core's `markdown_html_link_kept` says its destination could ever be admitted (HTTP(S), a fragment or a path, without credentials); otherwise its text alone, as GitHub drops a `javascript:` link. Activation still goes through application admission. GitHub keeps a credential-bearing link that the reader shows as text. |
| `script`, `style`, `iframe`, `textarea`, `title`, `xmp`, `noembed`, `noframes`, `plaintext` | Visible text, as GFM's tag filter shows them. |
| `img`, `picture`, `source`, and an `a` around one | Left as written for [RFC-BPR-0084](../rfcs/RFC-BPR-0084-inline-markdown-images.md), which places images in the presented document. |
| `u`, `small`, `span`, `font`, `center`, `form`, `input`, `button`, `svg`, `object`, any other tag | The tag is dropped and its text kept, as GitHub's sanitizer does (GitHub strips `u`, though the amendment named it). |

Every attribute other than those above is dropped, so no style, class, event
handler, `src` or `background` reaches Qt. Qt's HTML import is given only
structure tags; formatting travels as the reader's own marks (U+FDEB `<br>`,
U+FDED and U+FDEE around a span, U+FDEF for a block). The resource denial of
`markdown_document` still answers every request, and the hostile courses count
none.

Emoji shortcodes (`:+1:`, `:tada:`) come from a table of gemoji 4.1.0's 1,913
aliases (MIT, `third_party/gemoji`), generated by
`scripts/generate_markdown_emoji_table.py` into
`src/adapters/qt/src/markdown_emoji_table.inc`; nothing is fetched. As GitHub's
filter, a shortcode needs a boundary on both sides (`a:smile:b` and `10:30:45`
stay), and code, URLs, link destinations and HTML tags are left as written.
Shortcodes are replaced even in a body that holds a reserved mark.

## Bounds and lifecycle

Provider descriptions are already admitted at 262,144 bytes. Preview adds these
conservative pre-parse work budgets; they count literal syntax, including code,
and may refuse a document that a full Markdown parser could accept:

| Budget | Limit |
| --- | --- |
| Rendered UTF-8 source | 65,536 bytes |
| Source lines / bytes in any line | 2,048 / 4,096 |
| Leading whitespace/quote prefix | 64 columns; tab counts as four |
| Quote prefix / open square brackets | 8 / 16 |
| Formatting characters `*_~` plus backticks, brackets, angle brackets and pipes | 8,192 |
| Pipes in any line | 32 |
| Pipe-bearing lines × (maximum pipes + 1) | 1,024 |

The budgets are measured on the original body, before the GitHub-parity
preparation, which adds at most one mark to each line, a few characters to
each alert and moves footnote definitions without copying them. The bracket
count spans source lines. Table budgeting bounds rectangular
expansion before Qt allocates native cells. Eager preparation was selected after
measuring admitted long prose and rejected nesting. It runs synchronously on the
GUI lane under these budgets: no worker, timer, late completion or cross-window
result exists. Owners must clear the reader on authority invalidation and pass
an identity unique to the current body/surface. An identical identity and body
performs no document mutation; a replacement clears selection and link state.

The 262,144-byte source path is measured separately. It does not establish
latency guarantees for arbitrary unadmitted input. Measurements and exact
platform context are in [the verification ledger](../verification/RFC-BPR-0036.md).

## Initial surface inventory and integration contract

| Surface | Current path | Delivered behavior |
| --- | --- | --- |
| PR overview | `pull_request_overview_page.cpp`, `overview_description_view` | RFC0063 places the retained reader in a resizable, scrollable normal summary beside the independent Files viewport. The optional expanded viewport keeps the same documents, Return/Escape, source anchors and file focus. |
| Public conversation, open thread | `conversation_open_card.cpp` (`thread_post_view`), `conversation_cards_view.cpp` | Since [[BPR-1290]] the drawer has no post list or body pane: each post of the open thread reads in place through the shared reader, as tall as its content, with View source and Copy source in its name row and its links through the window's admission with the post's identity. Review summaries, GraphQL roots/replies, REST roots/replies and issue comments are included. |
| Closed notes in the drawer and Discuss | `conversation_cards_view.cpp` | One- or two-line navigation summaries of the first post; the open thread owns the complete public body. My draft's open note renders its body the same way; editing keeps its plain-text contract. |
| AI side chat | `side_chat_panel.cpp` | Inventoried, outside initial delivery. |
| Local review/reply editors and draft cards | `review_draft_editor.cpp`, `review_submission_editor.cpp`, draft `conversation_cards_view` entries | Preserve Markdown editing and payloads; do not silently adopt rendering in this slice. |

In place, the reader has no vertical scrolling of its own and reports its
height for a width, so the open thread grows with its posts and a vertical
wheel scrolls the code the drawer follows. Its source controls and destination
line are the holder's: the post's name row has View source and Copy source,
and a link's destination is its tooltip, shown at once when Tab reaches it.
The reader's own control row is hidden in place unless a label mark shows,
and the height for a width counts that row when it shows: a visible empty row
still takes the layout's spacing, which cut the last pixels off every post
([[BPR-1332]]).
Headings keep to the body's scale, lists indent by half Qt's step, links on a
dark panel keep their hue at a readable lightness, and a recognized
Conventional Comments header is set in semibold as the notes set it.

The wrapper exposes the rendered and source text widgets for layout, focus and
selection integration. It does not own PR geometry, the conversation model,
publication, receipt state or accepted authority. The composed acceptance course
opens a synthetic PR through the real read host,
reads each complete body, activates admitted links and reaches the final long
paragraph. REST preview groups now travel in the accepted per-window projection;
profile-wide status cannot overwrite another window's bodies. A new body starts
at its first character; identical body/identity does not mutate either document.
Native Cocoa rendering, accessibility interfaces, keyboard events and local
horizontal overflow are checked. This does not claim VoiceOver certification,
physical-input attendance or testing on unavailable platforms.

## Renderer comparison requested by the owner

Mandate r2 explicitly adds md4qt versus native Qt for GFM to the plan. The live
plan preserves every original exit and adds a comparison exit, with its checked
amendment receipt beside it. Selection below was reconsidered after compiling
and probing current md4qt, rather than treating the initial Qt implementation as
settled.

The comparator is canonical KDE md4qt **5.1.4**, revision
`34d059e066e0dac138f0bc8144fe1d3acbba3dc3` (2026-09-19). The KDE GitHub mirror's
default branch was stale 4.0.0, so it was not used for the decision. Current
[upstream source](https://invent.kde.org/libraries/md4qt/-/tree/34d059e066e0dac138f0bc8144fe1d3acbba3dc3)
and [API documentation](https://api.kde.org/md4qt-module.html) identify a static
C++17/Qt Core parser with an AST, source positions, extensible parsing and HTML
export. Its implementation is MIT licensed. It adds a pinned source dependency,
notice and central-provider maintenance obligation; the inspected release notes
record incompatible 4.x and 5.x APIs. Recent changes and named consumers show
active maintenance, not a guarantee of future compatibility.

Qt's native Markdown API was evaluated with 6.10.3; the current provider is
6.11.2. Its
[native Markdown API](https://doc.qt.io/qt-6/qtextdocument.html#MarkdownFeature-enum)
uses [MD4C](https://doc.qt.io/qt-6/qtgui-attribution-md4c.html), an MIT-licensed
parser already included in Qt Gui. No new shipped dependency is needed. Qt's
native text structure provides the existing selection, layout and accessibility
surface directly.

| Dimension | Native Qt candidate | md4qt 5.1.4 candidate |
| --- | --- | --- |
| Declared corpus | Native heading, list hierarchy, 2 task markers, quote/code, table, links and inline styles pass after the documented marker repair. | AST contains the same declared structures; exact source positions and code tabs are useful advantages. |
| Additional dialect | Uses Qt's declared GitHub dialect; no claim of complete GFM conformance or GitHub styling. | Documents CommonMark 0.31.2, GitHub autolinks, tasks/tables/strikethrough plus footnotes and math; some extensions exceed this RFC. |
| Native rendering | Direct `QTextDocument` import. | A parser, not a native widget renderer. The measured HTML-export bridge lost task markers (`input` checkboxes are not native text tasks). A proper AST-to-document visitor could retain them but was not implemented or benchmarked. |
| Resources and raw HTML | HTML disabled, all resource requests denied, images replaced before enabled layout. | The stream entry avoids recursive file parsing, but `LinkImageParser::prepareUrl` still calls `QFileInfo::exists` and rewrites existing relative files. HTML export includes raw HTML and images; a safe visitor and an effect-free link preparation path are required. |
| Original source | Retained separately; checked code-tab repair and explicit source fallback where it cannot prove a mapping. | Retained original still necessary; AST positions could improve precise rendered/source anchoring and broader tab handling. |
| Accessibility | Native text widgets, keyboard link traversal and source controls already exercised. | Could share the same native widgets once a safe AST visitor exists; parser success alone proves no accessibility result. |
| Performance | Local 64 KiB native import/layout around 19 ms in the standalone probe. | Around 2.9 ms AST parse plus 20 ms HTML export/native layout for that body. The result does not measure an unbuilt direct AST visitor. |

The synthetic relative-link probe parsed `[relative](canary.md)` from a memory
stream before and after creating that one file in a temporary directory. The
md4qt destination changed from `canary.md` to an absolute local pathname. This
proves filesystem-dependent link interpretation even without recursive parsing;
it does not claim that file content was read. The exported synthetic HTML also
contained the raw script tag. Neither result means md4qt cannot be adapted
safely; they identify integration work required by this application.

**Decision:** retain native Qt for the initial bounded reader. It supplies the
required native structures with a smaller effect boundary and no additional
shipped package. The empirical timings do not justify choosing on speed alone.
md4qt remains the stronger candidate if precise source mapping or broader dialect
support warrants a safe custom AST renderer and removal/override of its local
link checks. Do not route its ordinary HTML export into production as a shortcut.
