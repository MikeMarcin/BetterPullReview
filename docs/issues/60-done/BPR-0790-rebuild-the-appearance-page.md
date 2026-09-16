---
schema: "implement-rfc.card.v1"
id: "BPR-0790"
title: "Rebuild the Appearance page as theme strip, type rows with preview, and colours for the theme on screen"
status: "Done"
rfcs: ["RFC-BPR-0034"]
depends_on: ["BPR-0789"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"appearance","acceptance":["C2","C3","C4","C5"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 - Tile inventory, order, scroll, keyboard selection and both System halves match the built-in palettes pixel-for-pixel where asserted; the combo box is gone","outcome":"pass","revision":"a54274992369c0c854144a0979c63ce8e62a5c0a","evidence":{"kind":"command","detail":"qt_theme_preview_strip, qt_appearance_settings_panel and the composed qt_settings_window at 100/125/150/200 pass (scratchpad d6-int-ctest-final.log, 21/21). Six tiles, one per core::appearance_mode, in the accepted order System, Light, Dark, High contrast system, High contrast light, High contrast dark, in a sideways-scrolling inset well; Left and Right move the selection and scroll it into view (asserted by both the scroll-bar value moving and the tile rect landing inside the viewport). Both System tiles split diagonally BY CLIPPING with no edge drawn, and pixel probes compare each half against builtin_appearance_palette(light), (dark), (high_contrast_light) and (high_contrast_dark); a non-System tile is asserted identical in both corners so a blanket clip would fail. Captions come only from the variant the composition supplies and from the per-variant counts published by BPR-0789; absent that variant the System tiles caption nothing, because RFC-BPR-0034 forbids deriving the variant in Qt. Overrides reach the tiles (a mutated dark palette changes the Dark tile's raster). The six-way mode combo box is deleted: test_the_retired_controls_are_absent walks the widget tree and the QAccessible tree and requires no combo box offering the mode labels or named 'Appearance mode', while requiring live identities present in the same walk. LIMITATION: offscreen rasters are layout and colour-composition evidence only; the rendered four-appearance, four-scale course and the attended verdict are C6 and belong to BPR-0779."}},{"criterion":"AC2 - Every Type row is one line; each consequence is reachable by hover, focus and accessible description; the preview repaints after each accepted change with the expected runs","outcome":"pass","revision":"a54274992369c0c854144a0979c63ce8e62a5c0a","evidence":{"kind":"command","detail":"qt_appearance_settings_panel (rewritten) and qt_type_preview pass. Eleven rows in the accepted order, each asserted to be ONE LINE by geometry - label, glyph and control centres within 4 px and the row no taller than its tallest child plus 12 - rather than by mere presence. Sizes render in points ('13 pt', two decimals dropped when zero) and never in hundredths, with ranges still taken from core::bounds_of; weight is a 100-to-900 stepper; tab width is a 2/4/8 segment; ligatures and both emphases are switches. Every consequence is reachable three ways, each compared against the sentence the CONTROL carries so no route is compared with itself: the glyph's tool tip (hover), the glyph's accessible description, and the popover actually becoming visible on keyboard focus with the matching text and disappearing on focus-out. That third route was genuinely broken and is fixed in this revision (rect-bound tip, plus the tip being shown from inside focusInEvent where Qt's tip window hides itself on FocusIn); a probe discriminated the two causes, and every other info_glyph consumer - qt_composed_github_scale_*, qt_workbench_window_scale_*, qt_profile_management_window, qt_settings_window - was re-run green. The preview carries the italic comment, bold keywords, a type, a function, the !=/&&/>= ligature pairs, two tab depths with faint markers, removed and added lines, a selection, a macro and the current-line band, with a caption stating the values in effect; it repaints after each accepted change and does no work on a repeat adoption."}},{"criterion":"AC3 - The colours list holds 47 roles in four groups; the picker previews live and Keep changes runs the existing commit; the bar exists only while previewing","outcome":"pass","revision":"a54274992369c0c854144a0979c63ce8e62a5c0a","evidence":{"kind":"command","detail":"qt_colour_role_list and qt_appearance_role_display pass. All 47 roles appear across four groups - Surfaces and text 7, Diff 8, Review states 6, Syntax 26 - with the counts derived from membership rather than restated, sticky group headers, a filter, a swatch, the role's name IN WORDS and its value in the code font, scrolling inside the card. The display vocabulary is a switch over the core enumeration with no default, so a new core role fails to compile rather than rendering blank, and the course asserts no displayed string equals a core::stable_name (the durable exchange identifier, which must not be shown). A swatch opens the picker through an injectable seam a course can drive; movements preview live (a burst of picker moves yields one queued preview intent carrying the latest palette), cancelling restores the role's starting colour, and an edited row shows its new value with the old beside it. Keep changes dispatches the EXISTING commit_appearance_palette_intent with the previewed palette; the bar is created when a preview begins and destroyed when it ends, asserted by findChild returning nullptr rather than by a hidden widget. The role combo box, hex field and the separate Preview/Commit/Cancel buttons are absent from both the widget and accessibility trees. Against the rebuilt page the real binary's appearance probe emitted all-ones fact strings through import, commit, export, cancelled, persisted and native stages (scratchpad d6-int-ctest-e2e-final.log). LIMITATION: no course opens the real macOS colour panel - only the injected seam - so the platform picker's live preview is attended evidence belonging to BPR-0779."}}]
---

# BPR-0790: Rebuild the Appearance page as theme strip, type rows with preview, and colours for the theme on screen

## Current frontier — BPR-0790 accepted; the `appearance` milestone is complete, bpr-shell-surfaces-20260915 d6 / rfc-integrator-d6 / mandate r3

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `appearance`: obligations `BPR-0789:*` and
  `BPR-0790:*`, both exits. This card closes `BPR-0790:*` and milestone exit 2; `BPR-0789` closed exit 1.
- **Candidate.** Local `main`, nothing pushed: promote `703ffc59`, claim `59410d96`, source `a5427499`, and
  this record in the finish commit.
- **Usable behaviour.** Appearance is now a title, the lede, and cards in one row language. **Theme** is an
  inset well of six tiles that scrolls sideways, one per mode, each painted from its own built-in palette
  plus that theme's overrides; both System tiles split diagonally by clipping, light over dark, each half in
  its own palette, captioned with the half in effect from the variant the composition supplies; a customised
  theme captions its count. **Type** is eleven one-line rows — label, info glyph, control — with sizes in
  points, weight as a 100-to-900 stepper, tab width as a 2/4/8 segment and switches for ligatures and the two
  emphases, beside a labelled preview painted from the accepted typography and the theme on screen with a
  caption stating the values in effect. **Colours** is titled for the theme on screen, says edits change only
  it, carries Import, Export and Reset all in its header, and lists all 47 roles in four groups with sticky
  headers, counts, a filter, a swatch, the role's name in words and its value in the code font; a swatch
  opens the platform picker and previews live, an edited row shows its new value beside the old, and one bar
  with Cancel and Keep changes exists only while previewing. **Dif** keeps its controls as a fourth card.
  The mode combo box, the three hundredths spin-box groups, the role combo box, the hex field and the five
  full-width buttons are gone from the widget tree and from the accessibility tree.
- **Verification.** Under Evidence: 21 courses green including every composed surface that owns the page, a
  rewritten panel course covering C3 and C5 and the retirements, new courses for the shared painter, the
  strip, the preview, the role vocabulary and the colour list, and one integration pass over the three
  end-to-end appearance contracts.
- **Carried forward.**
  - **Rendered, keyboard, screen-reader and attended evidence (C6) belongs to `BPR-0779`**, which unblocks
    now. It must cover: the page in its default, editing and light-desktop states at 100, 125, 150 and 200
    percent in the four appearances; the tiles' captions announced; the real macOS colour panel, which no
    course here drives (only the injected seam); and Dif's 40 px corner rendering already owed from BPR-0775.
  - **Documentation belongs to `BPR-0780`**: the settings guide's Appearance page, the appearance and
    accessibility UX document, the storage note for the schema bump, and the theme-document variant scope.
  - **Drift, for main to allocate:** `diff_hunk_header_background` and `diff_hunk_header_text` are consumed
    by **no painter at all**, before or after this card. `code_current_line_background` and tab markers are
    now painted only by the Appearance preview; the review pane still paints neither. This card deliberately
    did not widen the review surface to fix that.
  - Follow-up candidate: the Type card's single header Reset dispatches the two existing scopes
    (`interface_font`, `code_font`); there is no combined scope and none was invented.
- **Next.** `verification-closeout` (`BPR-0779`, `BPR-0780`, `BPR-0781`, `BPR-0782`, `BPR-0791`) is the only
  milestone left in this pack and was **not** authorized for this dispatch. `BPR-0779` and `BPR-0780` have no
  unmet dependencies now.

## Outcome

Lay the six preview cards in a sideways-scrolling well with the wider snippet, the diagonal System split by clipping, resolution and customised-count captions and keyboard selection, and remove the mode combo box; turn the typography group boxes into eleven one-line rows with an `info_glyph` (popover on hover and focus, accessible description) and a labelled `type_preview` painted by the review window's painter; replace the role combo box and hex field with a `colour_role_list` (filter, four sticky groups, swatch, name, value, platform picker with live preview) titled for the theme on screen, with Import, Export and Reset all in its header and one Keep changes and Cancel bar shown only while previewing.

## Acceptance

- Tile inventory, order, scroll, keyboard selection and both System halves match the built-in palettes pixel-for-pixel where asserted; the combo box is gone.
- Every Type row is one line; each consequence is reachable by hover, focus and accessible description; the preview repaints after each accepted change with the expected runs.
- The colours list holds 47 roles in four groups; the picker previews live and Keep changes runs the existing commit; the bar exists only while previewing.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Verification — d6, source revision `a5427499`

Logs are in the dispatch scratchpad: `d6-int-*` (integrator), `d6-0790a-*` and `d6-0790b-*` (the two
implementing slices).

| Check | Direct surface and command | Result |
|---|---|---|
| Full link before any claim | `cmake --build --preset dev`, then `ninja -n` | Exit 0, 0 pending actions before every run below. Enforced deliberately: the BPR-0789 worker's "PASS" had 167 actions outstanding, so results here were re-earned after a complete link (`d6-int-build-06/07/08.log`) |
| The page and every composed surface that owns it | `ctest --preset dev -R '^(qt_appearance_settings_panel\|qt_appearance_role_display\|qt_colour_role_list\|qt_appearance_profile\|qt_code_sample_painter\|qt_theme_preview_strip\|qt_type_preview\|qt_settings_window(_scale_(125\|150\|200))?\|qt_settings_effect_boundary\|qt_composed_github_scale_(100\|125\|200)\|qt_workbench_window_scale_(100\|125\|200)\|qt_profile_management_window\|application_appearance_host\|sqlite_appearance_settings_storage)$'` | **21/21 pass**, 66.73 s (`d6-int-ctest-final.log`). Chosen by composition rather than subsystem, per the board's recorded selector trap. `qt_settings_window` at four scales holds the page to no horizontal scroll at 760x480 in all four appearances, with the panel still a direct child of the Appearance page widget under its own object name |
| The same surface plus the end-to-end profile courses the retirement touched | same selector plus `.*profile_(startup\|multi_window\|management)_public_path` | **24/24 pass**, 140.70 s (`d6-int-ctest-0790b.log`) |
| C2 — the tiles | `qt_theme_preview_strip` | Six tiles in the accepted order; keyboard Left/Right moves the selection and scrolls it into view; **pixel probes on both halves of each System tile against `builtin_appearance_palette` for light, dark, high-contrast light and high-contrast dark**; captions from the supplied variant and the published counts; overrides reach the tiles; the raster is unchanged by the platform font, which is how the old hard-coded family is kept out |
| C3 — the rows | `qt_appearance_settings_panel` (rewritten) | Eleven rows, each asserted **one line** by geometry rather than by presence; sizes in points within `core::bounds_of`; the 2/4/8 segment; the switches; each card's header Reset dispatching its existing scope |
| C3 — every consequence, three ways | `test_every_consequence_is_reachable_three_ways` | Hover, the glyph's accessible description, **and the popover actually appearing on keyboard focus** — the last of which was broken and is fixed here (see Decisions) |
| C4 — the preview | `qt_type_preview` | The six sample lines carry the italic comment, bold keywords, a type, a function, the `!=`/`&&`/`>=` ligature pairs, two tab depths with markers, removed and added lines, a selection, a macro and the current line; the caption states the values in effect and never the hundredths; it repaints after each accepted change and not otherwise |
| C5 — the colours list | `qt_colour_role_list`, `qt_appearance_role_display` | All 47 roles across the four groups counted 7/8/6/26 from membership rather than restated; the filter; sticky headers; the swatch→picker→live-preview path through a recording seam; an edited row showing new and old; the bar present only while previewing; Keep changes carrying the existing commit intent; no displayed string equals a `core::stable_name` |
| Retired controls absent | `test_the_retired_controls_are_absent` | The mode combo box, the hundredths spin boxes, the role combo box, the hex field and the three separate palette buttons are absent from the widget tree **and** from the `QAccessible` tree, with live identities asserted present in the same walk so the absences are not an empty result |
| The extraction is behaviour-preserving | `qt_syntax_pane_composition`, `qt_diff_workbench_scale_100/125/200` | Pass. These assert exact format ranges, band colours, layering order and bounded span/block work in the review panes whose helpers were lifted into the shared painter (`d6-int-ctest-0790a.log`) |
| Integration pass, end-to-end | `ctest --preset dev -R '^(application_appearance_gesture_contract\|application_appearance_store_fallback_contract\|application_appearance_theme_customization_contract)$'` | **All three fail; not claimed as green** (`d6-int-ctest-e2e-final.log`, 350.58 s). Against the rebuilt page every appearance probe fact string from the real binary is all-ones — `ADOPTION_FACTS:11111111`, `EQUAL_COMMIT_FACTS:111111111`, `IDENTITY_FACTS:11111111111`, `MANUAL_FACTS:11111111`, `FIRST_FACTS:1111111` — across the import, commit, export, cancelled, persisted and native stages. The courses stop at `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` (x2), `EPOCH_FIXTURE_SEED_FAILED` and `PUBLIC_PATH_FAILED` |
| Why those failures are not this change | `ctest --preset dev -R '^application_diff_workbench_public_path$'` — the diff workbench through the real composition with **no appearance probe** | Fails identically on the same binary (`DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED`, 60 s timeout), so that stage is independently red in this sandbox. `EPOCH_FIXTURE_SEED_FAILED` is the fixture-seed class recorded on BPR-0775 |
| Format | per-file `clang-format --dry-run --Werror` under the clean-at-HEAD rule | Every file this card touches is clean. The project gate stays red for 17 files this milestone does not touch and which were already dirty at HEAD; they are deliberately not swept into this commit |

Limitations: offscreen rendering is layout and colour-composition evidence only. Nothing native, rendered,
VoiceOver or attended is claimed — C6 is `BPR-0779`'s, and the real macOS colour panel is never opened by a
course, only the injected seam. Four `better_pull_review` probe processes outlived their ctest runs across
this dispatch and were killed, leaving `build/dev/tests/end_to_end/appearance-theme-customization-root` and
`…/appearance-store-fallback-contract` dirty. The seven sandbox-blocked native courses recorded on BPR-0775
were not re-run.

### Decisions — d6, mandate r3

- **Dif keeps his controls, as a fourth card (drift D1, Appearance part; mandate r2).** BPR-0775 bounded this
  question to this milestone. RFC-BPR-0031 **B3** requires real Appearance controls that apply, migrate,
  persist and reset motion and visibility through the current profile authority; RFC-BPR-0034's accepted
  design names three cards and is silent about Dif. The two yield to each other cleanly: the controls stay
  and are re-expressed in the new row language as a **Dif** card — Show Dif mascot, Dif motion, the motion
  status sentence as a value row, and "Reset Dif preferences" (scope `dif`) in the card header like every
  other card. The accessible names and the existing accessors are unchanged, so B3's evidence still holds.
  Nothing about the mascot's *placement* changes here: the owner's r2 direction that Dif sit in the window's
  bottom-left corner is about where the mascot appears, which BPR-0775 settled, not about whether his
  preferences remain reachable — which B3 requires. This is the narrow layout adaptation D1 asked the owning
  milestone to record, and it needed no owner question.
- **Role names are new presentation text.** `core::stable_name` is the durable exchange identifier that theme
  documents are keyed by; displaying it would have tied a wire format to a label. The vocabulary is a single
  `switch` over the core enumeration with **no `default`**, so a role added to the core stops this
  translation unit compiling rather than rendering a blank row, and the group counts are counted from
  membership rather than restated.
- **The colour picker needs an injectable seam.** There is no `QColorDialog` anywhere else in the tree and no
  picker port. A modal no course can drive would have made C5 unverifiable, so the list takes a settable
  picker defaulting to the platform dialog, with a `Changed` callback because the RFC wants the preview to
  follow the picker *as it moves*. Cancelling restores the role's starting colour, so nothing stays previewed
  that nobody accepted.
- **The preview is the review window's painter, not a second one.** RFC-BPR-0034 prohibits "a preview painter
  of its own", but the real painter is file-local inside `diff_workbench.cpp` and needs an accepted diff
  result, and `theme_preview_strip` already carried its own three-row renderer with a hard-coded font family
  — the very thing the prohibition names. The role→format, row-band and byte-to-character helpers were
  therefore lifted into one shared translation unit that the review panes, the tiles and the preview all use;
  the panes' behaviour is unchanged and their courses prove it. What the review pane paints was deliberately
  **not** widened, so the roles it never consumed are recorded as drift rather than silently fixed here.
- **The info glyph's keyboard route was broken, in two ways, and is fixed.** RFC-BPR-0034's row language
  requires the consequence on hover **and on keyboard focus**. `show_popover` bound the tip to the glyph's
  rect, which hides it whenever the pointer is elsewhere — and a keyboard reviewer's pointer always is. With
  that removed the popover still did not appear, and a probe discriminated why: the tip was shown from inside
  `focusInEvent`, and Qt's own tip window hides itself on FocusIn, so the focus change that asked for the
  sentence destroyed it. It is now unbound and deferred one turn of the event loop, and the course asserts it
  appears and then goes away on focus-out. The widget is shared with the Accounts page, so every other
  `info_glyph` consumer was re-run and is green. Worth stating plainly: before this, every property a test
  could read looked correct while the sentence was shown to nobody.
- **Five courses outside this card's files were edited**, unavoidably: deleting `mode_selector()` and the
  palette buttons left `settings_effect_boundary_tests`, `profile_management_window_tests` and the three
  `profile_*_public_path_tests` referring to an API that no longer exists. Each change is a mechanical
  substitution of the new gesture (`choose_mode`, the swatch and picker seam) for the retired control, driving
  the same behaviour with the same assertions; none weakens a claim.
- **Built in two sequential slices with disjoint files** — the painting half, then the page half — because
  this run had already lost time to two concurrent workers leaving a half-applied change. No file had two
  writers.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
