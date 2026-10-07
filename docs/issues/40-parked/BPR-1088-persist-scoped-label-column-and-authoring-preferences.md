---
schema: "implement-rfc.card.v1"
id: "BPR-1088"
title: "Persist scoped label-column and authoring preferences"
status: "Parked"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0049"]
depends_on: ["BPR-1082", "BPR-1087"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-14","milestone":"overview","acceptance":["R20","R28","R41"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"R20","outcome":"pending","revision":"4a07f26bc","evidence":{"kind":"observation","detail":"Built: Reviews & queue > Comment labels > Overview chips selects and orders any subset of the twelve labels (drag, Alt+Up/Down), independent of the label helper and marks; core_feedback_label_columns, sqlite_workbench_settings_storage (all twelve persist in order; repeats/unknowns refused, stored bad lists load kept-and-counted with a recovery notice) and qt_feedback_label_settings pass on bpr-main-linux. Pending: the owner look at the section (offscreen renders only; frontend-design skill not installed on this station)."}},{"criterion":"R28","outcome":"pass","revision":"4a07f26bc","evidence":{"kind":"command","detail":"application_workbench_preferences_consumers test_label_preferences_follow_the_profile_authority: one durable write reaches both windows of a profile, a second profile is untouched both ways, failed writes never accepted and the asking window told, cancellation claims nothing, scoped reset writes only the label defaults; qt_feedback_label_settings Reset scoped with the drawer filter kept. Failing-before controls recorded on the card. 14/14 focused rows re-verified on bpr-main-linux at 1fd74b34c."}},{"criterion":"R41","outcome":"pass","revision":"4a07f26bc","evidence":{"kind":"command","detail":"sqlite_workbench_settings_storage migrates a v12 profile with every other field off default (letters, hooks, window row) to v13 with helper off, Open, no chips and every other field equal; two windows editing at once settle on the accepted record and a mid-flight change-back ends on the last choice (host and window fixes, controls fail). application_label_authoring_contract passes with the helper reaching the editor only through the setting."}}]
---

# BPR-1088: Persist scoped label-column and authoring preferences

Pack alias: **PACK-CC-14** (live `BPR-1088`, allocated at import).

## Outcome and work

Extend the existing typed profile settings and migration path for ordered label columns and convention/count defaults. Keep active view state in its existing owner. Preserve the unrelated Comments drawer filter, other settings and custom hooks; handle failed saves and concurrent windows.

## Acceptance

- New fields absent in old settings adopt safe defaults without resetting anything else.
- All twelve column choices persist; duplicate/unknown IDs fail safely and explain recovery.
- Reset, cancellation, cross-window update and profile isolation follow the existing authority contract.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Built and verified on Linux (bpr-main-linux, 2026-10-06)

Product commit `4a07f26bc` (rebased from `429e2cb15` onto `1c652c97c`,
which changed no source). Everything rides the existing typed profile
authority: `review_behaviour_settings`, the profile's
`workbench_preferences_host`, the window's `workbench_application_host`,
the workbench SQLite store and Reviews & queue. No new settings mechanism.

**Typed fields** (`workbench_runtime_ports.hpp`):

| Field | Type | Default |
|---|---|---|
| `LabelHelper` | `review_label_helper` (off, on) | off |
| `OverviewCounts` | `review_overview_counts` (open, all) | open (`feedback_resolution_mode::unresolved`) |
| `LabelColumns` | `core::feedback_label_columns`, an ordered subset of the twelve, fixed size | none |
| `LabelColumnsRecovery` | optional `{ Unknown, Repeated }` | absent; a load observation, never stored, cleared by `edited()` on every edit |

`valid()` refuses an out-of-range helper or count and any column list that
`core::check_feedback_label_columns` refuses (repeated, unknown, too many,
non-canonical padding), so no store ever writes one. The active Overview
mode stays the view's (BPR-1089); only the starting default is here.

**Core** (`feedback_label_columns.hpp/.cpp`): `make_feedback_label_columns`
refuses a repeat or unknown label rather than dropping it;
`feedback_label_columns_text` stores stable IDs and commas (all twelve are
84 bytes); `read_feedback_label_columns` keeps every exact catalog ID once,
in order, and counts unknown, empty and repeated entries, bounded at 256
bytes and saturating at 255.

**Store** (`workbench_settings_database`): schema **13**. The profile row
gains `label_helper` (0), `overview_counts` (0) and `label_columns` ('',
at most 256 bytes); `MaximumTableColumns` 23 to 24; the version-12 step
carries `feedback_marks`. A stored list with an unknown or repeated ID loads
without it and reports it; the row is not rewritten until the reviewer saves.
Out-of-bounds storage (an over-long list, a helper or count of 2) fails
closed at admission like every other column.

**Settings** (Reviews & queue › Comment labels, the design's order):
Label helper (switch; info glyph "The label chip and :shortcodes: while
writing. Off: write labels yourself; they still count."), Label marks
(unchanged), Overview counts (Open or All; info glyph), Mark colours (link
"Appearance › Colours ›", tooltip "Opens Appearance with the filter set to
Label", opens Appearance with the colour filter set to "label"), Overview
chips (`qt::feedback_label_columns_editor`: ticked rows first with a grip,
a rule, the rest in catalog order; check box, mark, ID and, at 340 px or
wider, the description; drag or Alt+Up/Alt+Down to reorder; anchored row
tooltips, "Drag to reorder (⌥↑ ⌥↓)" on ticked rows). Reset keeps the page's
"Reset" button, gains the tooltip "Reset comment labels" and resets these
four choices only. A recovered list shows "Some saved chips were left out:
… The rest keep their order. Change a chip or Reset to save this list."

**Helper switch.** The window hands the comment editor the published
`LabelHelper` (`apply_label_helper`, only on a published change), and the
drawer's in-place reply takes it from the editor as before. The tests-on
`--label-helper` and the label-authoring probe now write the profile's
preference through `workbench_preferences_host` (once per profile)
instead of reaching into the editor; Qt tests still call
`set_label_helper` directly.

**Found and fixed while building it.** A gesture that returned to the
published record while the same window's earlier edit was in flight was
dropped twice: by the window (compared with the published record) and by
the host (`Layout.Behaviour == aSettings`), so the in-flight edit won. The
window now compares with the record it last asked for, and the host sends
such a record after the pending one.

### Acceptance

- **Absent fields adopt safe defaults without resetting anything else.**
  `sqlite_workbench_settings_storage`
  (`test_label_preferences_migrate_from_twelve_without_resetting_anything_else`):
  a version-12 row with every other field off its default, letters, writing
  hooks and a window row migrates to 13 with helper off, Open, no chips and
  no report, and compares equal on every other field; the window row is
  kept. Pass.
- **All twelve column choices persist; duplicate or unknown IDs fail safely
  and explain recovery.** `core_feedback_label_columns` (every label alone,
  every rotation and length, all twelve reversed, repeats and unknowns
  refused when built, read back kept-and-counted, bounds) and
  `sqlite_workbench_settings_storage` (`test_all_twelve_label_columns_persist_in_order`:
  each label alone and all twelve out of catalog order round-trip whole and
  are stored as IDs; a typed repeat or unknown is refused with the row
  untouched; a cancelled write changes nothing.
  `test_unknown_or_repeated_stored_columns_load_safely_and_explain`:
  `issue,blocker,question,issue,,note` loads as issue, question, note with
  two unknown and one repeat reported, every other field kept, the stored
  text unchanged until the next save, which stores `issue,question,note`).
  Qt shows the explanation (`test_a_recovered_list_is_explained`). Pass.
- **Reset, cancellation, cross-window update and profile isolation follow
  the existing authority contract.** `application_workbench_preferences_consumers`
  (`test_label_preferences_follow_the_profile_authority`): one durable write
  fans out to both windows of the profile; a second profile is untouched in
  both directions; a repeated column is refused and the held record
  republished; a failed write is never accepted and the asking window is
  told; a cancelled write claims nothing and the owner reloads; a scoped
  reset writes the label defaults only; two windows editing at once settle
  on the accepted record; going back mid-flight ends on the last choice.
  `qt_feedback_label_settings`: rows, defaults and words; the switch reaches
  the editor only once published; every row reverts to a republished
  record; ticking, Alt+Up/Down and unticking; all twelve in order; Reset
  scoped to the section with the Comments drawer filter carried; Mark
  colours; the minimum size. Composed: `application_label_authoring_contract`
  passes with the helper reaching the editor only through the preference.
  Pass.
- **Owner look: OPEN.** Built from the page's existing row language on Linux
  with offscreen renders only; the frontend-design skill is not installed on
  this station. R33's native matrix is not run.

### Failing-before controls

Each mutation was applied, built, run and reverted.

| Mutation | Fails at |
|---|---|
| Reading keeps repeats (`core`) | "a repeated ID is left out once, the first place kept, and counted" |
| Version-12 step drops `feedback_marks` | "every other field, the mark style and the writing hooks included, is kept exactly" |
| A bad stored list fails the load | "and the row still loads" |
| Columns not written | "and is kept as its stable ID" |
| `edited()` keeps the load report | "one durable write, with no load observation carried into it" |
| Host drops a return to the held record | "going back to the held record while the first edit is in flight is written after it, so the reviewer's last choice wins" |
| Window does not apply the published helper | "the published choice turns the editor's helper on" |
| Reset keeps the chips | "Reset dispatches the label defaults and keeps every other preference, the drawer filter included" |
| Window compares with the published record | "unticking before the first tick is answered is still sent, though it equals the published record" |
| Recovery line never shown | "the Overview chips row says what was left out" |
| Mark colours sets no filter | "with its colour filter set to label" |
| `--label-helper` writes no preference | `application_label_authoring_contract`: `LABEL_AUTHORING_FAILED`, `phase=4 body=:qu: draft=:qu: helper=0` |

### Regressions and gates

Linux x86_64 (WSL2), GCC 15, Debug `-Werror`, `build/dev` whole tree and
`build/core-only`:

- 32 of 32: `core_feedback_*`, `application_workbench_host`,
  `application_workbench_preferences_consumers`,
  `sqlite_{workbench,appearance}_settings_storage`, `qt_settings_window` and
  its three scales, `qt_settings_effect_boundary`, `qt_feedback_label_*`,
  `qt_appearance_settings_panel`, `qt_workbench_window_scale_{100,125,200}`,
  `qt_composed_github_scale_{100,125,200}`,
  `application_review_behaviour_contract`,
  `application_label_authoring_contract` and
  `application_review_round_trip_composition_contract`.
- `core-only`: the `core_feedback_*` rows, `core_dependency_boundary` and
  `core_boundary_contract`, 11 of 11.
- `architecture_boundaries`, `architecture_boundary_contract` (346 s) and
  `test_network_isolation_manifest`.
- Optimized `app-release` with `BUILD_TESTING=ON` (`-O3 -Werror`): the whole
  tree built (only the vendored SQLite warns); the seven new or touched rows
  pass there; the tree was deleted.
- `coding-format-check` with `/usr/bin/clang-format` 22.1.8, `git diff
  --check`, `DOCUMENTATION_OK`. `tidy.py` finds nothing in the new
  translation units, the store, both hosts and the two extended tests.
- **Pins.** Full-application and production-manifest pins resealed over
  `8a9e280a4` with no intervening pinned-scope commit; the change is inside
  `github_application.cpp`'s tests-on blocks. The mutation-branch pin did not
  move (`e067befe…a5df`).

### Limits

- **Concurrent windows** keep the existing whole-record contract: two
  windows editing at once are written in turn and both settle on the later
  record; a field changed in one window and not yet published can be
  replaced by the other window's record. No per-field merge is added.
- **Overview counts** has no round state mark beside Open yet; BPR-1089
  draws the state marks and renders the chips this card stores.
- **Platforms.** Linux only; macOS and Windows runs are owed. No live GitHub.
- **Page design** is BPR-1288's; this card used the page's existing rows.

## macOS run (coordinator, 2026-10-06)

At 122e5d6b7 on macOS arm64 Debug with BPR_BUILD_TOUR_CLI=ON, clean worktree:
22/22 of the tour owner channel, tour CLI composition/process/public entry, both
skill package rows, both handoff courses, cycle security qualification, cycle
CLI public path and documentation, the round-trip contract, the settings window
and workbench settings rows, workbench settings storage, the feedback label rows
and architecture_boundaries.
