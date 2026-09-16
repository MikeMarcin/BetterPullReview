---
schema: "implement-rfc.card.v1"
id: "BPR-0791"
title: "Verify and close RFC-BPR-0034"
status: "Done"
rfcs: ["RFC-BPR-0034"]
depends_on: ["BPR-0779", "BPR-0780"]
closeout_for: ["RFC-BPR-0034"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["C1","C2","C3","C4","C5","C6"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"All C1-C6 rows reference direct evidence or a specific unresolved criterion","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"observation","detail":"Every C row carries a direct check or a named unresolved criterion. C6 and the attended halves of C4 and C5 now carry the owner's verdicts of 2026-09-16, bound to executable sha256 d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b from source ed2b3f14: bpr-0779-appearance-rendered (c5349059), bpr-0779-voiceover-appearance-tiles (6825fbc6), bpr-0779-macos-colour-panel (1b80dbd8) and bpr-0779-appearance-cosmetics (c85f2679, 'Accept both as shipped'). The named unresolved items are unchanged and are not closed by those verdicts: no automated course opens the real macOS colour panel, C1's two storage limitations stand, and C4's drift - diff_hunk_header_background and _text painted by nothing, code_current_line_background and the tab markers painted only by this preview - stays recorded for main rather than fixed here."}},{"criterion":"One integration pass is recorded with its revision","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"command","detail":"d8's integration pass at the pack's combined boundary: 48 courses, 100% passed, 48/48, 882.21 s (scratchpad/d8-int-pass-01.log, zero ***Failed lines), at zero pending build actions - `ninja -C build/dev -n` reported 'no work to do' immediately before and immediately after - and against the same executable the owner reviewed, sha256 d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b. src/ is byte-identical to the reviewed revision (git log ed2b3f14..HEAD -- src/ is empty), so the pass confirms the pack at exactly the revision the verdicts bind to. d7's 34-course pass at ed2b3f14 (scratchpad/d7-int-pass-01.log) and its 16-course post-fix run (d7-int-ctest-04.log) remain valid and are not superseded. test_network_isolation_manifest was run separately because d8 adds a test registration: it fails on the .claude/worktrees checkout's duplicate add_test ownership and on gh_cli_git_smart_transport, as before, and names nothing from the new course (scratchpad/d8-isolation-manifest.log). Documentation gate DOCUMENTATION_OK (d8-doc-gate-06.log) and 64 documentation tests OK (d8-doc-tests-01.log)."}},{"criterion":"Formal closeout obeys canonical order and actual platform and attended results","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"observation","detail":"OBEYED BY NOT MOVING THE STATUS. RFC-BPR-0034 stays Proposed. Same index rule: 'Closure remains strictly ordered: no RFC becomes Auto-Verified before every listed dependency is Auto-Verified.' RFC-BPR-0034 depends on RFC-BPR-0006 (Auto-Verified) and RFC-BPR-0018 (Proposed), so it is gated by BPR-0593 alone. Attended results: all four RFC-BPR-0034 candidates cleared - the rendered page, the VoiceOver walk over the tiles, the real macOS colour panel and both cosmetic departures - and none was rejected. Platform results: C6 names macOS only. Not closed by any verdict and still open: no automated course opens the real macOS colour panel, and the three end-to-end appearance contracts are red in this sandbox for reasons proven independent of this pack. RFC-BPR-0006 is not reopened."}}]
---

# Verify and close RFC-BPR-0034

RFC base: `38f32226351f5cc8e4657d55c3e151d8450c6ce5`.

This card tracks formal closeout for its RFC. The checklist rows are the canonical
criteria of the live RFC; every row records the check run, its result and the commit.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **C1 — One customisation per theme.** The store keeps a customised palette per variant; migration files the existing palette under its recorded variant with nothing lost; overrides apply only while their variant is resolved; committing changes only the theme on screen; Export, Import and Reset all act on the theme on screen and say so; customised counts equal the roles that differ from the built-in palette. | **Pass, with two named limitations.** The store keeps one palette document and one stored flag per variant at schema 7. Migration is proven per direction by `test_version_six_files_one_palette_under_its_recorded_variant`, one case per variant with a whole-record comparison and the mode held at `high_contrast_dark` so a mode-derived reader fails three of four, and by `test_version_six_without_a_stored_palette_carries_no_customisation`. Per-variant commit, reset, export and import are proven by `test_an_adopted_palette_records_the_variant_it_customises` (Dark then Light both survive a fresh host lifetime), `test_a_palette_reset_names_one_variant_and_spares_the_others` and `test_editing_a_colour_keeps_every_other_theme`; `sqlite_appearance_settings_storage` and `application_appearance_host` pass. **Limitation 1:** the bytes of a version-6 `palette_document` whose `palette_stored` was 0 are discarded - dead data since the version-4 reset that was never rendered, so no customisation is lost, but the bytes do not survive. **Limitation 2:** the host's export publication carries a palette rather than a variant, so the theme it acted on is named by the composition and the page, not by the publication itself. | src `867e4bed`; finish `57b31397` |
| **C2 — The theme strip.** Six tiles, one per mode, in a sideways-scrolling well with a thin scrollbar and Left and Right selection; each painted by the existing preview painter from its built-in palette plus its overrides with the wider snippet; both System tiles split diagonally by clipping with the half in effect named in the caption; customised counts captioned; the mode combo box absent. | **Pass.** `qt_theme_preview_strip` proves six tiles, one per `core::appearance_mode`, in the accepted order, in a sideways-scrolling inset well, with Left and Right moving the selection and scrolling it into view - asserted both by the scroll bar's value moving and by the tile rect landing whole inside the viewport. Both System tiles split diagonally **by clipping** with no edge drawn, and pixel probes compare each half against `builtin_appearance_palette` for light, dark, high-contrast light and high-contrast dark, with a non-System tile asserted identical in both corners so a blanket clip would fail. Captions come only from the variant the composition supplies, because the RFC forbids deriving it in Qt. The six-way mode combo box is absent from the widget tree and the `QAccessible` tree. Rendered: `appearance-tile-0` through `tile-5` captured at four appearances x four scales. | src `a5427499`; finish `2607168c` |
| **C3 — Type rows.** Eleven one-line rows of label, info glyph and control with sizes in points, weight as a 100 to 900 stepper, tab width as a 2, 4, 8 segment and switches for ligatures and emphasis; every consequence reachable by hover, by keyboard focus and as the glyph's accessible description; Reset in the card header. | **Pass; one genuine defect was found and fixed here.** `qt_appearance_settings_panel` proves eleven rows in the accepted order, each asserted to be **one line by geometry** - label, glyph and control centres within 4 px and the row no taller than its tallest child plus 12 - with sizes in points and never hundredths, a 100-to-900 weight stepper, a 2/4/8 tab segment, switches for ligatures and both emphases, and each card's header Reset dispatching its existing scope. Every consequence is reachable three ways, each compared against the sentence the control carries so no route is checked against itself: hover, the glyph's accessible description, and the popover actually becoming visible on keyboard focus. **That third route was broken:** the tip was bound to the glyph's rect, which hides whenever the pointer is elsewhere, and was shown from inside `focusInEvent`, where Qt's own tip window hides itself on FocusIn. Both causes were discriminated by a probe and fixed. Before that, every property a test could read looked correct while the sentence was shown to nobody. | src `a5427499`; finish `2607168c` |
| **C4 — The preview.** A labelled preview painted by the review window's painter from the accepted typography and the theme on screen, whose six code lines show an italic comment, bold keywords, a type, a function, ligature pairs, two tab depths with markers, removed and added lines, a selection, a macro and the current line, with a caption stating the values in effect, repainting on every accepted change. | **Pass, with drift and one cosmetic departure referred to the owner.** `qt_type_preview` and `qt_code_sample_painter` prove the six sample lines carry the italic comment, bold keywords, a type, a function, the `!=`/`&&`/`>=` ligature pairs, two tab depths with faint markers, removed and added lines, a selection, a macro and the current-line band, with a caption stating the values in effect, repainting on every accepted change and not otherwise. The RFC's ban on 'a preview painter of its own' is met by lifting the review window's own role-to-format, row-band and byte-to-character helpers into one shared translation unit; `qt_syntax_pane_composition` and `qt_diff_workbench_scale_100/125/200` prove the review panes' behaviour is unchanged. **Drift, for main to allocate, not fixed here:** `diff_hunk_header_background` and `diff_hunk_header_text` are painted by **nothing at all**, before or after this pack, and `code_current_line_background` and the tab markers are now painted **only** by this preview - the review pane still paints neither. **Cosmetic departure, verified in source in d7 and never previously recorded, now accepted:** the preview's selection is a full-width row wash (`code_sample_painter.cpp` sets `QTextFormat::FullWidthSelection` and washes whole lines with Highlight at alpha 72) rather than a character-range highlight. Put to the owner as `bpr-0779-appearance-cosmetics` and answered "Accept both as shipped" on 2026-09-16 (decision c85f2679). | src `a5427499`; finish `2607168c` |
| **C5 — The colours card.** Titled for the theme on screen with its subtitle; Import, Export and Reset all in the header; a filtered, grouped, scrolling list of all 47 roles with sticky headers, swatch, name and value; a swatch opens the picker and previews live; edited rows show new and old; one bar with Cancel and Keep changes appears only while previewing; the role combo box, hex field and separate preview buttons are absent. | **Pass by course, with the platform picker unresolved and one cosmetic departure.** `qt_colour_role_list` and `qt_appearance_role_display` prove all 47 roles across four groups counted 7/8/6/26 from membership rather than restated, sticky group headers, a filter, a swatch, the role's name in words and its value in the code font, an edited row showing new beside old, the bar created when a preview begins and destroyed when it ends (asserted by `findChild` returning `nullptr`, not by a hidden widget), Keep changes carrying the existing commit intent, and the absence of the role combo box, hex field and separate preview buttons from both trees. No displayed string equals a `core::stable_name`. **Attended on 2026-09-16, and the machine gap is unchanged:** the owner exercised the real macOS colour panel on the reviewed build and accepted it (`bpr-0779-macos-colour-panel`, decision 1b80dbd8). No automated course opens that panel - only an injectable seam - so the platform picker's live preview still has no machine evidence, and that remains named here rather than quietly closed by the verdict. **This dispatch found and fixed a defect on this card:** the colours scroll well carried an object name but no accessible name, and a `QScrollArea` defaults to `Qt::WheelFocus`, so it was a real Tab stop a screen reader reached unannounced. **Cosmetic departure, verified in source and never previously recorded, now accepted:** the subtitle is added to the card body rather than the header row. Same candidate and same verdict as C4's: `bpr-0779-appearance-cosmetics`, "Accept both as shipped", decision c85f2679. | src `a5427499`, `ed2b3f14`; finish `2607168c` |
| **C6 — Rendered and reachable.** The page in its default, editing and light-desktop states, with every tile, renders at 100, 125, 150 and 200 percent in the four appearances, every control reachable by keyboard with an accessible name and the tiles' captions announced, and the owner accepts the real macOS application through the attended-review workflow. | **Pass. Both halves are done.** Built in d7, because no such course existed despite the criterion's wording: `qt_surface_capture_course_scale_100/125/150/200` produces **144 Appearance captures** - default, editing, light-desktop and each of the six tiles - at four appearances x four scales, each raster asserted to equal logical size x device ratio. The accessibility walk covers **151 stops with unnamed=0** after the colour-well fix; before it, all four scale entries failed on exactly that one assertion, which is the negative control. Tile captions are asserted to appear in the strip's announcement. **The owner accepted the real macOS page on 2026-09-16**, bound to executable sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b` from source `ed2b3f14`: `bpr-0779-appearance-rendered` (c5349059), the VoiceOver walk over the tiles `bpr-0779-voiceover-appearance-tiles` (6825fbc6) and the real macOS colour panel `bpr-0779-macos-colour-panel` (1b80dbd8), each accepted. | src `ed2b3f14`; captures `scratchpad/d7-captures-final`, 400 files, SHA-256 manifest `ec9e6100cdebff710f77b54dcdf157e298017529c7b27cfbf657b740182384f6` (d8) |

## Outcome

Reconcile every C row with direct evidence or a named unresolved criterion, run one integration pass over the appearance host, store migration and composed Settings courses, and record the truthful formal status.

## Acceptance

- All C1–C6 rows reference direct evidence or a specific unresolved criterion.
- One integration pass over the appearance migration, host and composed Settings courses is recorded with its revision.
- Formal closeout obeys canonical order and actual platform and attended results; RFC-BPR-0006 is not reopened.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Integration pass - d7, revision `ed2b3f14`

One integration pass over the appearance migration, host and composed Settings
courses, inside the 34-course selection run at zero pending build actions
(`scratchpad/d7-int-pass-01.log`, **100% passed, 34/34, 271.77 s**):
`application_appearance_host`, `sqlite_appearance_settings_storage`,
`qt_appearance_settings_panel`, `qt_appearance_role_display`, `qt_appearance_profile`,
`qt_colour_role_list`, `qt_theme_preview_strip`, `qt_type_preview`, `qt_code_sample_painter`,
`qt_settings_window` and its 125/150/200 scales, and `qt_settings_effect_boundary` all pass.

**The three end-to-end appearance contracts do not reach OK, and are not claimed as
green.** Established directly this dispatch rather than carried on a previous
dispatch's word (`scratchpad/d7-int-e2e-status.log`, **0% passed, 4 tests failed out
of 4**):

| Course | Where it stops |
|---|---|
| `application_appearance_theme_customization_contract` | never reaches `APPEARANCE_THEME_CUSTOMIZATION_CONTRACT_OK` |
| `application_appearance_gesture_contract` | `PUBLIC_PATH_FAILED`, 120.44 s |
| `application_appearance_store_fallback_contract` | never reaches `APPEARANCE_STORE_FALLBACK_CONTRACT_OK` |
| `application_diff_workbench_public_path` - the discriminator, which drives **no appearance probe at all** | fails identically on the same binary |

The stop markers across the run are `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` (x3),
`EPOCH_FIXTURE_SEED_FAILED` and `PUBLIC_PATH_FAILED` (x2) - none of them an appearance
stage. Every appearance probe fact string from the real binary is all ones:
`ADOPTION_FACTS:11111111`, `EQUAL_COMMIT_FACTS:111111111`, `FIRST_FACTS:1111111`,
`IDENTITY_FACTS:11111111111`, `MANUAL_FACTS:11111111` and
`STORED_ADOPTION_FACTS:111111111`. So the appearance work these contracts carry passes
and the courses stop on unrelated diff-evidence, epoch-fixture and public-path stages
that are red in this sandbox independently of this pack. Do not read a `*_CONTRACT_OK`
string in a marker count as a pass: ctest echoes it in its own `Regex=[...]` line.

## Formal status: RFC-BPR-0034 stays `Proposed`, and that is the truthful result

Every C row above now carries direct evidence or a named unresolved criterion,
and the owner accepted the rendered page, the VoiceOver walk over the tiles,
the real macOS colour panel and both cosmetic departures on 2026-09-16. The
RFC's own criteria are met.

It does not become `Auto-Verified`, because the RFC index - the repository's
sequencing authority - says so in as many words: "Closure remains strictly
ordered: no RFC becomes `Auto-Verified` before every listed dependency is
`Auto-Verified`." RFC-BPR-0034 depends on RFC-BPR-0006 (`Auto-Verified`) and
**RFC-BPR-0018 (`Proposed`)**. This milestone's own boundary is "formal
closeout follows canonical order and never falsifies a status", so the status
stays where it is.

The gate is held by [[BPR-0593]], RFC-BPR-0018's closeout card. When it closes,
RFC-BPR-0034's status and its index row are a one-line change with no further
verification owed.

Two things stay open and are named rather than closed by a verdict. The real
macOS colour panel has an owner acceptance but still **no machine evidence**:
no automated course opens it, only an injectable seam. And the three
end-to-end appearance contracts are red in this sandbox for reasons proven
independent of this pack - the discriminator that drives no appearance probe at
all fails identically on the same binary.

### Integration pass - d8, source revision `ed2b3f14`, courses at `a85a1365`

One integration pass at the pack's combined boundary, run by the integrator, at
**zero pending build actions** (`ninja -C build/dev -n` reported "no work to do"
immediately before and immediately after) and against the **same executable the
owner reviewed**, sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b`.

**48 courses, 100% passed, 48/48, 882.21 s** (`scratchpad/d8-int-pass-01.log`,
zero `***Failed` lines): every `qt_composed_github`, `qt_settings_window`,
`qt_workbench_window`, `qt_home_list`, `qt_anonymous_public_preview`,
`qt_device_authorization` and `qt_surface_capture_course` scale variant, the
appearance set (`qt_appearance_*`, `qt_colour_role_list`, `qt_theme_preview_strip`,
`qt_type_preview`, `qt_code_sample_painter`, `application_appearance_host`,
`sqlite_appearance_settings_storage`), `qt_profile_management_window`,
`qt_settings_effect_boundary`, `application_workbench_host`,
`application_queue_composition_contract`,
`application_setup_destination_public_path`, `architecture_boundary_contract`,
`sqlite_workbench_settings_storage`, all seven `application_profile_*_public_path`
courses, and the new `application_profile_empty_desktop_native_menu_bar`.

`src/` is byte-identical to the reviewed revision - `git log ed2b3f14..HEAD -- src/`
is empty - so this pass confirms the pack's behaviour at exactly the revision the
owner judged, rather than at a moved one. `test_network_isolation_manifest` was
run separately because d8 adds a test registration; it fails on the
`.claude/worktrees` checkout's duplicate `add_test` ownership and on
`gh_cli_git_smart_transport`, exactly as before, and names nothing from the new
course (`scratchpad/d8-isolation-manifest.log`).

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
