---
schema: "implement-rfc.card.v1"
id: "BPR-1079"
title: "Develop compact authoring, counter and resolution UX candidates"
status: "Done"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0050"]
depends_on: ["BPR-1075"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-05","milestone":"design","acceptance":["R30","R33"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1079:constraints","outcome":"pass","revision":"59f65ad9e","evidence":{"kind":"artifact","path":"docs/design/BPR-1079-feedback-ux-frames-v1.html","detail":"Frames at 1440x820 and 960x700-760 (Settings at its 760x560 minimum), rendered headless and inspected: Overview A and B with none, issue+question and all twelve label columns; twelve at 960 px pin File and scroll (A) or fold into +n (B); 0/1/11/104 counts; long paths elide; long comment body with a native suggestion; labelled reply under a differently labelled root; unknown coverage chip and Not on a file row. Static design, not native evidence."}},{"criterion":"BPR-1079:explains","outcome":"pass","revision":"59f65ad9e","evidence":{"kind":"artifact","path":"docs/design/BPR-1079-feedback-ux-v1.html","detail":"Rules every candidate keeps: conversation counting unit (root, replies never count), Other as catchall remainder with the partition computed in the frames (fixture src/cache.cpp: Open 2/1/4 of 7, All 4/1/5 of 10), Not on a file remainder, four resolution states, explicit Comment/Approve/Request changes with no automatic verdict, public label text versus private defect badge and uncounted drafts."}},{"criterion":"BPR-1079:tradeoffs","outcome":"pass","revision":"59f65ad9e","evidence":{"kind":"artifact","path":"docs/design/BPR-1079-feedback-ux-v1.html","detail":"Tradeoff tables for Overview A/B, authoring A/B/C and preflight A/B; the record and every frame say nothing is implemented and the two-letter marks are placeholders for BPR-1080."}},{"criterion":"BPR-1079:owner-selection","outcome":"pass","revision":"6793935cd","evidence":{"kind":"freeform","path":"docs/design/BPR-1079-feedback-ux-selected.html","detail":"Owner verdicts in attended sessions ca54b0e8e5bc8a46f40cee31ea0619fd (v1: Overview B, authoring A+C, preflight B, states and settings Accept), 4742c47e636d407949d455abdd8030a3 (v2: shortcodes and tints Accept), 830ffc2a626c879b180a04e3a4e9d802 (v3: threads A and D, combine) and 3a17a6eb6b995ba6414161d6d158b315 (v4: threads-v004 Accept, no note; decision at sequence 4, handed-back at 5). Consolidated in the selected design record. Static frames, not native evidence; R33 belongs to the built surfaces."}}]
---

# BPR-1079: Develop compact authoring, counter and resolution UX candidates

Pack alias: **PACK-CC-05** (live `BPR-1079`, allocated at import).

## Outcome and work

Inspect the current tour-enabled workbench before proposing changes. Produce a small set of concrete candidates covering optional authoring, native review preflight, Settings and Overview with zero/typical/all label columns. Include unknown coverage and labelled replies. Do not prescribe icons from the chat.

## Acceptance

- Candidates demonstrate actual narrow/wide constraints, long content and full twelve-column overflow behavior.
- Each candidate explains counting units, catchall remainder, native review choices and public/local ownership.
- Tradeoffs are recorded without claiming a mockup is implemented.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Candidates, round v1 (2026-10-05)

Before drawing anything, the current surfaces were inspected at `7ff4126ca` through
offscreen renders from `qt_pull_request_overview_page` (`BPR_QT_RENDER_PATH`),
`qt_review_submission_editor_scale_100` and `qt_review_draft_editor`, and through
the workbench spec. The Overview Comments cell shows an amber unresolved bubble and
a grey total bubble. Compose review chooses the disposition in a combo box and has
no preflight. The inline page has Publish and Discard over plain text. Comment's
arrow menu is reserved for labels and offers none.

The [design record](../../design/BPR-1079-feedback-ux-v1.html) sets out the
rules every candidate keeps: counting unit, Other as the catchall, the
Not on a file remainder, four resolution states, labels versus states,
identification, drill-down, public versus local, native review and suggestions.
It also records the candidates, their tradeoffs and the open items.
[Frames](../../design/BPR-1079-feedback-ux-frames-v1.html) are drawn at 1440×820
and at 960×700–760, with Settings at its 760×560 minimum, in light, and three of
them in dark:

- Overview counts: **A** a column per label, against **B** label chips in one
  Comments column. Each is shown with no columns, issue + question and all
  twelve. Twelve columns at 960 px overflow with File pinned (A) or fold into
  "+n" (B).
- Open/All switch, the ? coverage chip, the state bar in All, and the Label key (⌥K).
- Authoring: **A** a label chip with a menu, **B** an icon strip, **C** typing
  with completion. Each has a labelled reply under a differently labelled root
  and a long body with a native suggestion.
- Thread cards: square label marks, round state pills, a mark on each reply,
  a plain tour stop, applicable and fallback suggestions, and the private
  defect badge.
- Review preflight: **A** a list beside the summary, against **B** one chip
  with its list on demand. Both have three visible dispositions.
- Settings → Reviews & queue → Comment labels.

The counts are computed in the frame from conversation units. HairStrandsLighting.cpp
equals the pack fixture `src/cache.cpp`: Open gives 2/1/4 of 7 and All gives
4/1/5 of 10. Label marks are two-letter placeholders, and the icons are BPR-1080's.
Limitations: these are static HTML frames rendered with headless Chrome. They
are not the product, and they are not native, scaled, high-contrast or
assistive-technology evidence (R33 applies to the built surfaces). Nothing is
implemented.

## Round v1 verdicts (2026-10-05)

Session `ca54b0e8e5bc8a46f40cee31ea0619fd`, read with `status --after 18`
(sequences 19 to 25). The owner's notes are quoted verbatim.

| Item | Verdict | Note |
|---|---|---|
| `BPR-1079-authoring-v001` | Selected **A: label chip and menu** and **C: type it, with completion** | "I like type it as a shortcut but it needs something more distinct like \`:qu:\` maybe treat them like typing an emojis that we replace." |
| `BPR-1079-overview-columns-v001` | Selected **B: label chips in one Comments column** | (none) |
| `BPR-1079-overview-states-v001` | **Accept** | (none) |
| `BPR-1079-preflight-v001` | Selected **B: one chip, list on demand** | (none) |
| `BPR-1079-settings-v001` | **Accept** (sequence 25 supersedes the note-less Accept at 23) | "I think we should tint the marks so they are more recognizable at a glance without always having to read the tiny letters. Tints should be configurable in settings appearance." |
| `BPR-1079-threads-v001` | **Reject** | "Looks like its doing both the mark and the full word still. It's designed to save space so it should be just the mark, mark can have a tooltip with the whole word. Also the marks look like they are lower than the text." |

## Round v2 (2026-10-05)

[Design record v2](../../design/BPR-1079-feedback-ux-v2.html) and
[frames v2](../../design/BPR-1079-feedback-ux-frames-v2.html), at candidate
`48b588d25`. Only the chosen options are left: Overview B, authoring A, and
preflight B. Round v1's files stay as history.

- **Threads v2.** The card header has the root's mark, the line and the state
  pill. A reply's mark sits in its name row. The word and its decoration
  appear only in the tooltip and the accessible name, for example "issue
  (blocking)". Round v1's reply marks sat 3 px low because of
  `vertical-align:-3px`. In v2 every mark is centred in a flex row. With
  `?check=align` the frames measure each mark's centre against its text: the
  largest gap is 0.008 px in th-wide (6 marks), th-narrow (6) and th-detail
  (3), in light and dark, on the frozen source.
- **Tints.** Each mark has a soft fill and a border in its label's colour.
  The letters stay ink, and no default uses the amber of open or the green of
  resolved. Following RFC-BPR-0034, the tints are twelve colour roles ("Label
  issue" and so on) in a fifth group, Comment labels, on Appearance ›
  Colours. That makes 45 offered roles 57. Each theme variant has its own
  built-in tints, listed in the record. An edit changes only the theme on
  screen and previews through the existing bar. A stored palette without
  these roles reads them as the built-in tints. Reviews & queue › Comment
  labels gets a "Mark colours" link, and "Overview columns" becomes
  "Overview chips".
- **Shortcodes.** A label's code is its mark's two letters between colons:
  `:pr: :ni: :su: :is: :td: :qu: :th: :ch: :no: :ty: :po: :qb:`. Codes work
  only at the start of the comment. A colon and one letter open the
  completion list, which matches codes before names. The closing colon, Tab
  or Enter replaces the code in one undoable step, and ⌘Z puts it back.
  A code replaces an existing label. Esc, an unknown code or pasted text
  leaves the text as it is. Codes belong to the label helper, and the
  preview shows plain label text.

Limitations: these are static HTML frames rendered with headless Chrome. They
are not native, scaled, high-contrast or assistive-technology evidence.
Nothing is implemented.

## Round v2 verdicts (2026-10-05)

Session `4742c47e636d407949d455abdd8030a3`, read with `status --after 9`
(sequences 10 to 12, `handed-back` at 13). The owner's note is quoted verbatim;
its line break is shown as " / ".

| Item | Verdict | Note |
|---|---|---|
| `BPR-1079-shortcodes-v002` | **Accept** | (none) |
| `BPR-1079-tints-v002` | **Accept** | (none) |
| `BPR-1079-threads-v002` | **Reject** | "It's weird that its positioned differently for the thread opening comment. / Maybe it belongs consistently to the left of the comment below the author icon? Or consistently to the left of the author name? Or consistently in line with the comment so it makes sense when you edit it? I dunno try a couple options, surprise me with one. Let me pick. But i dont like this currently non-consistency." |

Shortcodes and tints are settled as round v2 drew them.

## Round v3 (2026-10-05)

[Design record v3](../../design/BPR-1079-feedback-ux-v3.html) and
[frames v3](../../design/BPR-1079-feedback-ux-frames-v3.html), at candidate
`275e74b82`. Only the thread cards are redrawn. In every option each post,
root and reply alike, places its mark the same way, and the card header holds
only the line (or tour stop) and the state pill:

- **A, under the avatar.** The mark sits below the author's avatar, in its column.
- **B, left of the name.** The mark leads the name row.
- **C, start of the text.** The mark is the first thing in the comment, where the
  label is. While editing, the same mark stands in the same place for the words
  "question: ", so the label shows once. The composition editor would follow.
- **D, label rail.** This is the surprise option. Each post gets a thin left
  rule. A labelled post's rule takes the label's tint, and the mark heads it,
  level with the name. Plain posts keep a grey rule.

Every option is drawn at 1440×820 and 960×760, in light and dark, with root
plus replies. Each has two edit states: my reply being edited and my own
thread's first comment being edited. A comparison frame puts all four side by
side. In A, B and D the label shows twice while editing: as the mark, and as
the highlighted words at the start of the field. Tints, tooltips and
accessible names are unchanged from round v2.
`?check=1` measures each mark against its anchor. Across 34 renders of the
frozen source (17 frames, light and dark) the root-to-reply spread is 0 px
in both directions for every option, including posts being edited, and no
card header holds a mark. v3 also fixes v2's narrow frames, where long code
pushed the tray partly off screen.

Limitations: these are static HTML frames rendered with headless Chrome. They
are not native, scaled, high-contrast or assistive-technology evidence.
Editing is drawn in place on the card. Where an edit opens is not decided
here. Nothing is implemented.

## Round v3 verdict (2026-10-05)

Session `830ffc2a626c879b180a04e3a4e9d802`, read with `status --after 3`
(decision at sequence 4, `handed-back` at 5). The owner's note is quoted verbatim.

| Item | Verdict | Note |
|---|---|---|
| `BPR-1079-threads-v003` | Selected **A: under the avatar** and **D: label rail** | "I like A but it's right aligned and needs to be centered. I like the colored rail but it takes too much space with the label ontop so combine A and D. Horizontal space is a premium. Also I don't think there's enough emphasis put on the blocker attribute now." |

## Round v4 (2026-10-05)

[Design record v4](../../design/BPR-1079-feedback-ux-v4.html) and
[frames v4](../../design/BPR-1079-feedback-ux-frames-v4.html), at candidate
`6793935cd`. Only the thread cards are redrawn, and in one way, A and D
combined:

- **Mark.** It sits under the avatar, centred, on every post. Measured from
  the ink of the round v3 renders, v3's 15 px mark under the 18 px avatar sat
  2 px in on the left and 1 px on the right. The v4 mark is 16 px, which
  leaves 1 px on each side at 1× and 2 px at 2×.
- **Rail.** A rule in the label's tint runs down the post's left edge, with no
  mark on it. Plain posts get a grey rule. The rail uses space the card
  already had: the card's 9 px padding for the root and the 24 px indent for a
  reply. Its right edge is always 3 px from the avatar. Measured at the 380 px
  tray, plain A and v4 are the same: root text starts 35 px from the card edge
  and is 335 px wide, and reply text starts at 59 px and is 311 px wide. v3's D
  cost about 21 px a post.
- **Blocking.** The mark is filled with its tint, its letters white or
  near-black (lowest contrast 4.52:1 across the twelve tints in four theme
  variants), and the rail is 4 px instead of 2 px. "(non-blocking)" looks like
  an undecorated label. The mark is an image named "issue (blocking)", and the
  post is an article named "tbutler, issue (blocking)". Both were read from
  Chrome's accessibility tree.
- **Frames.** 1440×820 and 960×760, in light and dark, reading and editing (my
  reply; my own blocking first comment), with two blocking posts, one
  non-blocking post and two plain posts. A detail frame sets plain A beside v4,
  shows every label normal and blocking, and shows the avatar column at 3×.

Limitations: these are static HTML frames rendered with headless Chrome. They
are not native, scaled, high-contrast or assistive-technology evidence; the
accessibility names come from Chrome's tree for the frame, not from the Qt
surface. Nothing is implemented.

## Round v4 verdict (2026-10-05)

Session `3a17a6eb6b995ba6414161d6d158b315`, read with `status --after 3`
(decision at sequence 4, `handed-back` at 5, nothing pending). The owner gave no
note.

| Item | Verdict | Note |
|---|---|---|
| `BPR-1079-threads-v004` | **Accept** | (none) |

## Selected design

Every item now has an owner verdict. The
[selected design](../../design/BPR-1079-feedback-ux-selected.html) consolidates
them in one place:

- Overview B: label chips in one Comments column.
- Open/All states, the ? chip and the Label key, as accepted.
- Authoring A, the chip and its ⌘L menu, plus round v2's `:shortcodes:`.
- Preflight B: one chip, with the list on demand.
- Settings › Reviews & queue › Comment labels, as accepted.
- Round v2's tints, configurable on Appearance › Colours.
- Round v4's thread cards: a 16 px mark centred under the avatar, a tinted
  rail, and blocking shown by a filled mark with a 4 px rail.

BPR-1080 replaces the two-letter placeholders with icons. BPR-1081 records the
implementation contract. Native evidence (R33) belongs to the built surfaces.
