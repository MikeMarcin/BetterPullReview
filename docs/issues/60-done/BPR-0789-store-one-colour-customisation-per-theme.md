---
schema: "implement-rfc.card.v1"
id: "BPR-0789"
title: "Store one colour customisation per theme"
status: "Done"
rfcs: ["RFC-BPR-0034"]
depends_on: ["BPR-0770"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"appearance","acceptance":["C1"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 - Prior-schema fixtures with a stored light or dark palette migrate with nothing lost and the other variants render built-ins","outcome":"pass","revision":"867e4bedee24b4519384a0fddc4654ed348b5acf","evidence":{"kind":"command","detail":"sqlite_appearance_settings_storage passes on this revision (scratchpad d6-int-ctest-focused-03.log, 7/7 with the full focused appearance selector). test_version_six_files_one_palette_under_its_recorded_variant runs one case per variant and compares the WHOLE migrated record to the original, so the light direction and the dark direction are each proven rather than one standing in for the other; the fixture's mode stays high_contrast_dark in every case, so a migration deriving the variant from the mode instead of reading palette_variant fails three of four. Every other variant arrives as exactly appearance_palette_customisation{} - no customisation - and the profile therefore renders that variant's built-in palette. test_version_six_without_a_stored_palette_carries_no_customisation covers palette_stored = 0. LIMITATION: the bytes of a v6 palette_document whose palette_stored = 0 are discarded; that document has been dead data since the version-4 one-time reset and was never rendered, so no customisation is lost, but the bytes do not survive."}},{"criterion":"AC2 - Committing on one variant leaves the others' customisations intact across reopen; export and import name the variant they act on","outcome":"pass","revision":"867e4bedee24b4519384a0fddc4654ed348b5acf","evidence":{"kind":"command","detail":"application_appearance_host and qt_appearance_profile pass. test_an_adopted_palette_records_the_variant_it_customises commits Dark then Light and asserts both survive, still different, including from a second host lifetime that re-reads the written record - the reopen claim; under the previous single-palette record the second commit erased the first. test_a_palette_reset_names_one_variant_and_spares_the_others and test_editing_a_colour_keeps_every_other_theme cover reset and edit isolation, the latter being the inverted form of a fixture that previously asserted the Light override WAS discarded. Export and import act on the theme on screen: the composition passes palette_for(variant) and the page names that theme in its wording and in the Colours card title. The real binary agrees - the theme-customization probe emitted STORED_ADOPTION_FACTS:111111111, which flips the observation to another variant and requires the committed theme's slot to still hold the candidate (scratchpad d6-int-ctest-e2e.log). LIMITATION: the host's export publication carries a palette rather than a variant, so the theme it acted on is named by the composition and the page, not by the publication itself; recorded as a non-blocking follow-up candidate."}},{"criterion":"AC3 - Future or malformed schema versions fail closed at the appearance store's scope; no widget reads or writes the store","outcome":"pass","revision":"867e4bedee24b4519384a0fddc4654ed348b5acf","evidence":{"kind":"command","detail":"test_a_newer_schema_is_refused_for_the_right_reason and test_legacy_refusal_preserves_schema_and_rows pass: establish_schema refuses *current > appearance_settings_schema_version with failure::schema_mismatch before any write, verifies PRAGMA application_id before altering a stamped file, admits the legacy row through the same reader that refuses a tampered document, and performs the whole v7 rebuild inside one BEGIN IMMEDIATE with ROLLBACK on every failure path, so a file is wholly v6 or wholly v7. Layering holds: no widget gained store access - the store is reached only through the appearance settings port from the application host, qt_settings_effect_boundary passes, and both places where Qt previously derived a variant were removed (appearance_profile's defaulted stored variant and theme_preview_strip's constructor assumption), with the panel's convenience adoption now using the composition-supplied variant. An independent rfc-reviewer-critical pass over the named risk of irreversible palette loss or misattribution returned no blocking findings."}}]
---

# BPR-0789: Store one colour customisation per theme

## Current frontier — BPR-0789 accepted; `appearance` continues with BPR-0790, bpr-shell-surfaces-20260915 d6 / rfc-integrator-d6 / mandate r3

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `appearance`: obligations `BPR-0789:*` and
  `BPR-0790:*`, both exits. `milestone_plan.py inspect` at `8e756fe6` recomputed definition
  `sha256:a7502cbb…`, obligation `sha256:a3e0bd11…` and prerequisites `sha256:0b907a25…`, identical to the
  dispatch assignment. This card closes `BPR-0789:*` and milestone exit 1; the milestone stays open.
- **Candidate.** Local `main`, nothing pushed: promote `e6b43a7e`, claim `fe233453`, source `867e4bed`, and
  this record in the finish commit.
- **Usable behaviour.** Each theme keeps its own colours. Tuning Dark no longer forgets Light: the durable
  record holds one `{palette, stored}` slot per variant, a commit or reset changes only the theme on screen,
  and the other three survive a restart. Overrides still apply only while their variant is resolved, so a
  light customisation cannot colour the dark profile. Export writes, and import previews into, the theme on
  screen and the page says which theme that is. A pre-existing customisation migrates to the variant its
  record named. Per-variant customised counts are published, ready for BPR-0790's tile captions.
- **Verification.** Under Evidence: a full clean-tree link, the focused appearance selector 7/7, the composed
  Settings selector 5/5, migration proven per direction with a restart, the documentation gate, and an
  independent critical review with no blocking findings. The three end-to-end appearance contracts do not
  reach OK, for reasons proven independent of this change.
- **Carried forward.**
  - `BPR-0790` owns milestone exit 2 and, per BPR-0775's Decisions, **Dif's Appearance controls** (drift D1,
    mandate r2): Show Dif mascot, Motion and the scoped `appearance_reset_scope::dif` reset must stay
    reachable on the rebuilt page, re-expressed in its row language, with the reconciliation recorded there.
  - The theme-tile captions are not painted yet: the counts are published and exposed, and C2 is BPR-0790's.
  - Storage documentation for the schema bump belongs to `BPR-0780`; no documentation is edited here.
  - Follow-up candidate, not blocking: the host's export publication carries a palette rather than a variant,
    so the theme it acted on is named by the composition and the page, not by the publication itself.
- **Next.** Promote and claim `BPR-0790`, then rebuild the Appearance page. Its known hard problem is
  recorded on that card: RFC-BPR-0034 C4 wants the type preview painted by "the review window's own painter
  in miniature, not a second renderer", but the real painter is `source_pane`, file-local inside
  `diff_workbench.cpp` and dependent on an accepted diff result, while `theme_preview_strip::paint_sample` is
  already a second renderer, and nothing in the tree paints `code_current_line_background` or a tab marker at
  all.

## Outcome

Give the appearance settings record one stored palette per variant; bump the appearance settings store schema with a transactional, restart-tested migration that files the existing single palette under its recorded variant; key the Qt appearance profile's override table by variant; scope commit, reset, import and export intents to the resolved variant supplied by the composition; publish per-variant customised counts in the projection.

## Acceptance

- Prior-schema fixtures with a stored light or dark palette migrate with nothing lost and the other variants render built-ins.
- Committing on one variant leaves the others' customisations intact across reopen; export and import name the variant they act on.
- Future or malformed schema versions fail closed at the appearance store's scope; no widget reads or writes the store.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Verification — d6, source revision `867e4bed`

Logs are in the dispatch scratchpad under `d6-int-*` (integrator) and `d6-0789-*` (the implementing worker).

| Check | Direct surface and command | Result |
|---|---|---|
| Full link before any claim | `cmake --build --preset dev`, then `ninja -n` | Exit 0, 177/177, then 0 pending actions. The worker's own run had **167 actions outstanding**, so every result below was re-earned after a complete link. This also retires the earlier caveat about a run linking against an archive of uncommitted objects: this link is of a committed, clean tree (`d6-int-build-01/02/03.log`) |
| Focused appearance surface | `ctest --preset dev -R '^(core_appearance_policy\|core_appearance_contrast\|application_appearance_host\|application_appearance_profile_consumers\|sqlite_appearance_settings_storage\|qt_appearance_settings_panel\|qt_appearance_profile)$'` | **7/7 pass** (`d6-int-ctest-focused-03.log`) |
| Composed Settings, chosen by composition | `ctest --preset dev -R '^(qt_settings_window(_scale_(125\|150\|200))?\|qt_settings_effect_boundary)$'` | **5/5 pass** (`d6-int-ctest-settings-03.log`). Selected by composition, not subsystem, per the board's recorded selector trap: these build the real Settings window that owns the panel |
| Migration, both directions, nothing lost | `sqlite_appearance_settings_storage`: `test_version_six_files_one_palette_under_its_recorded_variant` (one case per variant), `test_version_six_without_a_stored_palette_carries_no_customisation`, `test_version_six_refuses_a_malformed_legacy_value` | Pass. A v6 row's palette lands under exactly the variant that row recorded — the mode stays `high_contrast_dark` in all four cases, so a reader deriving the variant from the mode fails three of four — every other variant arrives with no customisation at all, and the whole record is compared, not just the palette |
| Restart | Reopen assertions inside the same courses (`the migrated record survives a close and reopen unchanged`), plus a second host lifetime reading the written record in `test_an_adopted_palette_records_the_variant_it_customises` | Pass. Committing Dark then Light leaves both stored and different after reopen; under the previous single-palette record the second commit erased the first |
| Fail closed | `test_a_newer_schema_is_refused_for_the_right_reason`, `test_legacy_refusal_preserves_schema_and_rows`, and `establish_schema`'s `*current > version` guard plus its `PRAGMA application_id` check | Pass. A future version and a foreign application id are refused before any write; a refused legacy file keeps its bytes |
| Dif preferences survive the bump | `distinct_settings()` carries `{hidden, full}` **in the two version-6 cases only** | Pass. Added on a reviewer finding: the fixture had left Dif defaulted, so a migration that re-ran the version-6 step (which rewrites those columns as shown/System) would have gone unnoticed. Placing it in the pre-version-6 cases instead is wrong and was caught: those versions have no such columns, so `down_edit_to_version` drops them and the value cannot be carried |
| Three end-to-end appearance contracts | `ctest --preset dev -R '^(application_appearance_gesture_contract\|application_appearance_store_fallback_contract\|application_appearance_theme_customization_contract)$'` | **All three fail; not accepted as green** (`d6-int-ctest-e2e.log`, 350 s). Every appearance probe fact string from the real binary is all-ones, including the new per-variant `STORED_ADOPTION_FACTS:111111111` and `EQUAL_COMMIT_FACTS:111111111`. The courses stop at `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` (x2), `EPOCH_FIXTURE_SEED_FAILED` (x1, phase one, before any gesture) and `PUBLIC_PATH_FAILED` (x2) |
| Discriminator for those failures | `ctest --preset dev -R '^application_diff_workbench_public_path$'` — the diff workbench through the real composition with **no appearance probe at all** | Fails on the same binary with the identical `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` and a timeout at 60 s (`d6-int-ctest-diffwb.log`). So that stage is independently red in this sandbox and the contracts' failures there are not this change. `EPOCH_FIXTURE_SEED_FAILED` is the fixture-seed class already recorded on BPR-0775 |
| Documentation | `python3 scripts/verify_documentation.py --root .`; `python3 tests/scripts/verify_documentation_tests.py` | `DOCUMENTATION_OK`; 64 tests OK (`d6-int-docgate.log`, `d6-int-doctests.log`) |
| Format | `cmake --build --preset dev --target coding-format-check` | The gate is red, and **none of this change's twenty files is flagged** (`d6-int-formatcheck-03.log`). Four files it touched were clean at HEAD and had become dirty (`appearance_profile.hpp`, `appearance_settings_database.cpp`, `github_application.cpp`, `appearance_settings_storage_tests.cpp`) and were formatted. The seventeen that remain are untouched here and were already dirty at HEAD — `workbench_window.hpp` alone has 110 violations — so they are pre-existing and deliberately not swept into this commit |
| Independent critical review | `rfc-reviewer-critical` against the named risk of irreversible palette loss or misattribution, over the uncommitted diff | **No blocking findings.** All eight hunted counterexamples resolved safe with anchors: filing by recorded variant; the rebuild wholly inside one transaction with rollback on every path and no stamp-before-carry; both cross-version directions fail closed; per-variant isolation through the deferred reset ingress and consumer fan-out; both Qt guess sites gone. Its two non-blocking findings were acted on (the Dif fixture gap, and the panel's residual derivation) |

Limitations: no native, rendered, VoiceOver or attended evidence is claimed here; offscreen runs are layout
evidence only. The three end-to-end contracts do not reach their OK markers, for the independent reason
above, so the real-process restart claim rests on their appearance stages plus the in-process reopen courses
rather than on a green contract. Two `better_pull_review` probe processes outlived their ctest run and were
killed, leaving `build/dev/tests/end_to_end/appearance-theme-customization-root` and
`…/appearance-store-fallback-contract` dirty. The seven sandbox-blocked native courses recorded on BPR-0775
were not re-run, and `architecture_boundaries` still stops on the unrelated `.claude/worktrees` checkout.

### Decisions — d6, mandate r3

- **One authority per theme, not two facts.** The record holds four `{Palette, Stored}` slots indexed by
  variant, replacing `Palette` + `PaletteStored` + `PaletteVariant`. Keeping the trio and adding a table
  would have stored the same fact twice, which is the defect class BPR-0216 and BPR-0645 exist to record.
  `Stored` stays per slot because it is what distinguishes "this theme has no customisation" from "this theme
  was customised to its built-in colours".
- **The variant is scoped at the host API, not carried on the intent.** RFC-BPR-0034 requires the resolved
  variant to be "supplied by the composition, never guessed in Qt", so `apply`/`reset` take it and the
  composition reads `AppearanceProfile->variant()` once, inside the gesture's own turn. Had Qt stamped it onto
  an intent, a widget holding a stale variant could have re-targeted a commit — the exact failure the RFC
  prohibits.
- **A version-6 file skips the version-6 step.** That step rewrites `dif_visibility` and `motion_mode` as
  shown/System. Before this change the version constant was 6, so a version-6 file returned early and never
  re-ran it; with the constant at 7 it no longer would have, and re-running it would have silently reset a
  preference the record held. `establish_schema` therefore runs it only `if (*current < 6)`.
- **An unstored version-6 palette does not survive, and the claim says so.** Its `palette_document` has been
  dead data since the version-4 one-time reset and was never rendered, so no *customisation* is lost — but
  the bytes are discarded, and this card claims no more than that.
- **Both Qt derivations are gone.** The profile no longer defaults the stored variant, `theme_preview_strip`
  no longer assumes light in its constructor, and the panel's convenience adoption now shows the variant the
  composition supplied rather than one derived with `core::resolve_appearance`. Light remains a stated default
  for a caller that named no theme; it consults no mode and no observation and never reaches the record.
- **Counts are computed in presentation.** A count of roles differing from the built-in palette needs the
  built-in palettes, which are the adapter's; the host holds none and the core holds no default theme. The
  composition publishes the four numbers to the page.
- **Dif's Appearance controls are BPR-0790's**, per BPR-0775's Decisions and drift D1. Nothing about them is
  changed here beyond the schema carrying them intact.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
