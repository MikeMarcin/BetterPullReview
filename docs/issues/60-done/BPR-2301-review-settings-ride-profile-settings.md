---
schema: "implement-rfc.card.v1"
id: "BPR-2301"
title: "Settings › Review rides Profile Settings as an optional Review section"
status: "Done"
rfcs: []
depends_on: ["BPR-2283", "BPR-2299"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2301--BPR-2301--9b6865de7ded.json","origin":"owner, 2026-11-19, relayed by the coordinator: \"I think the review settings should be in exported bprsettings as an optional group. Yes?\" (coordinator answered yes)","related_cards":["BPR-2283","BPR-2299"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"]}
---

# BPR-2301: Settings › Review rides Profile Settings as an optional Review section

## Asked

Owner, 2026-11-19: "I think the review settings should be in exported
bprsettings as an optional group. Yes?" The coordinator answered yes. It
settles [[BPR-2299]]'s open question: the Profile Settings file had no
section for any Review setting.

## What travels

The Review section carries every preference Settings › Review sets for the
profile (`application::review_behaviour_settings`), each as a named member:

| Member | Settings row | Values |
| --- | --- | --- |
| `open_in` | A pull request opens in | `overview`, `review`, `last_used` |
| `mark_read` | Mark a file read | `by_hand`, `when_fully_seen` |
| `return_comparison` | On return, compare | `whole_pr`, `since_last_review`, `since_last_upload` |
| `comments_filter` | Comments drawer opens on | `all`, `unread`, `unresolved` |
| `drawer_controls` | Drawer controls | `hidden`, `shown` |
| `feedback_marks` | Label marks | `icons`, `letters` |
| `label_helper` | Feedback guidance | `off`, `on` |
| `overview_counts` | Overview counts | `open`, `all` |
| `line_details` | Line details | `shown`, `hidden` |
| `markdown_images` | Images | `load`, `off` |
| `reading_width` | Reading width | `narrow`, `medium`, `wide`, `full` |
| `threads_width`, `discuss_width`, `reports_width` | Reading width › Advanced | `reading_width`, `full`, or pixels |
| `label_columns` | Overview chips | label IDs, in order |
| `center_ribbon_width` | Center ribbon width | pixels |
| `finish_comment`, `finish_approve`, `finish_request_changes` | Close and go Home after | `home`, `pull_request` |

Left out, because they belong to one window or one computer, or are not a
preference: drawer widths and states, the Status drawer, window geometry,
the Line details height, the summary share, remembered view filters, Discuss
bot marks, and `LabelColumnsRecovery` (what a load found wrong with the
stored label columns).

## Format

Version 4 of `bpr.profile-settings` adds the optional `review` member. A
version 3 file, which has none, still reads; a version 3 file that has one is
malformed; version 5 is refused as newer. The section is read member by
member: a member or value this version does not know is left out and named
(`decoded_review::Ignored`); a surface width is held to 720..3840 as a stored
one is (`reading_override_of`), the label list keeps its known labels once
each, both named (`Adjusted`); the center ribbon is held to 16..256 by the
application. Nothing in the section refuses the file; only a `review` that is
not a JSON object makes it malformed. A section that sets nothing usable is
listed "Cannot be imported".

## Design

- Core (`agent_configuration.hpp`): `profile_settings_section::review`
  (ticked by default), `review_setting`, `review_choice_names`,
  `portable_review`, `decoded_review`, `plan_review_import`; encoder and
  lenient reader.
- Application (`agent_configuration_exchange.hpp`): `portable_review_of`,
  `with_review`, `review_differences`; the application's enums are
  `static_assert`ed against the core's names; `agent_configuration_preview::Review`.
- Qt: Export lists a Review row; Import lists "Review settings" with Updated,
  Unchanged or Cannot be imported, and its page shows each change as "yours →
  the file's" with what was held or left out. The `frontend-design` skill
  was loaded; the page follows the dialog's existing Theme and Identity pages.
- Composition: Export reads the profile's accepted Behaviour; Import plans
  against it and applies the chosen record through
  `workbench_preferences_host::apply_behaviour`, which every window follows.

## Acceptance

- **AC1.** The Review section round-trips every listed setting.
  `core_agent_configuration` (`test_review_section`).
- **AC2.** A file without Review (and a version 3 file) imports with none.
  `core_agent_configuration` (`test_one_format`, `test_review_section`),
  `qt_agent_configuration_dialogs`.
- **AC3.** Out-of-range and unknown values: a width below 720 held to 720,
  above 3840 to 3840, the ribbon to its bounds, unknown presets and members
  left out and named, the file never refused. `core_agent_configuration`,
  `sqlite_agent_configuration_storage`.
- **AC4.** Export → import through the real workbench settings stores makes
  the recipient's Settings › Review the sender's and reads back.
  `sqlite_agent_configuration_storage` (`review_settings_travel`).
- **AC5.** The Export and Import dialogs show the group: ticked by default,
  untick leaves it out; Import lists it Updated, Unchanged or Cannot be
  imported, shows each change, and Leave out imports nothing.
  `qt_agent_configuration_dialogs` (`test_review_section`).
- **AC6.** The composed application's Export Profile Settings… ticks Review
  and writes it. `application_profile_settings_export_public_path`.

## Evidence

macOS arm64, dev (Debug), offscreen; full dev build passed.

| Criterion | Check | Result |
| --- | --- | --- |
| AC1–AC3 | `core_agent_configuration` (dev and core-only) | Passed |
| AC3, AC4 | `sqlite_agent_configuration_storage` | Passed |
| AC5 | `qt_agent_configuration_dialogs` | Passed |
| AC6 | `application_profile_settings_export_public_path` | Passed |
| Regression | `core_prompt_pack`, `core_workflow`, `sqlite_prompt_pack_storage`, `qt_window_file_drop`, `application_window_file_drop_public_path`, `application_appearance_theme_customization_contract`, `application_appearance_older_theme_contract`, `application_cpp_identifier_appearance`, `application_appearance_host`, `qt_appearance_settings_panel`, `qt_profiles_settings_panel`, `sqlite_workbench_settings_storage`, `core_workbench_interaction`, `architecture_boundaries` | 18 of 18 passed |

Format (clang-format 22) on the touched C++ files; clang-tidy on the touched
translation units reports nothing in the changed lines (the storage test's
older snake_case locals are on main unchanged).
