---
schema: "implement-rfc.card.v1"
id: "BPR-1213"
title: "Keep Overview folders flat and collapse only their files"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"Owner clarification during BPR-1199 review, 2026-09-26","related_rfcs":["RFC-BPR-0037"],"related_issues":["BPR-0934","BPR-0972","BPR-1199"],"scope":"Tracking-only later Overview presentation correction; not a new performance requirement or private-comparison diagnosis","session":"2c2e72e3af2e2fd7fa1a566ec49bc5ca","candidate":"BPR-1199-private-comparison-v001","candidate_revision":"c982586fbfc774614c6f4993c4e6c81d450d15be; executable sha256 4c4adf38b82511e9d2b77e7fc85f6f8fd3a4a327e15341ca65e23aecdd23bf58","fingerprint":"ef60b7926f20db034da29907f0d9e30c4d5bf2f7aeb48e4981f3083599d0d212","decision_id":"a74d529b-4aff-4662-b97a-a78d0277b255","mandate_revision":"r4-with-owner-compose-20260925-r3; coordinator tracking addendum 2026-09-26"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"830ef134b","evidence":{"kind":"command","detail":"qt_pull_request_overview_page: one row per folder that holds files, named by whole path (no Engine/Private/Deep rows), zero indentation and a single left column for folder rows and direct, deep and root files; compared point by point with the owner's quoted request on the card; native macOS and offscreen 1.0/1.25/2.0 renders inspected."}},{"criterion":"AC2","outcome":"pass","revision":"830ef134b","evidence":{"kind":"command","detail":"qt_pull_request_overview_page and application_setup_destination_public_path: accessible expandable/expanded folder state; the row click, disclosure mark and Left/Right hide only that folder's files and restore the same identities and order; focus moves to the folder without file activation; same-authority refresh keeps collapse and focus; authority clearing drops the former expansion."}},{"criterion":"AC3","outcome":"pass","revision":"830ef134b","evidence":{"kind":"command","detail":"Focused Qt course covers empty intermediate folders, direct, nested and root files, sibling folders, collapse/reopen, incremental insert/remove/reorder refresh and authority transition; the composed read-only Overview course also passes; application_active_changed_scaling passes; zero dispatches and no comparison from disclosure; renders at 1.0/1.25/2.0 and natively. Windows/Linux native evidence not captured."}},{"criterion":"AC4","outcome":"pending","revision":"830ef134b","evidence":{"kind":"freeform","detail":"Owner verdicts at hand-back 815, session cbd48b91: BPR-1213-flat-rows-v001 accepted (event 813); BPR-1213-collapse-v001 rejected (event 812): \"Impossible to tell with this PR, only one folder and no subfidrs\". Collapse needs asking again on a pull request with several folders and nested subfolders. BPR-1213-collapse-v002 (fingerprint e573cace…, session sequence 825) asks it on the constructed nested-folders-v1 pull request, frozen build of 0f6e7b292, app sha256 d1c5ed68…; pending."}}]
---

# Keep Overview folders flat and collapse only their files

## Owner clarification

On 2026-09-26, the owner supplied this unrelated Overview clarification in
[[BPR-1199]]'s review, verbatim:

> Unrelated: screenshot shows something got lost in translation on the guidance to make folders collapsable on the overview page. folders should still be flat how they were before so that folders without files underneath are a single row and files are always aligned with the left column. I just wanted to be able to hide the files. I didn't want to break out the folder hierarchy.

Source: session `2c2e72e3af2e2fd7fa1a566ec49bc5ca`, candidate
`BPR-1199-private-comparison-v001`, final Reject decision
`a74d529b-4aff-4662-b97a-a78d0277b255`, saved
2026-09-26T18:29:43.795421+00:00. The screenshot's candidate was
`c982586fbfc774614c6f4993c4e6c81d450d15be`, executable SHA-256
`4c4adf38b82511e9d2b77e7fc85f6f8fd3a4a327e15341ca65e23aecdd23bf58`,
review fingerprint `ef60b7926f20db034da29907f0d9e30c4d5bf2f7aeb48e4981f3083599d0d212`.
Only the Overview paragraph is carried here. The private-comparison diagnosis
and review attachment stay with BPR-1199 and its runtime evidence.

The desired presentation stays flat: folders that have no files directly under
them take a single path row, and every file aligns with the left column.
Disclosure hides or shows the files without breaking the path into an indented
folder hierarchy. [[BPR-0934]] implemented a nested tree from the earlier
collapse request; this later correction preserves that terminal card's history
and records the owner's clarified intent separately.

[[BPR-0972]]'s current performance work preserves the existing tree, flat,
changed-first and compact modes. Its implementer confirmed that the scaling
patch does not introduce the hierarchy. This card therefore owns the later
presentation correction; it does not make that independent scaling work or
the private comparison repair responsible for a redesign.

## Intended work and acceptance

Activate this card separately and inspect the then-integrated Overview rows and
mode choices. Preserve the owner's flat presentation while retaining the
existing file identities, changed-file metadata, selection, keyboard operation
and bounded incremental refresh. Keep Qt responsible for layout and local
expansion; no provider, Git, storage or publication effects arise from disclosure.

- AC1: The actual Overview uses the owner's flat folder presentation: no expanded indented ancestry, folder paths without intervening files remain one row, and file labels align at the same left column. The evidence explicitly compares this result with the quoted request.
- AC2: Accessible pointer and keyboard disclosure hide only the associated files and restore their identities and order. Collapse and same-authority refresh retain sensible focus/selection without an unintended file activation; authority replacement clears the former expansion state.
- AC3: Focused Qt and composed read-only fixtures cover nested paths with empty intermediate folders, direct and nested files, sibling folders, collapse/reopen, incremental refresh and mode transitions. Capture the real Overview at representative widths and fonts, retain the existing scaling bounds, and prove disclosure causes no remote mutation.
- AC4: Record the owner's judgment of the built flat rows and collapse behavior against the exact candidate, asking independent visual criteria separately. Preserve unavailable native-platform evidence without inventing acceptance or rewriting BPR-0934's historical result.

## Registration state

Backlog for a later authorized implementation round. No source, fixture, model,
mode default or RFC criterion was changed when this card was registered. The
future course uses synthetic or recorded public fixtures and read-only navigation;
this card grants no live private-PR inspection or remote mutation authority.

## Implementation — 2026-09-29

Candidate `830ef134b` (claimed at `794194371`), macOS arm64, Qt 6.11.2.
The then-integrated Overview was [[BPR-0934]]'s nested tree. It had one node
per path component (`Engine` → `Renderer`/`Shaders`), native branch
indentation, and every file one indentation step deeper per level. The
Overview has no mode choice of its own. The tree, flat, changed-first and
compact modes that [[BPR-0972]] preserves belong to the Files drawer, which
this card does not change.

The request, verbatim from above: "folders should still be flat how they were
before so that folders without files underneath are a single row and files are
always aligned with the left column. I just wanted to be able to hide the
files." The result, point by point:

- **Flat rows.** `overview_file_model::build_tree` makes one folder node per
  distinct `overview_file_row::Folder`, directly under the root and named by
  its whole path. `Engine/Renderer/Private/Deep` is one row; `Engine`,
  `Private` and `Deep` take none. A root file is under `/`. The retained
  identity, reconcile and incremental machinery is unchanged, so refreshes
  still insert, move and remove rows instead of resetting the model.
- **Files at one left column.** The view has no indentation and no root
  decoration. Every file label and every folder row starts at the same left
  x, and the test asserts it for direct, deep and root files.
- **Hide only the files.** The folder's own cell draws the style's native
  disclosure mark (`PE_IndicatorBranch`) before its path. The path is measured
  in the DemiBold it is painted in, so it is never elided. A click on the
  folder row toggles it. Left and Right collapse and expand it as before.
  Double-click expansion is off, so a double-click does not toggle twice. A
  hidden current file moves focus to its folder. No folder gesture dispatches
  a file, source or provider effect.
- **Documentation.** `docs/ux/workbench-specification.md` "Overview files" now
  describes the flat rows. It keeps the note that [[BPR-0934]] recorded the
  earlier hierarchy. That card's history is not rewritten.

### Evidence

| Criterion | Check | Result |
|---|---|---|
| AC1, AC2, AC3 | `qt_pull_request_overview_page` (rewritten `test_folder_controls_preserve_identity_and_refresh_state`; the presentation course now expects two flat rows) | Pass. One row per folder that holds files, with none for `Engine`, `Private` or `Deep`; `/` holds a root file; zero indentation and one left column for folder rows and direct, deep and root files; the longest path is unelided beside the mark; accessibility gives the path with expandable/expanded state; Left hides only its own files (a deeper folder's files stay); metadata refresh keeps collapse, focus and identity with no reset or insert; structural refresh inserts two new, expanded folders and keeps collapse; Right restores the same identities and order; clicking the disclosure mark hides the files and leaves the folder current; clicking elsewhere on the row shows them again; double-click and activation on a folder dispatch nothing; removal and reorder keep identities without a reset; authority clearing drops identities, then a new authority starts expanded without the former selection |
| Negative control | same course with the size hint in regular weight | Fails: "the longest folder path fits its cell beside the disclosure mark, unelided". The nested model fails the flat-row assertions by construction |
| AC2, AC3 (composed, read-only) | `application_setup_destination_public_path` | Pass. Real composition: the flat `/` row and its two files share one left column; Left and Right hide and restore both exact files; Overview does not choose a comparison or leave |
| AC2, AC3 | `qt_workbench_overview`, `application_pull_request_overview`, `application_overview_since_review_contract` | Pass |
| AC3 scaling bounds | `application_active_changed_scaling` (BPR-0972's 515-file course) | Pass |
| AC3 renders | `BPR_QT_RENDER_PATH` renders offscreen at 1.0, 1.25 and 2.0, plus natively on macOS cocoa (1100×700 page, system font) | Inspected: flat rows, native disclosure mark, collapsed `Engine/Tests`, unelided `Engine/Renderer/Private/Deep`, all files at one column. The first native render showed that path elided, which led to the DemiBold measurement fix |
| Gates | clang-format on touched files; clang-tidy on the page, its test and the composed test | Pass; tidy's only findings are two older lines in the composed test (950, 1160) |

`application_setup_destination_public_path` proves the no-effect part of AC3:
a disclosure selects no comparison and sends nothing. The page tests
count zero file dispatches.

Native-only failures in `qt_pull_request_overview_page` also occur at
`bd6b7a363`: under cocoa, "scrolling sizes the newly visible last file" and
"Shift+Tab leaves source for reader controls". The registered offscreen row
passes. Windows and Linux renders were not captured. Their native evidence
remains unobserved.

### AC4 — pending the owner

Two attended candidates, one question each, both Accept or Reject, are ready
in the checkout's attended-review session (`cbd48b91…`, sequence 619):

- `BPR-1213-flat-rows-v001` (fingerprint `e1f885cc…`): how the rows look,
  with the native render of nested paths attached.
- `BPR-1213-collapse-v001` (fingerprint `4dc42f1c…`): whether a folder row
  hides and shows only its files.

Both use the frozen tests-enabled app built from `830ef134b`, executable
SHA-256 `637ff8667d8c3da1c5aa76adb7ace73299752ac43322c7fd641645ad258f50be`,
in `/Users/jaeger/bpr1213-review.FuJqWc`. Its `launch.py` holds the window
open for at most 15 minutes on the recorded `google/UIforETW#74` fixture,
with HOME set to that root and no publication. The recorded fixtures have
no nested paths, so the nested case is shown by the attached render; the
fixture gives a real `UIforETW` row for the collapse. No server was started.
Next: on hand-back, record each verdict against its fingerprint. Finish on
two Accepts. On a Reject, apply the note and prepare a v002.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2, GCC 15.2, Qt 6.11.2, offscreen `QT_STYLE_OVERRIDE=Windows`,
`build/dev` at `52fa4a06`. The build needed the GCC fix recorded on
[[BPR-1214]]. `qt_pull_request_overview_page` and `qt_workbench_overview`
pass, and `application_active_changed_scaling` passes after the settle fix
recorded on [[BPR-1211]]. No render was inspected on this station, and AC4
stays with the owner. Logs are in `build/logs/bpr-station-1211-1213-1214/`,
not committed.

## Windows — bpr-main-win, 2026-09-29

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, offscreen,
`build/windows-dev` Debug with warnings as errors. `main` at `22d18416`
did not build on Windows. The first run is at `22d18416` with the build
fixes of `537f718d`. The second is at `f9870baa`, which contains the Linux
station's fix `52fa4a06`. Neither build fix touches this card's code.

| Row | At `22d18416` + `537f718d` | At `f9870baa` |
|---|---|---|
| `qt_pull_request_overview_page` | Pass, 17.22 s | Pass, 17.02 s |
| `qt_workbench_overview` | Pass, 3.73 s | Pass, 3.63 s |

Both ran offscreen. No native Windows render was captured, so AC3's
Windows native evidence is still not captured. AC4 still waits on the
owner, and the card stays Parked. The logs are station records in
`C:\Users\mikem\bpr-main-helpers\run\station-0929\` (`new.log`, `pass2.log`), and are not committed.

## Owner verdicts at hand-back 815 — 2026-10-02

The owner decided both AC4 items in session
`cbd48b91000b00f1e72e1ad62c7c384c` and handed back at event 815. Both were
asked on the `830ef134b` build (app SHA-256 `637ff866…50be`):

| Candidate | Event | Verdict |
|---|---|---|
| `BPR-1213-flat-rows-v001` | 813 | Accept |
| `BPR-1213-collapse-v001` | 812 | Reject |

The rejection, verbatim: "Impossible to tell with this PR, only one folder
and no subfidrs". The owner could not judge collapse on the review pull
request, which has one folder and no subfolders. The rejection is about the
material the item was asked on, not the behaviour.

AC4 stays pending on the collapse question. The next step is a separate
milestone: ask `collapse-v002` on a pull request with several folders and
nested subfolders. The flat-rows verdict stands. Both candidates are still
protected for that milestone. The card stays Parked.

## Collapse asked again on nested folders — 2026-10-02

No existing sanitized fixture has nested folders: `uiforetw-74` has one,
`large-review-v1` has four flat ones, and the other recordings have fewer.
`0f6e7b292` adds `nested-folders-v1`, a constructed pull request
(`bpr-fixtures/nested-folders#21`) with 17 files in 12 folder rows, folders
nested three deep (`src`, `src/engine`, `src/engine/shaders`,
`src/engine/shaders/post`), intermediate folders with no file of their own
(`src/ui`, `tests/ui`, `tools`, `tools/build`) and a root file.
`scripts/build_nested_folders_fixture.py` writes its construction, and the
existing pack and contract builders make its `objects.pack` and served tree.
It is served as `--github-fixture nested-folders-v1`; see
`tests/fixtures/github/pull-requests/nested-folders-v1/README.md`.

No collapse defect showed up with nested folders, so the Overview code did
not change. A folder row hides only the files directly in it. A deeper
folder is its own row and stays, with its files.

| Check | Result |
|---|---|
| `application_nested_overview_folders_public_path` (new, composed) | Pass. Opens the pull request on Overview. Rows are `/`, `docs`, `docs/api`, `docs/api/v2`, `src`, `src/engine`, `src/engine/shaders`, `src/engine/shaders/post`, `src/ui/widgets`, `tests/engine`, `tests/ui/widgets`, `tools/build/scripts`, all expanded, and all 17 files share the folder rows' left column. A click on `src` hides only `src/main.cpp`. Left on `src/engine/shaders` hides only its two files, keeps it current, and leaves `post` shown. Right and a second click restore the same files in order. No file selection, no comparison, still Overview. The collapsed and expanded offscreen captures were inspected |
| `application_github_public_path_nested_folders_v1` (new) | Pass. `PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:nested-folders-v1`, `RENDER_OK` |
| `github_recorded_fixture_contract`, `github_contract_fixture_security` | Pass. Negative control: one hand-edited path in the served `files.json` fails the byte comparison |
| `test_network_isolation_structure_contract`, `test_network_isolation_manifest` | Pass with both new rows pinned `dos-volume-name`. The Windows pin is by analogy with the `uiforetw-74` and selected-file-safety rows, not measured |
| Frozen app, native cocoa | `PUBLIC_PATH_OK` in 2.0 s. The native render is attached to the question |
| clang-format; documentation gate | Pass; `DOCUMENTATION_OK` |

`BPR-1213-collapse-v001` was released by candidate name. Its Reject stays
recorded. `BPR-1213-collapse-v002` (fingerprint `e573cace…`) is ready in session
`cbd48b91…` at sequence 825 and asks one Accept/Reject question. The owner opens
`https://github.com/bpr-fixtures/nested-folders/pull/21`, clicks `src`
(only `main.cpp` hides), clicks or presses Left on `src/engine/shaders`
(only `basic.frag` and `basic.vert` hide, and `post` stays), and restores
both. The frozen tests-enabled app is
`/Users/jaeger/bpr1213-review-v002.tXA1Io/BetterPullReview-bpr1213-v002.app`,
built from `0f6e7b292`, executable SHA-256
`d1c5ed68e9bb0fc28371712776b0be42b3fe889535a95727fed9011d98544eee`. It reads
its fixture from this checkout. Its `launch.py` sets HOME and
CFFIXED_USER_HOME to the root's `home` and closes the window after 15
minutes. `flat-rows-v001` keeps its Accept and protection.

Next: on hand-back, record the v002 verdict against its fingerprint. On
Accept, AC4 is complete. Finish the card with Windows and Linux native renders
recorded as not captured. On Reject, apply the note.

The push rebased `0f6e7b292` onto main as `5b7eddb83`. The only commit in
between, `f21072628`, edits one BPR-1157 card. `git diff 0f6e7b292 5b7eddb83 --
src tests scripts CMakeLists.txt cmake` is empty, so the frozen build is
also `5b7eddb83`'s. The review item keeps the name `0f6e7b292` that its
fingerprint binds.
