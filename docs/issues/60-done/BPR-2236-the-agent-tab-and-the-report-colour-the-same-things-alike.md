---
schema: "implement-rfc.card.v1"
id: "BPR-2236"
title: "The Agent tab and the report colour the same things alike"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2236--BPR-2238--777d083d72cc.json","origin":"BPR-2219's look review-report-v002, 2026-11-16"}
---

# The Agent tab and the report colour the same things alike

## Origin

The owner, 2026-11-16, on look review-report-v002: "it looks like we have colors in the agent tab and the report which correspond to the same elements but use different colors." And: "I think we could also tone major down a tiny bit in the process." The report page takes its three verdict colours from the theme's report roles ([[BPR-2219]]); the Agent tab's finding cards, severity and label marks, verdict text, suggested-comment blocks and report row take theirs elsewhere. Since round 2 of [[BPR-2219]] the page no longer reads the report paper, ink and code wash roles, which Settings › Appearance › Reports still lists. Not started: [[BPR-2219]]'s worker ran out of room for it.

## Scope and acceptance

- **AC-a.** Every coloured element that appears in both the Agent tab and the report page (a severity or label mark such as Major or issue (blocking), a verdict, a finding's rule or border, the counts, a suggested change) takes its colour from one shared role, decided in core, not from a constant of its surface.
- **AC-b.** Settings › Appearance lists only roles that are read, and a change to one shows on both surfaces. The report paper, ink and code wash roles are removed.
- **AC-c.** Major is a small step less intense than today, still plainly apart from Minor and readable in light and dark.
- **AC-d.** A side-by-side render of the matching elements, light and dark, with Major before and after, goes to the owner with one Accept or Reject question.

## Audit (2026-11-16)

Where each element shown on both surfaces took its colour before this card:

| Element | Agent tab | Report page |
|---|---|---|
| A finding's severity word (Major, Minor) and its card's edge | constants in `agent_tab_view.cpp` (`severity_colour`: Major `#CF222E` / dark `#F85149`, Minor `#C38A10` / `#E3B341`) | — (the page names the finding by its label) |
| A finding's label (issue (blocking), suggestion) and its rule | the suggested comment's label in placeholder grey | `report_verdict_*` by `core::report_tone_of` (open `#B2392A` / dark `#E5A093`) |
| The verdict | plain bold Markdown in the summary ("**Verdict:** Needs changes") | a mark filled with `report_verdict_*` |
| Counts (tally) | — | `report_verdict_*` |
| A suggested change | the tab's own code box | the same code box (already shared) |
| The page's paper, ink, code wash | — | the view's own palette since [[BPR-2219]] round 2; the `report_paper`, `report_ink`, `report_code_wash` roles were still listed in Settings › Appearance and read by nothing |

## Built

- **One role per element, decided in core.** `core::report_tone_of(defect_severity)`
  joins `report_tone_of(label)`: critical and major need work, minor is half
  way, trivial is neutral. The Agent tab's card edge, severity word and its
  suggested comment's label, and the report page's labels, rules, verdict and
  counts all wear `tone_colour(tone, tone_colours)`, where `tone_colours` is
  read once from the profile (`tone_colours_of`) by both surfaces.
  `severity_colour`'s constants are gone.
- **The verdict in the tab.** `core::report_card` now carries `Verdict` and
  `VerdictTone` and its `Summary` no longer opens with the verdict line; the
  tab shows the verdict over the summary as the page does, "Verdict" quiet and
  the verdict filled with its tone (`agentTurnReportVerdict`).
- **Appearance.** `report_paper`, `report_ink` and `report_code_wash` are out
  of the vocabulary (68 roles, 66 offered). A stored palette or theme that
  still carries one is read past (`core::retired_appearance_role`), not
  refused. The group is "Findings and reports": Needs work (Major), Half done
  or asked (Minor), Done or good. The three tones are held to the text floor
  on the code canvas, which is both surfaces' ground; the page's derived
  shades are measured against `code_background` and `syntax_normal`.
- **Major toned down.** Needs work is `#A64538` by day (was the slate's
  `#B2392A`; the tab's was `#CF222E`), 5.9:1 on white, and `#DCA295` by night
  (was `#E5A093`; the tab's `#F85149`), 8.7:1 on `#0D1117`: same hue and
  lightness, less chroma, 30 degrees of hue from Minor. High contrast keeps
  its values.
- **Tests.** `qt_agent_tone_colours` (new): one review in the tab and on its
  page; Major's severity and its issue (blocking), Minor's and its
  suggestion, and the verdict wear the profile's role on both, and a
  customised tone shows on both; the built-in Needs work is quieter than
  either old red, apart from Half done and at the text floor. `core_agent_report`
  (severity tones, the card's verdict), `core_theme_exchange` (retired roles
  read past), `core_appearance_contrast`, role-count courses, and
  `application_review_report_public_path` (the tab's verdict over its summary).

## Owner look (filed, not waited on)

Attended-review session `3b22e7868c33624badafbfc8a436baf6`, external state
directory `~/.attended-review-state/tones-v001-2026-11-16/` (`state/` the
session, `manifest.json`, `serve.log`, `serve.pid`), served on the tailnet
since 2026-11-16. One Accept/Reject question, `tones-alike`, on the frozen
build `tones-v001` (root `~/tones-v001.u6chy2i4`, frozen from
`87e489594672`, Fixture agent only, fixture `returning-review-v1` with
`src/engine/renderer.cpp` open and its next push): one colour per element on
both surfaces, Major a step quieter, light and dark. Its images: before and
after, side by side, light and dark, drawn by `qt_agent_tone_colours`
(`BPR_TONE_RENDER_ROOT`, Cocoa native look) at `87e489594` and, for before,
at `3dd6195f9` in a scratch export. The freeze's own check passed whole,
first head, next push and the Fixture agent's 17 asks. Lifecycle: pending.

Filed: [[BPR-2239]] (the freeze tool's own check exiting at the next-push
start on looks review-report-v002 and v003; not reproduced here).

## Checkpoint (2026-11-16)

- Commits on `claude/v0-5-0-prerelease-targets-52265d`: `3dd6195f9` (In
  Progress), `555ba0abf` (the change), `87e489594` (render mode). The lane is
  shared with BPR-2234's worker, whose commits and uncommitted edits sit
  beside these.
- Passing at `555ba0abf`/`87e489594` (dev): `core_agent_report`,
  `core_appearance_contrast`, `core_appearance_policy`, `core_theme_exchange`,
  `sqlite_appearance_settings_storage`, `qt_agent_tone_colours`,
  `qt_agent_report_view`, `qt_agent_finding_cards_retained`,
  `qt_agent_finding_cards_restart`, `qt_appearance_role_display`,
  `qt_colour_role_list`, `qt_appearance_settings_panel`,
  `qt_appearance_profile`, `qt_theme_preview_strip`,
  `application_review_report_public_path`,
  `application_fixture_agent_every_ask`, the appearance end-to-end contracts
  (theme customization, older theme, gesture, store fallback, native probe,
  profile consumers, host), `architecture_boundaries`, documentation gate.
- Failing in the same tree, in paths this card did not touch:
  `qt_composed_github_scale_100/125/200` ("the prepared loading overview does
  not manufacture accepted content"), `qt_agent_launch_canary` ("hostile
  partial reads"), `qt_agent_launch_canary_durable_course`. Not compared with
  the base.

## Verdict, look tones-v001 (handed back 2026-11-16)

- **Accept** `tones-alike` (decision `565a9c58`): "If we could pick a red
  like halfway between those 2 it would be better. I don't need a
  rereview." Then, through the coordinator: "only dark needs to change."
- **Done from the note.** Dark Needs work is `#EC7F6E`, halfway in OKLCH
  (lightness, chroma and hue each averaged) between the Agent tab's old dark
  Major `#F85149` and the look's `#DCA295`: 7.0:1 on `#0D1117`, hue 30
  against Minor's 85. Light Needs work stays `#A64538`. `qt_agent_tone_colours`
  pins both; the older-theme contracts expect the new dark built-in.
  Passing: `qt_agent_tone_colours`, `qt_appearance_profile`,
  `core_appearance_contrast`,
  `application_appearance_theme_customization_contract`,
  `application_appearance_older_theme_contract`. No new look, as the owner
  asked.
- AC-a to AC-c are met by the change above; AC-d by look tones-v001 and its
  accept. Look tones-v001's host is stopped and its root removed.
