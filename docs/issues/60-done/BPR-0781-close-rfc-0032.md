---
schema: "implement-rfc.card.v1"
id: "BPR-0781"
title: "Verify and close RFC-BPR-0032"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0779", "BPR-0780"]
closeout_for: ["RFC-BPR-0032"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["A1","A2","A3","A4","A5","A6","A7","A8"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"All A1-A8 rows reference direct evidence or a specific unresolved criterion","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"observation","detail":"Every A row in the checklist carries a direct check or a named unresolved criterion, and the rows that were waiting on a person now carry the owner's verdict of 2026-09-16 bound to executable sha256 d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b from source ed2b3f14. A1, A6 and A8 carry acceptances (bpr-0779-accounts-rendered d0bb147d, bpr-0779-home-rendered 521502f7, bpr-0779-voiceover-accounts-home 506a1c44, bpr-0779-attention-tones-wording f9a2a39b). A5's 'and nothing else' is resolved in the owner's own words (bpr-0779-a5-nothing-else c8e02af0) and RFC-BPR-0032 A5 now carries that direction; its second clause, the RFC's example session line naming two accounts and a refresh time that no projection carries, remains the row's one named unresolved clause. A7's three divergences were accepted as shipped (bpr-0779-filter-and-refresh-wording 910af65f). A2, A3 and A4 keep their named limitations: the manual-token prompt's own contents, application_saved_server_contract stopping on a fixture-seed failure in this sandbox, and the text inventory covering rendered rather than painted text. A2 additionally records the defect the owner reported outside the review, BPR-0797, and says why it is not held against the row."}},{"criterion":"One integration pass is recorded with its revision","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"command","detail":"d8's integration pass at the pack's combined boundary: 48 courses, 100% passed, 48/48, 882.21 s (scratchpad/d8-int-pass-01.log, zero ***Failed lines), at zero pending build actions - `ninja -C build/dev -n` reported 'no work to do' immediately before and immediately after - and against the same executable the owner reviewed, sha256 d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b. src/ is byte-identical to the reviewed revision (git log ed2b3f14..HEAD -- src/ is empty), so the pass confirms the pack at exactly the revision the verdicts bind to. d7's 34-course pass at ed2b3f14 (scratchpad/d7-int-pass-01.log) and its 16-course post-fix run (d7-int-ctest-04.log) remain valid and are not superseded. test_network_isolation_manifest was run separately because d8 adds a test registration: it fails on the .claude/worktrees checkout's duplicate add_test ownership and on gh_cli_git_smart_transport, as before, and names nothing from the new course (scratchpad/d8-isolation-manifest.log). Documentation gate DOCUMENTATION_OK (d8-doc-gate-06.log) and 64 documentation tests OK (d8-doc-tests-01.log)."}},{"criterion":"Formal closeout obeys canonical order and actual platform and attended results","outcome":"pass","revision":"ed2b3f14","evidence":{"kind":"observation","detail":"OBEYED BY NOT MOVING THE STATUS. RFC-BPR-0032 stays Proposed. The RFC index is the repository's sequencing authority and states: 'Closure remains strictly ordered: no RFC becomes Auto-Verified before every listed dependency is Auto-Verified.' RFC-BPR-0032 depends on RFC-BPR-0016 (Auto-Verified), RFC-BPR-0018 (Proposed) and RFC-BPR-0028 (Proposed). Two of three are open, so the status cannot move without falsifying it, and this milestone's own boundary is 'formal closeout follows canonical order and never falsifies a status'. The gate is held where it can be: BPR-0593 is RFC-BPR-0018's closeout card; RFC-BPR-0028 has no closeout card at all, which the card records rather than glosses. Platform results: A8 names macOS only and the owner accepted it there; Windows and Linux empty-desktop quit belongs to RFC-BPR-0033 and is recorded not run on BPR-0798 and BPR-0799. Attended results: all seven RFC-BPR-0032 candidates cleared, none rejected. The settings guide named in this RFC's Documentation impact section does not exist because RFC-BPR-0018 is unimplemented; that is pre-existing RFC-BPR-0018 debt and nothing was invented to stand in for it."}}]
---

# Verify and close RFC-BPR-0032

RFC base: `38f32226351f5cc8e4657d55c3e151d8450c6ce5`.

This card tracks formal closeout for its RFC. The checklist rows are the canonical
criteria of the live RFC; every row records the check run, its result and the commit.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **A1 — One card per server.** Accounts & hosts shows the title, the sentence, one card per configured host with header, account row, Access, Rate limit and Details, and the Add enterprise server button; the host-profile picker and its buttons, the locator, the status banner and the servers combo editor are absent from the page and from its accessibility tree. | **Pass.** The composed Settings window walks the Accounts page's widget tree *and* its `QAccessible` tree in five connection states, asserting the card inventory and the absence of the host-profile picker and its buttons, the locator, the status banner and the servers combo editor: `qt_composed_github_scale_100/125/200`, `qt_settings_window`, `test_accounts_page_carries_none_of_the_retired_chrome`, `test_one_card_per_server_holds_its_ways_in`, `test_the_servers_combo_editor_is_gone`. Rendered: 48 Accounts captures at four appearances x four scales (d7). The owner's acceptance is A8's and was **given on 2026-09-16**: `bpr-0779-accounts-rendered`, accepted. | src `284d1eac`, `aee19b26`; finish `41353ba8`, `4e891655` |
| **A2 — Sign-in is the account row.** Sign in begins the device flow inside the card's account row and shows the code with its copy glyph, the address, Open browser, the validity, one status sentence and Cancel; denied, expired, unavailable and cancelled states show in the same row with one retry; no disabled button and no truncated field is shown, and the manual-token path stays reachable. | **Pass, with one named limitation.** `qt_device_authorization_scale_100/125/200` and `qt_composed_github_scale_*` prove one Sign in gesture reaches the code, address, Open browser, validity, status sentence and Cancel inside the account row, and that denied, expired, unavailable and cancelled each show there with one retry and no disabled button: `test_one_sign_in_gesture_shows_the_code_in_the_card`, `test_terminal_and_token_rows_stay_in_the_card`, `test_the_row_hides_what_it_cannot_do_and_offers_the_token_way_in`. **Unresolved:** the manual-token path is proven only as far as the Connect action's intent; the prompt's own contents are the composition's modal and are not asserted. **Reported by the owner on 2026-09-16, carried as [[BPR-0797]], and not held against this row:** in a build with `BUILD_TESTING` on - the CMake default - Open browser reaches `recording_device_authorization_browser`, which returns `true` without opening anything, and the row then says "Opened in your browser." A2 asks that Open browser be present *in the account row*, and this RFC's own verification table names "the recording device port" as A2's direct surface, so the criterion stands as written. What is wrong is that the surface asserts an outcome that build cannot produce, which is a defect against RFC-BPR-0016, not against A2. | src `fea32882`; finish `2a06779c` |
| **A3 — Enterprise servers are cards.** Add enterprise server adds a card in its edit state using the existing saved-server intents; Remember this server is a switch with its consequence; Forget server confirms and names its scope; a token session is labelled session-only. | **Pass, with one environmental gap.** `test_add_enterprise_server_opens_a_card_in_its_edit_state` and `test_a_kept_enterprise_server_remembers_and_forgets_on_its_card` prove the edit-state card over the existing saved-server intents, Remember as a switch with its consequence, Forget confirming and naming its scope, and the session-only token sentence. **Not run in this sandbox:** `application_saved_server_contract` stops at stage 4 `epoch_refresh` on a fixture-seed failure, as it does at HEAD, so the repointed seed stage is compiled but not exercised. **Accepted as shipped 2026-09-16:** the two silent refusals BPR-0774's independent review raised - a readiness or validity refusal of Remember returns the switch to the stored record and says nothing, and a collection-limit refusal of an Add is said on Reviews & queue rather than beside the refused draft. Put to the owner as `bpr-0779-review-followups` and answered "Accept both as shipped" (decision 3ed46318). They stay as they are; no follow-up is raised. | src `51c443d2`; finish `838aeae0` |
| **A4 — Nothing repeats.** On every state of the Accounts page each of host, login, transport and connection state appears once, checked by a text inventory over the rendered page. | **Pass, with one named limitation.** `test_accounts_page_says_each_fact_once` inventories the rendered page's text across seven states, each with Details closed and open, and a negative control catches a planted repeat. Two deliberate readings are recorded on BPR-0772: GHES keeps its transport because A4 needs it once per card, and the device address is exempt. **Limitation:** the inventory covers rendered text, not painted text such as the usage ring, nor tooltips. | src `284d1eac`, `aee19b26`; finish `41353ba8`, `4e891655` |
| **A5 — Home leads with the bar.** Signed in, Home's first row under the strip is the address-field bar with its label, placeholder, ⌘L and Return, focused on arrival; the landing's focus-only buttons are gone; a load refusal is one sentence with Copy details under the bar; the pull-request stack's empty page is not part of Home; first launch and signed out show the strip with Home, account and settings only, the bar relabelled for public reading, the sign-in card with Sign in filled and Add enterprise server bordered, and the status bar line, and nothing else. A signed-out window that has public reading history keeps Recently viewed (owner direction, 2026-09-16, attended candidate `bpr-0779-a5-nothing-else`); so "nothing else" bounds the landing chrome this clause lists rather than the window's reading history. | **Pass, with one unresolved clause.** `test_home_leads_with_the_bar_signed_in`, `test_home_signed_out_is_one_paste_field`, `test_a_load_refusal_is_said_under_the_home_bar` and the composed courses prove the bar alone in the first row, focused on arrival, the refusal beneath it, the removed focus-only buttons, the first-launch card with its two ways in, and the stack's empty page kept off Home. **This dispatch also found and fixed a defect here:** signed out, the bar's frame absorbed the column's spare height and drew its border around 527 points of nothing against a size hint of 37 - the empty box A6 forbids - now pinned by an assertion that failed at 527 before the fix (`ed2b3f14`). **Resolved 2026-09-16:** the RFC's "and nothing else" versus the signed-out Recently viewed that BPR-0775 deliberately keeps. Attended candidate `bpr-0779-a5-nothing-else`, decision c8e02af0: "Keep Recently viewed when signed out and there is public reading history". The shipped behaviour is unchanged and RFC-BPR-0032 A5 now carries that direction in its own text. **Unresolved:** the RFC's example session line names two accounts and a refresh time; the shipped line carries one account on one host and no refresh time, because no projection carries either. **Accepted as shipped 2026-09-16:** a host session failure is said only on that host's card in Settings and is not surfaced on Home, which BPR-0775's independent review raised. Put to the owner as `bpr-0779-session-failure-discoverable` and answered "Acceptable as shipped" (decision 5376adc5). | src `edb90493`, `d0eded6c`, `ed2b3f14`; finish `51fcca79` |
| **A6 — Rows in state order, panels by state.** Rows are always ordered Needs me, Waiting on others, Following from the core attention projection, with one row anatomy for the queue and Recently viewed; colour means state only, on the reason word and the row mark or panel rail; with State headers on each state is a bordered panel with a tinted sticky header, count, chevron and rail, collapsible by pointer and keyboard with a one-line summary when collapsed; empty states are absent and an all-empty list is one sentence linking to Reviews & queue; no empty box renders. | **Pass.** `qt_home_list_scale_100/125/200` prove state order over all seven buckets, one `home_row_delegate` shared by the queue and Recently viewed, 44-point non-eliding rows, collapse by chevron, Space, Return, Left and Right with the count and summary retained and the accessible state announced, and sticky headers under scroll. Colour-means-state is proven on a greyscale test palette, where saturated pixels appear only in the reason word, the rail and the header. Empty states are absent, the all-empty list is one sentence linking to Reviews & queue, and no empty box renders. **Accepted 2026-09-16:** the attended verdict on attention tones and row wording, `bpr-0779-attention-tones-wording`, decision f9a2a39b, taken on the real macOS application. | src `2729b93a`; finish `584260be` |
| **A7 — Group, account and Filter.** Group offers None, Organization, Repository and Account, each group a neutral panel headed by the owner's or account's avatar and name, with state bands nested inside when State headers is on and plain state-ordered rows when off; a host shows only where it differs; with more than one signed-in account each row carries the account chip at its right, a single-account group carries it in its header instead, and a single-account profile shows none; the strip holds Group, State headers, Filter and the refresh glyph before account and settings; Filter holds exactly My role, Added by, Quiet items and Sort within a group over the existing typed preferences with their store and reset unchanged; Group, State headers, the state panels' and Recently viewed's collapse, and Filter persist per window, while a group panel's collapse lasts only while its window is open (owner direction, 2026-09-15); Home over a review keeps its one return row; the status bar shows the session line and never that no pull request is open while Home is shown. | **Pass, with three recorded divergences from the RFC's own words.** 16 Qt courses prove every Group value with State headers on and off over a two-account fixture, chips following the account count and the group, sort within a state, band collapse and sticky nesting, the keyboard walk over groups, bands and rows, and the strip holding Group, State headers, Filter and refresh signed in and none signed out. Filter holds exactly the four surviving preferences over the existing typed store with Reset unchanged. Group collapse is deliberately **not** persisted, by the owner's direction of 2026-09-15 ('Do what's easiest, which sounds like dont remember for now'), recorded in the RFC and pinned by `test_a_group_collapse_never_reaches_the_remembered_view`. **Divergence 1:** there is no avatar cache, so every avatar is an initial tile rather than the RFC's cached GitHub avatar. **Divergence 2:** the Filter vocabulary is the typed one - the board's 'Rule', 'Follow' and 'Request' have no typed value, and no update time is projected, so there is no 'Updated'. **Divergence 3:** the refresh glyph acts on the pull request open beneath Home and is disabled when none is open; no whole-queue refresh intent exists. All three were put to the owner as `bpr-0779-filter-and-refresh-wording`, and on 2026-09-16 they chose "Accept all three as shipped" (decision 910af65f). They are accepted behaviour, not debt. The absent whole-queue refresh intent stayed a follow-up for main, named here rather than implied; it was allocated on main in d9 as [[BPR-0801]], which starts from the owner's acceptance and asks whether the queue should be refreshable at all before any intent is written. *Amended 2026-09-23, history unchanged:* [[BPR-0801]] resolved divergence 3 with RFC-BPR-0026's explicit Refresh - on Home the glyph is now the profile's discovery refresh ([[BPR-0990]], `0d56cee5`), enabled whenever an account session can run it; the review strip's refresh still acts on the open pull request. | src `ce62ed52`, `fda85932`; finish `697e4728`, `8bac7e4b` |
| **A8 — Rendered and reachable.** The Accounts page in its connected, signed-out and signing-in states and Home in its first-launch, default, long-list scrolled, nothing-needs-me, over-a-review and four grouping states render at 100, 125, 150 and 200 percent in Light, Dark, High-contrast Light and High-contrast Dark, every control reachable by keyboard in reading order with an accessible name and a collapsed panel's state announced, and the owner accepts the real macOS application through the attended-review workflow. | **Pass. Both halves are done.** Built in d7 (no such course existed, contrary to the RFC's wording): `qt_surface_capture_course_scale_100/125/150/200` produces **400 captures**, 25 named states x four appearances x four scales, including every Accounts and Home board this criterion names, with each raster asserted to equal logical size x device ratio. The accessibility walk reaches every control in reading order with **unnamed=0** on Accounts signed out (14 stops), Accounts connected (16) and Home (12), and a collapsed panel announces its state. **The owner accepted the real macOS application on 2026-09-16**, bound to executable sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b` from source `ed2b3f14`: `bpr-0779-accounts-rendered` (d0bb147d), `bpr-0779-home-rendered` (521502f7) and the VoiceOver walk `bpr-0779-voiceover-accounts-home` (506a1c44), each accepted. This criterion names macOS only; Windows and Linux are outside it. | src `ed2b3f14`; captures `scratchpad/d7-captures-final`, 400 files, SHA-256 manifest `ec9e6100cdebff710f77b54dcdf157e298017529c7b27cfbf657b740182384f6` (d8) |

## Outcome

Reconcile every A row with direct evidence or a named unresolved criterion, run one integration pass over the composed Settings and Home courses, and record the truthful formal status.

## Acceptance

- All A1–A8 rows reference direct evidence or a specific unresolved criterion.
- One integration pass over the composed Settings, device and Home courses is recorded with its revision.
- Formal closeout obeys canonical order and actual platform and attended results.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Integration pass - d7, revision `ed2b3f14`

One integration pass over the composed Settings, device and Home courses, run at
zero pending build actions after a complete link:

| Selection | Result |
|---|---|
| 34 courses: `qt_composed_github_scale_*`, `qt_settings_window` and its 125/150/200 scales, `qt_workbench_window_scale_*`, `qt_home_list_scale_*`, `qt_profile_management_window`, `qt_appearance_*`, `qt_settings_effect_boundary`, `application_workbench_host`, `application_queue_composition_contract`, `application_setup_destination_public_path`, `architecture_boundary_contract`, `sqlite_workbench_settings_storage`, `sqlite_appearance_settings_storage` and the five `application_profile_*_public_path` courses | **100% passed, 34/34, 271.77 s** (`scratchpad/d7-int-pass-01.log`) |
| 16 courses after the two defects found by the new capture course were fixed: the capture course at four scales plus `qt_composed_github_scale_*`, `qt_workbench_window_scale_*`, `qt_home_list_scale_*` and `qt_anonymous_public_preview_scale_*` | **100% passed, 16/16** (`scratchpad/d7-int-ctest-04.log`) |

Not re-run and unchanged: the seven sandbox-blocked native courses recorded on
[[BPR-0775]] (credential store unavailable, a fixture-seed failure, a stale pass
expression, a hidden per-window Settings visibility check). `architecture_boundaries`
still stops on the unrelated `.claude/worktrees` checkout while
`architecture_boundary_contract` passes.

## Formal status: RFC-BPR-0032 stays `Proposed`, and that is the truthful result

Every A row above now carries direct evidence or a named unresolved criterion,
and the owner accepted the four rendered surfaces, the VoiceOver walks and each
wording question on 2026-09-16. The RFC's own criteria are met.

It does not become `Auto-Verified`, because the RFC index - the repository's
sequencing authority - says so in as many words: "Closure remains strictly
ordered: no RFC becomes `Auto-Verified` before every listed dependency is
`Auto-Verified`." RFC-BPR-0032 depends on RFC-BPR-0016 (`Auto-Verified`),
**RFC-BPR-0018 (`Proposed`)** and **RFC-BPR-0028 (`Proposed`)**. Two of the
three are not closed, so the status cannot move without falsifying it, and this
milestone's own boundary is "formal closeout follows canonical order and never
falsifies a status".

The gate is held where it can be held: [[BPR-0593]] is RFC-BPR-0018's closeout
card. RFC-BPR-0028 has no closeout card at all, so nothing but this paragraph
and the index record that it gates this RFC. When both close, RFC-BPR-0032's
status and its index row are a one-line change with no further verification
owed.

Related and recorded elsewhere, not against these rows: the settings guide that
this RFC's Documentation impact section names does not exist, because
RFC-BPR-0018 has not been implemented. That is pre-existing RFC-BPR-0018 debt,
not a gap this pack introduced, and nothing was invented here to stand in for
it.

### The captures are bound by digest, and they are not in the repository

The 400 rendered captures the A8, B5 and C6 rows rest on live in this session's
scratchpad, not under version control, so the path alone is not durable
evidence. d8 hashed the set: 400 PNGs, and the sorted `shasum -a 256` manifest
over them digests to
`ec9e6100cdebff710f77b54dcdf157e298017529c7b27cfbf657b740182384f6`. That digest
is what binds the rows; the files themselves will go with the session. Anyone
regenerating them runs `qt_surface_capture_course_scale_{100,125,150,200}` at
`ed2b3f14` and compares.

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
