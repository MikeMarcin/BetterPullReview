# Conventional Comments feedback: delivery contract

[[BPR-1081]] records the design the owner selected for Conventional Comments
labels, and binds it to the code that will build it. It serves RFC-BPR-0050
(R30 to R33) and the surfaces of RFC-BPR-0048 and RFC-BPR-0049. The authoring
and Overview cards, BPR-1082 to BPR-1089, build against this page.

BPR-1083 built the marks, the tints, the authoring controls and the thread
marks; the Overview, preflight and Settings surfaces are their own cards. A
static frame is not runtime evidence: each surface earns R33 in the real
application on the card that builds it.

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
  turns white or `#111418`, whichever contrasts more. Its rail is 4 px.
- **Non-blocking and if-minor** (owner amendment, BPR-1083 `chip-menu-v002`,
  2026-10-05, verbatim: "I notice we have style for blocking but noting for
  non-blocking or if-minor"; the treatments are BPR-1083 v003's, designed
  with the `frontend-design` skill, and await the owner's
  `decorations-v003`). Until then BPR-1079 had "(non-blocking)" and an
  undecorated label look the same. The decorations now form a scale of
  weight, heaviest first, and the rails borrow road markings: a solid line
  is not crossed, a broken one may be.

  | Decoration | Mark | Rail |
  |---|---|---|
  | `(blocking)` | filled with the tint | 4 px solid |
  | none | soft fill, solid 1.5 px border | 2 px solid |
  | `(if-minor)` | soft fill, top-left corner squared off and filled in the tint (6 px along each edge) | 2 px dotted: 2 px dots, 2 px apart |
  | `(non-blocking)` | open: the panel shows inside a dashed border (2.5 px dashes, 1.5 px gaps) | 2 px dashed: 6 px dashes, 3 px apart |

  Each differs by shape as well as colour, so it reads in greyscale. The
  corner clears every glyph and both letters, and an open mark keeps its
  glyph in ink on the panel, so glyph contrast is at least the soft mark's.
  `qt_feedback_label_marks` checks all four at 16 px in the four built-in
  variants, in icons and letters.
- **Which emphasis applies** (derived). The annotation's `Expectation`:
  `blocking`, `non_blocking` and `if_minor` each have their own; `unspecified`
  and `conflicting` are undecorated. Built-in guidance, such as a todo's
  `necessary_change`, never decorates a mark: it is not an explicit
  decoration (RFC-BPR-0048). Code: `feedback_mark_emphasis` in
  `feedback_label_marks`.
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
- **Style: icons or letters** (owner, 2026-10-05, in chat, verbatim: "i
  think settings should also have a toggle to switch between the icons and
  the shortcode icons, because i liked both of them."). Icons, the default,
  are the twelve glyphs below. Letters put the label's two-letter code (`pr`,
  `ni`, `su`, `is`, `td`, `qu`, `th`, `ch`, `no`, `ty`, `po`, `qb`) in the
  same mark, with the same tints, blocking fill, size and accessible names.
  The choice is the Label marks row of Reviews & queue › Comment labels, kept
  with the profile's review behaviour (workbench settings store, schema 12),
  with six marks beside it (praise, suggestion if-minor, issue blocking,
  question, nitpick non-blocking, todo) drawn in the published style and the
  theme's tints (owner, `mark-style-v001`: "it'd be nice to see a preview"),
  each with its own tooltip, its name and catalog description (owner,
  `mark-preview-v002`: "There should be individual tooltips instead of one
  giant tooltip."),
  and it applies everywhere a mark is drawn: threads, the chip and its menu,
  completion, and the Overview chips and Label key when BPR-1087 and BPR-1089
  draw them through `feedback_mark_theme`.

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
  owner verdict. Built by BPR-1083: `qt_feedback_label_marks` compares all 29
  elements with `tests/fixtures/feedback/bpr-1080-selected-glyphs.txt`, and
  that transcription with the selected page itself.

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
  - A colon alone opens completion with all twelve labels in catalog order
    (owner, `shortcodes-v002`, verbatim: "I feel like since it's only the
    first character that does this replacement just typing : should be
    enough to open suggestions."). Each letter typed after it narrows the
    list, codes first. ↑ and ↓ move through it, and Tab or Enter takes the
    highlighted label. Before v003 a colon and one letter opened it.
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
- **Label first** (derived, BPR-1082). An empty comment takes the label
  alone, as in `issue: `, and until a subject follows it the chip, menu and
  decorations can still change or remove that label. The editor plans these
  edits with `core::decide_feedback_composition_edit`, the header decision
  above with this one case added. Until a subject follows, the recognizer
  reads the body as `malformed` (`empty_subject`). BPR-1083 settles how the
  chip shows it (derived): the chip shows the label and its mark, the
  decoration toggles stay live, and its accessible description and tooltip
  say the label is waiting for its subject. No mark is drawn on a thread for
  such a body, because only a recognized prefix gets one.
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
- **Comment ▾ lists no labels** (owner, BPR-1083 `comment-menu-v001`,
  Accept, 2026-10-05). The review bar's Comment ▾ offers the plain comment
  alone; labels are chosen in the editor's chip. The workbench
  specification's reservation of that menu for labels is withdrawn.
- **As built** (BPR-1083). `feedback_label_controls` is the editor's tool row,
  directly above the field and the next tab stop after it. The menu is a popup:
  Plain comment, the grid with each label's mark, name, code and catalog
  description, and the three decorations under Adds; arrow keys move through
  it, Return or Space chooses, Esc closes. Every change is one edit block of
  the field's text, so one undo step, and the editor's ordinary typed path
  sends it to the draft owner as one whole-body edit, the command
  `plan_feedback_draft_edit` plans for the same label. Completion is a list
  under the caret that never takes focus. The helper's Settings row is
  BPR-1088's; a tests-on fixture launch turns it on with `--label-helper`.
- **One edit with the draft owner at a time** (derived, BPR-1083 v002). The
  replacement of a leading code follows its closing colon at once, while the
  colon's own edit can still be with the draft owner (the private-review
  draft path answers asynchronously). It used to go on the revision that edit
  was based on and was refused as "Draft changed in another window". The
  editor now holds a change made while an edit is unanswered and sends it on
  the revision accepted; with no answer within half a second it goes as
  before. The composed `application_label_authoring_contract` found it.
- **Attended setup** (BPR-1083 v002). A tests-on `--attended-launch` with
  `--attended-open-file PATH` signs in with the fixture's account, opens the
  fixture pull request from the Home bar and selects `PATH` in Read, then
  prints `ATTENDED_FILE_SHOWN:PATH`. Without it the window opens on Home,
  whose bar reads a pull request in public preview, where comments are
  read-only (the owner's `chip-menu-v001` rejection). The automated
  `--label-authoring-probe` course drives the same path through Comment,
  `:qu:`, the chip, ⌘L, Publish and the refreshed thread's mark.

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
    does not move. Non-blocking breaks it into dashes and if-minor into dots
    (BPR-1083 v003, above).
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
- **As built** (BPR-1083). The full cards (`conversation_cards_view`) draw the
  mark under the avatar and the rail exactly as above. The application's
  Discuss and Read drawers show compact entries, the selected conversation's
  post list and its body, so: a compact entry carries its root's rail in its
  own padding, and its root's 16 px mark leads its first line, with the text
  after it (owner, rejecting `thread-marks-v002`: "Marks don't show on inline
  comments."; built in v003, inline threads and my draft's entry alike, with
  the root post's name as the mark's tooltip); each row of the post list
  (`github_conversation_delegate`) has its rail in the row's padding and its
  16 px mark leading the heading, with the body from the subject; the body
  reader (`markdown_body_view`, in the conversation panel only) renders from
  the subject with the named mark before View source and Copy source, which
  keep the prefix. The model reads each body's mark once per accepted body,
  and the post's accessible name is "Review comment by @tbutler, issue
  (blocking)".

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
- Label marks, Icons or Letters (owner, 2026-10-05; BPR-1083 adds this row and
  the Comment labels group, beside Drawers, on the review-behaviour record);
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

- The keyboard route to a single Overview chip (BPR-1089 records it).
- Live website interoperability with a real GitHub host (verification
  milestone). It uses `MikeMarcin/BPR-Test` only.
- Headless parity for prefix edits (BPR-1092). The CLI takes labels by name
  and draws no marks.
