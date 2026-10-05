# Conventional Comments feedback: delivery contract

[[BPR-1081]] records the design the owner selected for Conventional Comments
labels, and binds it to the code that will build it. It serves RFC-BPR-0050
(R30 to R33) and the surfaces of RFC-BPR-0048 and RFC-BPR-0049. The authoring
and Overview cards, BPR-1082 to BPR-1089, build against this page.

Nothing here is implemented. A static frame is not runtime evidence: each
surface earns R33 in the real application on the card that builds it.

## Sources and authority

Each row says who decided it. **Owner** means a verdict in an attended
session. **Existing** means a contract that was already in force. **Derived**
means this card wrote it down to fill a gap that the owner's selection leaves.
A derived rule may be revised by the card that builds it, if that card records
the change. An owner rule changes only with a new owner verdict.

| Source | Revision | SHA-256 | Holds |
|---|---|---|---|
| [BPR-1079 selected design](../design/BPR-1079-feedback-ux-selected.html) | `d7c828fa9` | `d8acbe66…ff90db` | Surfaces, rules, built-in tints |
| [BPR-1079 frames v2](../design/BPR-1079-feedback-ux-frames-v2.html) | `48b588d25` | `941118fd…5a257` | Overview, authoring, preflight and Settings drawings |
| [BPR-1079 frames v4](../design/BPR-1079-feedback-ux-frames-v4.html) | `6793935cd` | `4946afd1…dcc386` | Thread cards |
| [BPR-1080 selected icons](../design/BPR-1080-label-icons-selected.html) | `759ecc692` | `776e3764…ab03e` | The twelve glyphs, exact path data |
| [BPR-1080 frames v4](../design/BPR-1080-label-icons-frames-v4.html) | `44d09faa3` | `fa185de2…d236b` | The reviewed `GLYPHS` table, measurements |

The owner's sessions:

- BPR-1079: `ca54b0e8` (v1), `4742c47e` (v2), `830ffc2a` (v3) and `3a17a6eb`
  (v4).
- BPR-1080: `82eb35d6` (v1), `22be5050` (v2), `bfc59159` (v3) and `d7393d71`
  (v4).

The cards hold every verdict verbatim: [BPR-1079](../issues/60-done/BPR-1079-develop-compact-authoring-counter-and-resolution-ux-candidates.md)
and [BPR-1080](../issues/60-done/BPR-1080-develop-all-twelve-reusable-label-icon-candidates.md).

## The label vocabulary

### Labels and codes

The twelve labels are `core::feedback_label`, in catalog order. Each has one
icon, one tint role and one shortcode (owner, BPR-1079 v2; BPR-1080 v1 to v4).

| Label | Code | Glyph | Label | Code | Glyph |
|---|---|---|---|---|---|
| praise | `:pr:` | heart | chore | `:ch:` | broom |
| nitpick | `:ni:` | loupe | note | `:no:` | information i |
| suggestion | `:su:` | light bulb | typo | `:ty:` | Abc over a squiggle |
| issue | `:is:` | exclamation mark | polish | `:po:` | sparkles |
| todo | `:td:` | ticked box | quibble | `:qb:` | small wobble |
| question | `:qu:` | question mark | thought | `:th:` | thought cloud |

### The mark

- **Shape** (owner). A 16 px rounded square with a 1.5 px border in the
  label's tint. The glyph sits in a 12 px box in the middle, drawn in ink, over
  a soft fill: the tint at 22% over the panel in light and 30% in dark. Labels
  are square and states are round, so the two never share a shape.
- **Blocking** (owner). The mark is filled with the full tint, and the glyph
  turns white or `#111418`, whichever contrasts more. "(non-blocking)" and an
  undecorated label look the same.
- **When the fill applies** (derived). Only when the annotation's
  `Expectation` is `blocking`. `if_minor`, `non_blocking`, `unspecified` and
  `conflicting` keep the soft mark. Built-in guidance, such as a todo's
  `necessary_change`, never fills a mark: it is not an explicit decoration
  (RFC-BPR-0048).
- **Which bodies get a mark** (derived from the owner's "plain posts get a
  grey rail"). Only a `recognized` prefix gets a mark. `plain`,
  `unknown_label` (a custom label), `malformed` and `limited` bodies get no
  mark, and their text is shown exactly as written.
- **Name** (owner). The mark is an image whose accessible name and tooltip
  are the label and its standard decorations, as in "issue (blocking)". A post
  is named with its author first, as in "tbutler, issue (blocking)" (owner,
  BPR-1079 v4). When several standard decorations are written, the name lists
  them in written order, separated by commas (derived). Custom decorations stay
  in the text and are not added to the name (derived).
- **Colour is never the only cue** (owner). The shape names the label, and
  the fill shows blocking, so both read in greyscale. Measured on the frames:
  glyph contrast is at least 6.06:1 on the soft fill and 4.52:1 on the blocking
  fill, in all four theme variants.
- **Painting** (existing, Qt adapter rules). The marks are painted with
  `QPainter`, with no QSS and no forced palette. They are redrawn when Qt
  reports a palette, style or theme change.

### Tints

The owner accepted the tints in BPR-1079 v2 (`tints-v002`). The built-in
values for light, dark, high-contrast light and high-contrast dark are the
table in the BPR-1079 selected design. No default uses the amber of open or
the green of resolved.

- **Roles** (owner). There are twelve colour roles, "Label praise" to "Label
  quibble". They form a fifth group, Comment labels, on Appearance › Colours.
- **Core vocabulary** (derived). The roles are added to
  `core::appearance_role` after the syntax roles, so the vocabulary grows from
  47 roles to 59. Their stable names are `feedback_label_<id>`, as in
  `feedback_label_issue`. Theme documents key on these names, so they must not
  change once shipped.
- **Offered count** (owner). The Colours card offers 57 roles instead of 45.
  RFC-BPR-0034's amended C5 says 45. The card that adds the roles records the
  new count there as an amendment, citing `tints-v002`, as was done for
  BPR-0986.
- **Missing roles** (owner). A stored palette or theme document with no label
  roles reads them as the built-in tints, and no other role changes. Import,
  Export and Reset cover the label roles like any other role.

## Icon delivery

- **Bound inventory.** The twelve glyphs are exactly the path data in the
  BPR-1080 selected icons at `759ecc692`. That data is the `GLYPHS` table of
  BPR-1080 frames v4 at `44d09faa3`, the file the owner reviewed. Each glyph
  matches, byte for byte, the round in which it was accepted.
- **Where it lives** (derived). One Qt adapter unit:
  - `src/adapters/qt/include/bpr/qt/feedback_label_marks.hpp`
  - `src/adapters/qt/src/feedback_label_marks.cpp`

  It holds the glyph data as compiled `QPainterPath` construction and one
  lookup keyed by `core::feedback_label`. It also holds the mark painter
  (soft and blocking) and the accessible-name text. Every surface draws marks
  through that unit, so the twelve icons have one resource vocabulary
  (BPR-1083).
- **How it ships** (owner for the form; derived for the path). The unit
  compiles into `better_pull_review_adapter_qt`, a static library that
  `src/app` links into the application executable. That is the whole
  packaging path:
  - no file under `packaging/` or `src/adapters/qt/assets/`;
  - no Qt resource file and no Qt SVG module;
  - no icon font, runtime fetch or build-time generation;
  - no entry in `packaging/dependencies.json`, because the glyphs are the
    repository's own work, hand-written for BPR-1080.

  The core keeps only the semantic role string `feedback.label.<id>`. The
  core-only build and the `bpr` CLI carry no icons, and take a label by name.
- **Drawing rules.** They are on the BPR-1080 selected page:
  - a 24-unit grid with round caps and joins;
  - a 2.6-unit pen, except where an element names its own width (typo's
    squiggle is 2.0, chore's handle 2.2);
  - stroke, fill and disc elements;
  - M, L, C and Z mapped to `moveTo`, `lineTo`, `cubicTo` and
    `closeSubpath`;
  - holes wound against their outlines.
- **Proof of fidelity** (derived). The implementing card adds a Qt test that
  compares the compiled element list, entry by entry, with a checked-in
  transcription of the selected page's path data. Changing a glyph needs a new
  owner verdict.

## Surfaces

### Composition (BPR-1082, BPR-1083; RFC-BPR-0048)

Consumers: the inline composition page and reply drafts
(`review_draft_editor`, over the shared draft services of BPR-1082).

- **Chip and menu** (owner, authoring A). The editor's tool row starts with
  a label chip, "Plain ▾" or the current label. Its tooltip reads "Label (⌘L),
  or type a code such as :qu: at the start". Clicking it, or ⌘L in the editor,
  opens the menu:
  - "Plain comment" first, with ⌘0;
  - a 3×4 grid of the twelve marks with their names and codes, each with its
    catalog description as a tooltip;
  - the decorations under "Adds": blocking, non-blocking and if-minor.

  Once a label is chosen, the same three decorations also sit beside the chip
  as toggles. ⌘0 is the window's Home shortcut, so it is a key of the open menu
  only and never a window shortcut (derived).
- **Shortcodes** (owner, v2). They work only at the very start of the
  comment:
  - A colon and one letter open completion, codes first. ↑ and ↓ move through
    it, and Tab or Enter takes the highlighted label.
  - The closing colon of an exact code replaces the code at once.
  - A code in front of an existing label replaces that label; it never adds a
    second prefix.
  - Esc, an unknown code or pasted text leaves the text alone. GitHub's own
    `:emoji:` codes elsewhere are never touched.
- **One edit** (owner, existing). Every chip, menu and shortcode change is
  one draft edit through `core::decide_feedback_header_edit`. It is one undo
  step, and ⌘Z right after a shortcode restores the literal code. Plain
  removes only the recognized prefix. Malformed leading syntax is never
  deleted. A stale body revision is refused or offered as a conflict
  (RFC-BPR-0048).
- **Helper off by default** (owner, Settings v1). With the label helper off,
  there is no completion or replacement, and `:qu:` is plain text. A label
  typed out in full counts either way. The last `(blocking)` used is never
  carried to the next comment (RFC-BPR-0048).
- **Shown while writing** (owner, v4). The label appears as the highlighted
  words at the start of the field, and the mark and rail follow them.
- **⌘L collision** (derived). `homeFocusBarAction` already binds Ctrl+L
  (⌘L) with window context in `workbench_window.cpp`. The editor claims ⌘L
  through `QEvent::ShortcutOverride` while it has focus, so the label menu
  opens there, Home's Open bar keeps ⌘L everywhere else, and Qt never sees an
  ambiguous shortcut. A test covers both.
- **Keyboard and narrow windows.** The chip is a tab stop after the body.
  Every label and Plain can be reached by keyboard (RFC-BPR-0048). The chip
  does not change size at narrow widths. Focus returns to the body, with the
  caret after the prefix, when the menu closes.
- **Open.** The workbench specification reserves the Comment ▾ menu on the
  review bar for labels. The owner selected the chip, not that menu.
  BPR-1083 delivers the chip. If it proposes listing labels in Comment ▾ as
  well, that is asked of the owner first.

### Thread cards and detail (BPR-1083; RFC-BPR-0050)

Consumers: `conversation_cards_view`, `github_conversation_delegate` and the
selected thread's detail (`markdown_body_view`).

- **Layout** (owner, v4):
  - Every post, root and reply alike, has a 16 px mark centred under the
    author's 18 px avatar, 3 px below it.
  - A rail in the label's tint runs down the post's left edge, 3 px left of
    the avatar, in the card's existing padding or the reply's indent. It adds
    no width. Plain posts get a grey rail.
  - Blocking makes the rail 4 px instead of 2 px, growing outward, so the text
    does not move.
  - No card header holds a mark.
- **Reading** (owner, v1 and v4). The mark stands in for the recognized
  prefix, so the body starts at the subject. Copy source and View source keep
  the prefix byte for byte (R32).
- **Replies** (owner). A reply shows its own mark, which may differ from its
  root's mark. Replies never add to a count.
- **Kept as they are** (owner, R32):
  - The private defect badge stays dashed and locked, and never becomes a
    public label.
  - A suggested-change block keeps its own header and apply affordance, and a
    label never restyles it.
  - A tour stop is plain unless its author labelled it (BPR-1086).
- **Refresh** (existing). Marks follow the accepted body revision. A refresh
  keeps the selection, expansion and caret, and an unchanged body is not
  reparsed (BPR-1087).

### Overview Comments column (BPR-1087, BPR-1088, BPR-1089; RFC-BPR-0049)

Consumers: `overview_file_model::comments_column` and `overview_row_delegate`
in `pull_request_overview_page`. The figures come from
`core::project_feedback_counters`.

- **Chips** (owner, Overview B):
  - The column holds label chips: a mark and a count. Only non-zero chips
    appear, in the Settings order.
  - The speech bubble is Other, the catch-all, once any chip is chosen. With
    none chosen it is the plain Comments count. The Settings default is no
    chips (RFC-BPR-0049).
  - The chips plus Other equal the included conversations in every scope.
  - The column has a fixed width. Chips that don't fit fold into "+n", whose
    tooltip names each folded chip and its count.
- **Header menu** (owner). Comments ▾ chooses the label to sort by. Its
  tooltip says what is counted and that replies don't count.
- **Open and All** (owner, overview states):
  - Open is the default and is `feedback_resolution_mode::unresolved`. All
    counts the whole admitted population.
  - In All, a state bar under each number shows open, resolved, unknown and
    not resolvable, using the four round state marks.
  - The active mode belongs to the view state, not to Settings (RFC-BPR-0049).
- **Not on a file** (owner). A last row holds unanchored threads, pull request
  comments and review summaries (`Unattributed`).
- **Unknown states** (owner, with the core's facts):
  - A ? chip beside the switch counts what Open leaves out (`unknown`,
    `not_applicable`) and what has no known label (`ScopeUnclassified`).
    Clicking it lists those conversations in Discuss.
  - Unknown and not resolvable are never shown as resolved.
  - If the projection is not `Exact`, the ? chip shows even when every count is
    zero. Its tooltip names each limit: partial, stale or unavailable
    collection, unknown labels, unknown resolution or unknown file. A blank
    cell means a complete zero and nothing else (derived from R24 and R26).
- **Label key** (owner). ⌥K, or a keyboard-reachable Label key button, lists
  all twelve marks with their names and codes. A tooltip is never the only way
  to learn a mark.
- **Drill-down** (owner). Activating a chip opens Discuss filtered to that
  scope, label and mode, which are the same conversations the number counted.
- **Accessible text** (derived from RFC-BPR-0050). A count's accessible
  description carries the label, the unit ("conversations"), the mode and any
  limit, as in "issue: 2 open conversations; resolution unknown for 3".
- **Open: the keyboard route to a single chip.** The owner's frames show only
  the pointer gesture. BPR-1089 picks a keyboard route within the table's
  existing focus model and records it. One example is Enter on a Comments cell
  opening a menu of its chips. The route must reach every chip, Other and
  "+n", and keep focus across refresh. Changing chips or mode makes no
  provider request (BPR-1087).

### Review submission preflight (BPR-1085; RFC-BPR-0048)

Consumer: `review_submission_editor`.

- **Decisions** (owner, preflight B). Comment, Approve and Request changes
  are three visible, explicit buttons. They replace the combo box and keep
  their native disposition values.
- **Chip.** One chip, "Your blocking feedback", gives counts: blocking issues,
  drafts and unknown coverage. It opens the list on demand.
- **List.** The list shows only my own explicitly blocking items, published
  or pending, with links to them. Uncertainty has its own row. A chore and
  other label guidance appear separately and never as blocking. Other
  reviewers' items stay under "Others".
- **Approve.** It carries an amber dot while I have open blocking feedback.
  Its tooltip says that approving does not resolve it.
- **No automatic decisions.** No label or resolution sends a review or picks
  a disposition.

### Settings (BPR-1088; RFC-BPR-0049)

Consumer: `settings_window`, page Reviews & queue, section Comment labels
(owner, Settings v1, adjusted to Overview B in v2). The section holds:

- the label helper, off by default;
- the default count, Open;
- the Overview chips, ticked and dragged into order (⌥↑ and ⌥↓ also move
  them);
- a "Mark colours" row that opens Appearance with its filter set to "label";
- a Reset that covers this section only.

The values persist through the existing typed profile settings store, and the
Comments drawer's own filter is not touched.

## What every surface keeps

These are owner rules, unchanged from BPR-1079.

- **Counting.** One conversation counts once, classified by its root.
- **Public text.** A label is public text at the start of the comment. The
  preview shows exactly what GitHub gets. A shortcode is never sent.
- **Native decisions.** No label resolves, approves or requests changes.
- **Uncounted.** Drafts, private notes and pending posts are never counted.

## Native evidence each building card owes (R33)

Each card captures its own surfaces in the real application:

- at 100%, 125% and 200%;
- in light, dark and both high-contrast themes;
- in a narrow window (960 px) and a wide one (1440 px);
- with larger text;
- with the keyboard only;
- with the platform screen reader (VoiceOver or Narrator).

Each card also checks that no surface brings back a clipped sidebar
composer, a duplicate thread body, nested scrolling or the old small
description reader (RFC-BPR-0050). Offscreen renders are layout evidence only.

## Open, not decided here

- Whether Comment ▾ on the review bar lists labels (BPR-1083 asks the owner
  if it proposes it).
- The keyboard route to a single Overview chip (BPR-1089 records it).
- Live website interoperability with a real GitHub host (verification
  milestone). It uses `MikeMarcin/BPR-Test` only.
- Headless parity for prefix edits (BPR-1092). The CLI takes labels by name
  and draws no marks.
