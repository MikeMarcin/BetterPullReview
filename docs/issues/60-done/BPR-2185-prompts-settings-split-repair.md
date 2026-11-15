---
schema: "implement-rfc.card.v1"
id: "BPR-2185"
title: "Prompts settings: the core decides, the host saves, Qt presents"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2179"]
priority: "P1"
platforms: ["mac","linux","win"]
acceptance: [{"criterion":"N1-N6","outcome":"pass","revision":"798613f0f","evidence":{"kind":"command","detail":"core_prompt_editor, core_prompt_surfaces, core_prompt_macro, core_quick_ask, application_prompt_macro_host, sqlite_agent_setup_storage, qt_prompts_page, application_prompt_macros_public_path, architecture_boundaries, format, verify_documentation"}}]
metadata: {"reservation":"docs/issues/_reservations/BPR-2185--BPR-2185--8bf721b447aa.json","origin":"critical architecture review of Prompts settings (BPR-2179) at 03aae05cb, relayed by the coordinator 2026-11-14"}
---

# BPR-2185: Prompts settings: the core decides, the host saves, Qt presents

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md),
after [[BPR-2179]] (Done). A critical architecture review of `03aae05cb`
found that Settings › Prompts breaks the split "the deterministic core
decides; the imperative host performs; the Qt presentation adapter presents".
BPR-2179's evidence (`qt_prompts_page`, `core_prompt_surfaces`,
`application_prompt_macros_public_path` and the looks) missed every item
below: no course failed a save, wrote from another window, undid an on/off,
filled the library, or read a nested condition through the page.

## Scope

- **N1:** the save carries the revision its set was derived from; stale and
  failed outcomes are decided in core, not in the widget. A failed write keeps
  the edit; a projection that arrives while edits wait cannot be overwritten
  under its revision. A `prompt_macro_host` course.
- **N2:** the editing model, transitions, undo/redo and identity move into a
  core reducer over the library, surfaces and layers; the host mints a new
  prompt's identity. Undo is scoped to the layer Prompts for shows; on/off,
  placement, Reset, Restore built-ins and Use app-wide are undoable and clear
  Redo; `maximum_prompt_macros` is checked.
- **N3:** Use app-wide conditions, Has conditions, the facts Add condition
  offers, the problem line and the thread-activity reason read core
  decisions.
- **N4:** one core function makes the full differences, repositories included.
- **N5:** a Hide prompt at a wider scope with no match is hidden; None of an
  unknown fact does not hold; the condition index is the leaf's place in the
  tree whatever the prompt does while unmet; nested group count is bounded.
- **N6:** the store's conditions column is wide enough for core's limits,
  with a migration.

## Acceptance

Each item has its course; `architecture_boundaries`, format and the
documentation gate are green. Nothing the owner sees changes except that
undo and redo cover every edit, so no look.

## Closeout (2026-11-14)

Built at `798613f0f`; the verification record has the detail
([RFC-BPR-0128](../../verification/RFC-BPR-0128.md), "Correction: Prompts
settings split").

- **N1:** `core::settle_prompt_save` and `adopt_prompt_projection` decide a
  save's outcome; `prompt_macro_host::save` writes from the revision it is
  given. Courses `core_prompt_editor` (a failed write keeps the edit; a write
  after another window's names the older revision and is refused as stale)
  and the new `application_prompt_macro_host`.
- **N2:** `bpr/core/prompt_editor.hpp`; the page holds a `core::prompt_editor`.
  Undo covers On, placement, Reset, Restore built-ins and Use app-wide, each
  clearing Redo, and starts again when Prompts for changes layer; the
  library's bound is refused with a reason; `prompt_macro_host::mint_identity`
  supplies identities.
- **N3:** `prompt_fields_differing`, `prompt_has_conditions`,
  `prompt_condition_room_of`, `prompt_placement_problems` (shown in words) and
  `prompt_unmet_reading_of`.
- **N4:** `core::prompt_macro_differences_of(shipped, document)`.
- **N5:** `core_prompt_surfaces` `test_review_n5`.
- **N6:** `sqlite_agent_setup_storage` widens an earlier build's table with
  its rows and stores a tree longer than 8192 bytes.

Visible to the owner: Undo and Redo cover every edit; at 64 prompts New
prompt and Duplicate say the library is full; after a failed write the
editor reads "Not saved"; a problem line is in words. No look was asked for.
