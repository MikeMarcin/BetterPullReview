---
schema: "implement-rfc.card.v1"
id: "BPR-1214"
title: "Restore conversation content after Discard and Back"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"Implementer native inspection during owner hand-back32 follow-up, 2026-09-26","related_rfcs":["RFC-BPR-0052"],"related_cards":["BPR-1207","BPR-1208"],"reservation":"docs/issues/_reservations/BPR-1214--BPR-1214--2caaece7bc57.json","mandate_revision":"owner-20260926-unattended-worktrees","scope":"Separate return-path projection defect; not new owner acceptance for the repaired editor or accepted thread controls"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"bf7a96367","evidence":{"kind":"observation","detail":"Reproduced on the composed github-com fixture (--threaded-reply-probe): with the old page rule the course fails at threadless-line-hid-drawer, and after Discard/Back at comments-drawer-blank-after-discard-and-back. Transition located: github_conversation_panel::show_loaded_page chose the empty page when a threadless anchor line emptied the filtered detail, hiding cards, scope, filters, map and detail."}},{"criterion":"AC2","outcome":"pass","revision":"bf7a96367","evidence":{"kind":"command","detail":"Page now follows the retained snapshot; only empty map/detail hide. qt_workbench_conversation_reveal: cards restored after Discard and Back without another selection, one discard and no publish, thread line still filters with map/detail, empty snapshot keeps empty page. Composed course: zero submissions/replies at Back. Selection, authority and reveal code unchanged."}},{"criterion":"AC3","outcome":"pass","revision":"bf7a96367","evidence":{"kind":"command","detail":"qt_workbench_conversation_reveal and application_threaded_reply_contract (github-com, ghes-3.18) pass; both fail with the old rule (negative controls). Native macOS cocoa: composed probe PUBLIC_PATH_OK/THREADED_REPLY_OK and inspected render of the drawer after Discard/Back. Windows/Linux native runs not performed; pre-existing unrelated reds listed on the card."}}]
---

# Restore conversation content after Discard and Back

## Observed route

On macOS, source `c777a33d7467e83cfd0d2217329953e2044f1d41`, executable
SHA-256 `8589abf9dca824127e1531aa26699c9ee9d0456626fdca090c04eacf217b50ba`:

1. Seed the synthetic github-com defect course directly at its fixed runtime
   directory, then launch that same directory with epoch phase reuse and
   attended-launch. Connect the fixture account, open the synthetic PR and
   start Review.
2. Select a right/current-head source line, open Comment, and type local text.
   The editor is editable and Publish is available. Do not publish.
3. Click Discard, then Back. The comparison returns, but the Comments drawer
   is blank below its thread-availability text: cards, filters, map and detail
   are absent. Switching the source-line selection does not restore it.
4. Click Next defect. The drawer content returns and the comparison reveals
   the selected thread's exact line 60. No data loss or provider mutation was
   observed; persistence and the underlying cause have not been diagnosed.

The agent inspected the actual native screenshot and accessibility state. Its
private observation record is
`/Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-r3-rb0nbkl2/evidence/native-final-observations.json`.
That runtime's `review-seed-command.json` gives the exact seed argv and
`launch-review.py` gives the reuse argv. The proof app exited normally. The
older owner's r2 app and fixture were untouched.

## Scope and acceptance

This is a separate return/projection transition. BPR-1207's entry to an editable
Comment page and BPR-1208's accepted thread-command placement retain their
recorded results. Neither card claims this return path fixed. Activate this
card separately; do not infer a cause or widen authority from the observation.

- AC1 Reproduce Discard then Back on the public composed fixture and locate
  the state/projection transition that hides the existing conversations.
- AC2 Returning from an inline comment restores the Comments content permitted
  by the current conversation snapshot without requiring another selection.
  Preserve thread selection, current authority and exact code-reveal policy;
  a draft discard must not publish or erase the conversations.
- AC3 A focused public-path regression covers the reproduced sequence and
  recovery, with a negative control; inspect the actual native result. Keep
  unavailable platform evidence explicit and reuse unchanged command/placement
  checks rather than introducing a new owner approval checkpoint.

## Diagnosis and repair — 2026-09-29

Candidate `bf7a96367`, macOS arm64, Qt 6.11.2, dev preset.

**AC1.** Discard and Back are incidental. The transition that hides the
conversations is `github_conversation_panel::show_loaded_page`. Selecting a
current-head line that no thread is on sets the panel's anchor line.
`refresh_projection` filters the detail to that line's threads, which is none,
and `HasThreads` became false. The panel then showed its empty page. The cards,
the scope buttons, the filters, the map and the detail all live on the loaded
page, so the drawer went blank below the availability text. Comment needs such
a line, so the reviewer reached this state before opening Comment, and was
still in it after Discard and Back. Selecting another threadless line kept the
drawer blank. Next defect selected a thread and so restored it. That is the
native observation above. The composed course reproduced it: without the
repair, `--threaded-reply-probe` fails with
`failed=threadless-line-hid-drawer`. With the line check removed, it fails
after Back with
`failed=comments-drawer-blank-after-discard-and-back page=0 submissions=0 replies=0`.

**AC2.** The page now follows the conversation snapshot: the loaded page is
shown while the snapshot retains any conversation, and the empty page only for
an empty snapshot. The map and detail panes, which have nothing to show for a
threadless line, are hidden until the anchor shows a thread. Selection
restoration, anchor filtering, endpoint placement and code-reveal policy are
unchanged, because only the page choice moved. Discard sends one `discard`
command and no `begin_publish`. The composed course observes zero submissions
and zero replies at Back, and the conversations are unchanged.

**AC3.** Checks:

| Check | Result |
|---|---|
| `qt_workbench_conversation_reveal` with new `test_discard_and_back_keep_the_comments_drawer` | Pass. Production window: threadless line keeps the thread card with the map and detail hidden; Comment opens the editable page; Discard, Back shows the card without another selection; one discard, no publish; the thread's line shows the map and detail; an empty snapshot keeps the empty page |
| Same course with the old `show_loaded_page` | Fails: `a line no thread is on keeps the cards; only the empty map and detail step aside` |
| `application_threaded_reply_contract` (github-com, ghes-3.18) | Pass. The real composition, the fixture host and the draft discard now run Review, a threadless right line, Comment, typed text, Discard, Back, then the drawer check, before the existing reply/resolve course (`THREADED_REPLY_EVIDENCE:11111`) |
| Composed course with the old page rule | Fails at the line check, and at the Back check when the line check is removed (above) |
| Native: the app under the cocoa platform, `--github-fixture github-com --threaded-reply-probe` | `PUBLIC_PATH_OK`, `THREADED_REPLY_OK`, same phases as offscreen |
| Native: the reveal course under cocoa with `BPR_QT_RENDER_PATH` | Render inspected: after Discard and Back the Comments tab shows This file / All PR conversations, Unread / Unresolved / Clear and the thread card with Reply. The harness has no host, so its draft card remains |
| `qt_github_conversation_panel`, `qt_conversation_cards_view`, `qt_dif_workshop_placement`, `qt_shared_snapshot_navigation`, `qt_workbench_overview`, `architecture_boundaries`, `architecture_boundary_contract`, `core_dependency_boundary`, `core_boundary_contract`, `live_mutation_isolation_contract`, `application_review_ledger_composition_contract`, `application_workbench_composition_contract` | Pass |
| clang-format on the four touched files; clang-tidy on the panel, composition and test TUs | Pass; no finding on changed lines (the one found, a lambda name, was fixed) |

Failures that exist without this change, confirmed by rebuilding with the old
panel: `qt_composed_github_scale_100` ("the agent launcher, the side chat, and
the finding queue stack down one visible column") and
`qt_workbench_window_scale_100` ("no Notes tab control is clipped at 200
pixels"). `application_defect_composition_contract` stalls with
`DEFECT_SHARED_READY_STALLED`. `live_mutation_isolation` cannot read
`scoped_writing_agent`, because `test_scoped_writing_agent` compiles as C++98
and does not build. `application_review_round_trip_composition_contract` was
already failing at `bd6b7a363`: its github_application.cpp digest still pins
`aa8826e5a`, and `27dbeef51`, `637cd41df` and `eca094604` changed that file
without a reseal. Its asynchronous mutation-host literal also no longer matches
the column alignment of `ReviewMutationEffects`. This card adds only
BPR_BUILD_TESTING probe phases to that file (new LF digest
`063943ea7dfaa3bf0bb17630613954d9ff24c246c6a21b9a0edb6b4e5808e4b2`). It does
not reseal the other lane's unreviewed changes. `application_composition_contract`
reports the same drift and also finds `github_public_path_fixture.cpp` in a
release build tree.

Documentation: no impact. No product, UX or architecture document describes
the panel's empty page. The change is a presentation fix inside the Qt adapter.
Windows and Linux were not run. Both courses are portable, and native runs on
those platforms remain unobserved.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2, GCC 15.2, Qt 6.11.2, offscreen, `build/dev`.

- **Build.** At `22d18416` the Qt workbench test source does not build with
  GCC. `workbench_window_tests.cpp:7557`, from BPR-1134 (`2770ebb1`), copies a
  structured binding in a range-for, and GCC's `-Wrange-loop-construct` is an
  error under `-Werror`. Apple clang does not warn. `52fa4a06` binds it by
  reference.
- `qt_workbench_conversation_reveal`: pass.
- `application_threaded_reply_contract`: passed at first, then failed 1 of
  about 22 runs with `failed=refresh-unavailable` at phase 7. At that moment
  the reply was published (`replies=1`) and the status read `Refreshing the
  pull request.`: a refresh the application started after publishing still
  held the refresh action, and the probe failed instead of waiting. That is a
  race in the probe (`8651e1e1`), not in this card's page rule. In
  `52fa4a06` the probe waits while the action is disabled, and the run budget
  still bounds the wait. After the fix: 15 of 15 pass alone, and all six
  focused rows pass together.

The logs are station records in `build/logs/bpr-station-1211-1213-1214/` and
are not committed.

## Windows — bpr-main-win, 2026-09-29

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, offscreen,
`build/windows-dev` Debug with warnings as errors. `main` at `22d18416`
did not build on Windows. The first run is at `22d18416` with the build
fixes of `537f718d`. The second is at `f9870baa`, which contains the Linux
station's fix `52fa4a06`. Neither build fix touches this card's code.

| Row | At `22d18416` + `537f718d` | At `f9870baa` |
|---|---|---|
| `qt_workbench_conversation_reveal` | Pass, 15.43 s | Pass, 15.19 s |
| `application_threaded_reply_contract` | Pass, 14.20 s | Pass, 16.24 s |

Both ran offscreen. No native Windows run was made. The logs are station
records in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` (`new.log`, `pass2.log`), and are not committed.
