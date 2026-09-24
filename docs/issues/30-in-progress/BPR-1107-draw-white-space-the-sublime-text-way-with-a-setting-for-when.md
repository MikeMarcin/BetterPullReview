---
schema: "implement-rfc.card.v1"
id: "BPR-1107"
title: "Draw white space the Sublime Text way, dots for spaces and lines for tabs, with a setting for when"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"origin":"owner rejection of attended candidate BPR-0986-rendered-panes-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision 6b7243c3-f194-4396-a3b9-0f8309dabfcf, 2026-09-24","origin_rfc":"RFC-BPR-0034","related_rfcs":["RFC-BPR-0006","RFC-BPR-0018"],"related_cards":["BPR-0986","BPR-0790","BPR-0791"],"owner_reference":"docs/verification/BPR-1107-white-space-drawing/owner-sublime-reference-2026-09-24.png","reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json"}
---

# BPR-1107: Draw white space the Sublime Text way, dots for spaces and lines for tabs, with a setting for when

## Why

[[BPR-0986]] made the review panes paint the Appearance preview's tab markers
on every leading tab (`55e9405f`). On 2026-09-24 the owner rejected that
rendering in attended session `cbd48b91000b00f1e72e1ad62c7c384c` (packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
candidate `BPR-0986-rendered-panes-v001`, revision `55e9405f`, fingerprint
`5b87471b…`, decision `6b7243c3-f194-4396-a3b9-0f8309dabfcf`, sequence 580,
19:12:17Z, `actor: human`): **reject**, with this note, verbatim:

> Should follow the sublime text model with dots and lines and have settings to control white space. We don't need to support all their settings but at least easy and impactful ones.

The rest of the note is Sublime Text's own documentation of its
`draw_white_space` setting, pasted verbatim (tabs as the owner pasted them;
trailing blank lines omitted):

```text
	// Controls when white space is drawn. Any of the following options may be
	// combined:
	// - "selection": Draw white space under the current selection.
	// - "leading": Draw any white space between the beginning of a line and the
	//              first character.
	// - "enclosed": Draw white space enclosed by other characters.
	// - "trailing": Draw white space following the last character on a line.
	// - "isolated": Draw white space on lines containing no other characters.
	// - "all": All of the above, ie. always draw white space.
	//
	// These options may be further refined by appending any of the following
	// separated by an underscore:
	// - "none": Don't draw this kind of white space.
	// - "tabs": Only draw tabs here.
	// - "spaces": Only draw spaces here.
	// - "mixed": Only draw white space that does not match the indentation
	//            style. For example if "translate_tabs_to_spaces" is true only
	//            draw tabs.
	// - "mixed_tabs": Like "mixed" but only draw tabs.
	// - "mixed_spaces": Like "mixed" but only draw spaces.
	// - "all": Draw both tabs and spaces. This is the default.
	//
	// Note that options are applied in sequence. So a later option may override
	// an earlier one.
	//
	// Examples:
	// - ["selection", "trailing", "isolated"]:
	//     Draw white space at the end of any lines and under the selection.
	//
	// - ["all_tabs", "selection"]:
	//     Draw tabs anywhere and any white space under the selection.
	//
	// - ["all_mixed"]:
	//     Draw any white space that does not match the indentation style.
	//
	// - ["leading_mixed", "isolated_mixed"]:
	//     Draw any indentation that does not match the indentation style.
	//
	// - ["selection_mixed_tabs"]:
	//     Draw only tabs under the selection and only if the indentation style
	//     is spaces.
	//
	// - ["all_tabs", "selection"]:
	//     Draw all tabs and any white space under the selection.
	//
	// - ["all", "selection_none"]:
	//     Inverse of the default. Draw white space everywhere except under the
	//     selection.
	"draw_white_space": ["selection"],
```

He attached a Sublime Text window as the reference, kept here cropped to its
text (the full 3648x2216 original, SHA-256
`192322df1f0049d75f671ddaddc0ebd4c597ddfd0b164953fbaf805339e40616`, stays in
the session's decision record):

![Sublime Text drawing spaces as dots and tabs as lines](../../verification/BPR-1107-white-space-drawing/owner-sublime-reference-2026-09-24.png)

In it every space, leading, enclosed or trailing, is a small centred dot. Each
tab is a thin horizontal line running from where the tab starts to its tab
stop, so a tab that begins mid-stop is shorter ("Another tab, smaller because
of spacing"). Faint vertical guides mark the indentation stops. The editor's
status bar reads "Tab Size: 4".

## Scope

- **Draw.** In the review panes (both panes and one pane), draw a space as a
  dot and a tab as a line to its tab stop, in the white-space colour, at the
  pane's own tab width. The Appearance page's preview draws the same thing
  through the same painter. RFC-BPR-0034 C4 makes the preview "the review
  window's painter in miniature", and the shared helper stays the only
  renderer. It replaces `paint_tab_markers`' current leading-tab markers.
- **When.** A setting chooses when white space is drawn. At least the easy and
  impactful choices from the owner's list: none, selection, leading,
  trailing, and all. Consider enclosed, isolated and the tabs-only,
  spaces-only and mixed refinements, and include the ones that are cheap. Say
  which are left out and why. The default is proposed with the design and
  confirmed by the owner.
- **Where the setting lives.** On the Appearance page's Type card beside Tab
  width, as one more one-line row with an info glyph, in RFC-BPR-0034's row
  language. It is persisted with the typography, per profile, through the
  existing appearance store. A schema change is a migration with a test.
- **The colour.** Reuse the role the markers use today
  (`code_line_number_text`), unless the design shows it fails on some theme.
  A new role is an RFC-BPR-0006 decision, and this card does not take it.
- **Line identity is untouched.** Drawing white space changes no text, no
  selection, no copy and no line mapping.

## Acceptance

- AC1 A design page shows the drawing and the setting on the four themes, with
  the owner's reference beside it, rendered and inspected headless.
- AC2 The review panes and the preview draw spaces as dots and tabs as lines to
  their stop, through one shared painter, for each setting value offered.
  `qt_diff_workbench_scale_100/125/200`, `qt_code_sample_painter`,
  `qt_type_preview` and `qt_theme_preview_strip` pin it, and each fails with
  the drawing switched off.
- AC3 The setting is a live control on the Appearance page and persists.
  `qt_appearance_settings_panel`, `qt_appearance_profile`,
  `sqlite_appearance_settings_storage` and the appearance end-to-end contracts
  pass.
- AC4 The owner accepts the rendered drawing and the setting in an attended
  session: one question for the drawing and one for the setting's choices and
  default.

## Boundaries

The core decides, the host performs, Qt presents. The setting is appearance
state owned by the appearance domain. The painter only reads it. No
stylesheet and no hard-coded palette. [[BPR-0986]] keeps the current-line band
and the two hunk-header roles; this card owns white space.
