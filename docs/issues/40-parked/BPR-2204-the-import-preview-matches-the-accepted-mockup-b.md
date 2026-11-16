---
schema: "implement-rfc.card.v1"
id: "BPR-2204"
title: "The import preview matches the accepted mockup B"
status: "Parked"
parked_on: "human-review"
rfcs: ["RFC-BPR-0112"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2204--BPR-2204--bd14773b7352.json","design":"docs/design/workflow-import-v1/README.md","follows":"BPR-2032","accepted_look":"workflow-import-v003","widened":"Export Profile Settings in the same language (coordinator, 2026-11-15)","built":"7d036ba93"}
depends_on: ["BPR-2032"]
---

# BPR-2204: The import preview matches the accepted mockup B

## Scope

The owner accepted the import preview in look `workflow-import-v003`
(session `5052733e41fd0662f09a0e04af730e4c`, decision `09e243f2`) with the
note, verbatim: "Still not quite as nice as what had settled on but its
pretty close." His two screenshots set the mockup B in Dark
(`docs/design/workflow-import-v1/png/b-dark.png`) beside the build at
`431c119b2`. This card closes the visible gaps between them, within the
Qt rules: theme roles only, the Prompts page's widgets, nothing that pans
sideways. Nothing here changes what the core decides.

## Gaps, mockup against build

The list:

1. **Tags.** The mockup draws each standing as a small tinted pill (New in
   blue, Updated in teal, Unchanged in grey, Required in grey); the build
   writes plain words after the name, in the list and in the detail title.
2. **Second line of a row.** In the mockup "Added as a new agent", "363f80
   replaces yours, 9b1e44" are in the secondary text colour; in the build
   they are the name's colour.
3. **Glyphs.** The mockup marks an agent row with the agent sparkle and a
   refused row with a red prohibited mark; the build has neither.
4. **Indent.** In the mockup a row with no tick (Unchanged, Cannot be
   imported) lines its text up with the ticked rows' names; in the build it
   starts at the left edge.
5. **Section headers.** The mockup's are in the body colour, semibold, with
   even air above each; the build's are smaller, in the placeholder grey.
6. **Selection.** The mockup selects with a quiet grey fill; the build uses
   the system highlight blue.
7. **List ground.** The mockup's list sits on the panel colour; the build's
   is the darker Base, a step darker than everything beside it.

The detail:

8. **Frame.** The mockup's detail sits on the sheet's own ground; the build
   frames it as a panel and then frames the comparison inside it again (a
   double border).
9. **Scroll position.** Selecting another item keeps the previous item's
   scroll offset, so the new item opens part-scrolled (the build screenshot
   shows the choice row cut off at the top). Each item should open at its
   top.
10. **The workflow's choice.** The mockup's workflow detail has no Update /
    Leave out control (the tick is the choice); the build shows one. Either
    keep it and say so, or follow the mockup; ask the owner if unsure.
11. **Revisions.** The mockup sets the two revisions in the origin sentence
    in bold; the build does not.

The comparison:

12. **Table.** The mockup is a table: a header band ("Yours, revision
    9b1e44" / "In the file, revision 363f80") in small semibold secondary
    type, a hairline between rows and a hairline between the columns. The
    build has no rules, and its headers are large and bold.
13. **Row alignment.** In the mockup each action's two cells share one row
    height; in the build a tag on one side ("Does more", "New action") adds
    a line there only, so the sides drift out of line.
14. **Weights.** The mockup's action names are regular weight at body size;
    the build's are bold.
15. **Secondary lines.** "You run …" and the effects line are secondary
    text in the mockup; primary in the build.
16. **New action.** The mockup tints the whole new cell and puts "New
    action" as a pill at the cell's top right; the build tints only the tag,
    which wraps onto its own line.
17. **Widened effect.** The mockup marks only the added effect ("changes a
    ticket") in the caution colour, with no "Does more" tag; the build tints
    it with the highlight colour and adds a "Does more" tag that wraps onto
    its own line.
18. **Required.** A grey pill in the mockup; plain grey words in the build.

The rest:

19. **Program card.** The mockup's is one filled, rounded card with tight
    padding, "synthetic_org_companion.py is not chosen yet" on one line,
    "Four actions ask for it" in words and Choose program… at the top right;
    the build's is a bordered square with more padding and a digit.
20. **Header.** The mockup keeps the title and its sentence close and wraps
    the sentence short of the full width; the build leaves more space and
    runs the sentence across the sheet.
21. **Footer.** The mockup reads "Adds 1 agent and updates 1 workflow." and
    wraps short of the buttons; the build reads "Import adds 1 agent,
    updates 1 workflow." on one long line.
22. **Type size.** The build's body text in the detail reads a size larger
    than the mockup's beside the same title; match the Prompts page's body
    and secondary sizes.

## Verification

`qt_agent_configuration_dialogs` keeps its theme check (the panel is the
Prompts page's in Dark, Light and both high-contrast themes) and its
no-sideways-pan check; add checks for the row alignment (13) and the
scroll reset (9). Native Cocoa renders in Dark and Light, compared with
`png/b-dark.png` and `png/b-light.png`, then one owner Accept/Reject.

## Built and parked (2026-11-15)

Built at `7d036ba93` on `claude/v0-5-0-prerelease-targets-52265d`. Scope
widened by the coordinator to Export Profile Settings, which has no mockup
and is drawn in mockup B's language. All 22 gaps are closed; the shared
parts are `src/adapters/qt/src/profile_settings_parts.{hpp,cpp}`. Every
colour is a palette role, the agent accent, or a theme role
(`state_unread` New, `state_verified` Updated/Edited/New action,
`state_attention` the added effect and Not saved, `state_defect` the refused
mark) of the theme the dialog inherits, made readable with
`readable_signal_ink`; the palette inheritance of `431c119b2` is kept.

Gap 10: the mockup is followed. A workflow's tick is its choice, and its
detail has no Update / Leave out. Agents keep their Add / Use / Leave out
control (mockup B draws radio buttons there; not in this card's list).

Export, as designed here: the title close over a sentence that wraps short;
the five sections as the import list's rows in one rounded well (section
rows semibold with their line, items indented so their tick sits under the
section's name, agents with the sparkle, Edited and Not saved as tinted
pills, the unsaved prompt dimmed with no tick, a section's tick shown
mixed from its ticked items); Choose every prompt under the list; at the
right the Label field, "The file" with its warning line, and the exact text
in its own well; the footer "Puts 2 agents, 1 prompt, and 2 repositories in
the file, 1.61 KiB." beside Cancel and Export….

Verification at `7d036ba93`: `qt_agent_configuration_dialogs` (new checks:
the two sides of a row level (13), another item opens at its top (9), tick
as a workflow's choice, the added effect in the caution colour without Does
more, New action tinting its cell on the task's line, the refused mark, the
list on the Prompts page's panel and the detail on the window by role and
by pixels in Dark, Light and both high-contrast themes, export's wells on
the panel in all four), `qt_profiles_settings_panel`,
`application_review_round_trip_composition_contract` (app pin resealed for
the look's `--attended-profiles-look` chaining) and `architecture_boundaries`
pass; the dialogs test passes offscreen at 100 and 200 percent and natively
in Dark and Light; `verify_documentation` passes.

Owner look `profile-settings-dialogs-v001`: session
`b27beb61d941c378e9fa1b2e0fd9dbdb`, state
`/Users/jaeger/.attended-review-state/profile-settings-dialogs-v001-2026-11-15/state`
(URL in that directory's `serve.log`), cursor 9, host PID 54853 (Tailscale
child 54863). Frozen root `/Users/jaeger/profile-settings-dialogs-v001.vsd_9gwm`
from `7d036ba93` (executable SHA-256 `3b8e2863d470…`); its check reached
`ATTENDED_PROFILES_SHOWN:profiles=2:page=profiles:share=shown` with the
workflow look fixture imported and the Fixture agent only. Two questions,
Accept or Reject each: `BPR-2204-import-preview` and `BPR-2204-export`, with
native Dark and Light renders beside mockup B.
