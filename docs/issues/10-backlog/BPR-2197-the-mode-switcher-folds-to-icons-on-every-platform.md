---
schema: "implement-rfc.card.v1"
id: "BPR-2197"
title: "The mode switcher folds to icons on every platform"
status: "Backlog"
rfcs: ["RFC-BPR-0112"]
metadata: {"design":"docs/design/windows-title-fit-v2/README.md","design_branch":"claude/windows-title-fit-mockups","design_revision":"615bd65c8","design_session":"20c49e523db3f8c071bda18a1ee8d2bd","earlier_design_session":"1073f45be455a420999e60fd5e502ee3","owner_decision":{"date":"2026-11-15","quote":"I think all 3 platforms should do the same icon on narrow trick."},"reservation":"docs/issues/_reservations/BPR-2197--BPR-2197--36dc5cc18504.json"}
depends_on: ["BPR-2196"]
---

# BPR-2197: The mode switcher folds to icons on every platform

## Scope and acceptance

The owner's accepted design `windows-title-fit-v2`
(`docs/design/windows-title-fit-v2/README.md` on
`claude/windows-title-fit-mockups` at `615bd65c8`; attended review
`windows-title-fit-v002`, session `20c49e523db3f8c071bda18a1ee8d2bd`, state
`~/.attended-review-state/windows-title-fit-v002-2026-11-15/`, Accepted
2026-11-15 with no note). It follows round 1
(`docs/design/windows-title-fit-v1/README.md`, session
`1073f45be455a420999e60fd5e502ee3`), where the owner chose icons that open:
"I like icons when it gets narrow because we already do that for the drawer
tabs", then "Okay nvm the ^ on windows" and "I think all 3 platforms should
do the same icon on narrow trick."

Build it after [[BPR-2196]] lands: both change the same switcher. The owner
has since renamed the fifth mode from Revise to **Author**. The design draws
it as Revise; build it as Author. Its pixel figures were measured with the
word Revise and are a guide, not a contract.

- **Faces, widest first.** Names and keys; names; icons with the open mode
  named; icons alone. The first two are what `modality_control` shows today.
  The last two are the drawer tabs' narrow faces (`fit_tray_tabs`:
  `face::open_named`, `face::glyphs`). No mode shows an icon while every name
  is showing, as in the drawers since [[BPR-1755]].
- **Which face.** The widest beside which the whole name fits: repository,
  number and title. When none does, the open mode keeps its name for as long
  as a title of the row's minimum (`identity_title_minimum`) still fits beside
  it; the last face is the icons alone. So the other modes' names yield before
  the title is cut, and the open mode's name yields before the title is lost.
  No face returns at a narrower width.
- **Icons.** Existing glyphs only, in the tabs' size and stroke: Overview
  `conversation_glyph::rendered`, Review `conversation_glyph::source`, Discuss
  `conversation_glyph::comment`, Threads `conversation_glyph::finder`, Author
  `strip_glyph::edit_file`. The open mode's glyph is drawn in the
  highlighted-text ink, since it sits on the accent.
- **Key hints.** Each platform keeps its own in the widest face: Command-1 to
  Command-5 on macOS, Ctrl+1 to Ctrl+5 on Windows and Linux
  (`QKeySequence::NativeText`, as today). No caret form.
- **Tooltips and names.** A mode showing only its glyph says its name first
  in its tooltip, as a folded tab does; the tooltip keeps the key. Accessible
  names keep the whole word in every face.
- **The same on macOS, Windows and Linux**, with four modes as with five.
- **What goes.** The three-letter prefixes (`set_abbreviated`); the
  Windows-only threshold that drops the keys below 1300 ([[BPR-1324]]); and
  the rule that shows the title only if it would also fit beside the keyed
  switcher ([[BPR-1251]], `IdentityTitleBudget`). The fold decides all three
  from the room.
- **One fold helper.** The face choice is one shared helper used by the
  tray's tabs (`fit_tray_tabs`), Overview's drawer tabs and the switcher. It
  must not become a third copy.
- **Architecture:** presentation only. Qt presents; nothing here is decided
  in the core or performed by the host.

Load `frontend-design:frontend-design` before the Qt work and say so in the
report.

## Intended verification

A Qt course over the switcher at falling widths with a long and a short
title, four modes and five: the face at each width, that no face returns at
a narrower width, that the title is cut only after the other modes' names
have gone, each platform's key hint, the tooltip of a glyph-only mode and
the accessible names. The drawer tab courses pass unchanged on the shared
helper. Native renders on each platform against the design's sheets
(`docs/design/windows-title-fit-v2/png/`).
