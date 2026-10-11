# BPR-1384 — visible Discuss comment actions

Owner amendment, 2026-10-10: “I saw neither a button, nor a right click menu action for this.” BPR-1383 had just been completed before this steering arrived. Done is terminal, so this follow-up records and completes the amended discoverability obligation without reopening or altering the prior accepted plan. The implementation and evidence for [[BPR-1383]] remain applicable to its exact thread/file pending pick.

Engineering owner `/root/discuss_inline`, nondelegating leaf, strategy coordinator-implementer recipe revision 2. Mandate revision `owner-20261010-discuss-inline-discoverability`. Filed at `4e643cc90`, claimed at `09fd3c160`, implemented at `5c1e264cc`. The authored follow-up plan is `docs/rfcs/plans/bpr1384-discuss-comment-affordance/milestone-plan.json`; `selection.json` preserves the selected strategy. The exact candidate revision and file digests are in `source-hashes.json`. Shared HEAD subsequently advanced through unrelated BPR-1385 tracker commits only; the logical checkpoint observes that repository revision, while engineering acceptance stays bound to `5c1e264cc`.

## Findings and resulting behavior

The open-thread header's old button read only filename:line and looked like a location label. Individual comment and reply bodies use the Markdown reader's own native context handling; that event did not open the separate thread menu. The owner's observation exposed a real discoverability gap, beyond BPR-1383's route repair.

The thread header now visibly says **Open in Review**, with filename:line beside it and the full path in its tooltip. Collapsed notes and open thread headers retain their thread menu, which opens the first post. Right-clicking an individual comment or reply body, in rendered or source view, now offers **Open in Review** and preserves that exact post identity. The existing route opens its file and Comments in Review and selects the clicked reply beside its source line.

First posts and replies of current inline threads are eligible. PR-wide comments and unavailable anchors show a disabled body-menu action with a reason; draft bodies do not install it. Review's own comment reader keeps its default native menu. The reader's new menu composition hook stays in Qt, as do the native controls; no provider, persistence or effect authority changed.

UX work used `/Users/jaeger/Documents/GitHub/AgentSkills/skills/frontend-design/SKILL.md` and its desktop guidance, loaded before the original work and retained through this amendment. The chosen controls use the native button, menu, typography and palette.

## Acceptance evidence

- AC1: card tests require the visible “Open in Review” text, strong keyboard focus, adjacent filename:line and full-path tooltip. The composed Discuss render confirms this on the actual application.
- AC2: card tests send actual context events to each post's rendered and source viewport, using mouse and keyboard context reasons, and verify the exact first-post and reply identity dispatched. The full application course right-clicks the real reply body and waits for that exact post to be selected after its Git/cache comparison is ready.
- AC3: card tests cover disabled PR-wide and missing-current-line body actions with explanations and the unchanged default Review reader menu. The body hook excludes drafts; existing draft/composer courses also pass in the full card/workbench suites.
- AC4: fail-before/pass-after on the full application course; focused card, reader, workbench and composition checks, offscreen rendering, formatting and documentation evidence below.

Fail-before, at `09fd3c160` with only the revised public test: `BPR_SETUP_ARTIFACT_ROOT=/tmp/bpr-discuss-inline/bpr1384-before ctest --test-dir build/bpr1384-discuss-comment-affordance -R '^application_diff_zoom_public_path$' --output-on-failure` fails “right-clicking the reply opens the reader's context menu”. The before render shows only `conversation_cards_view.cpp:38` in the header. A first compile rejected `findChildren<markdown_body_view*>` because that class has no Q_OBJECT; the course now finds native QWidget children and uses RTTI to identify the reader.

Pass-after, macOS Debug, Qt offscreen, isolated task-owned build:

```sh
cmake --preset dev -B build/bpr1384-discuss-comment-affordance
cmake --build build/bpr1384-discuss-comment-affordance --target test_diff_zoom_public_path test_better_pull_review_qt_conversation_cards_view_tests test_better_pull_review_qt_workbench_window_tests -j 4
cmake --build build/bpr1384-discuss-comment-affordance --target test_better_pull_review_markdown_body_view_tests -j 4
BPR_SETUP_ARTIFACT_ROOT=/tmp/bpr-discuss-inline/bpr1384-after ctest --test-dir build/bpr1384-discuss-comment-affordance -R '^(application_diff_zoom_public_path|qt_conversation_cards_view|qt_workbench_conversation_reveal)$' --output-on-failure
ctest --test-dir build/bpr1384-discuss-comment-affordance -R '^(qt_markdown_body_(1_0|1_25|2_0)|qt_workbench_window_scale_100|architecture_boundaries|application_workbench_composition_contract)$' --output-on-failure
ctest --test-dir build/bpr1384-discuss-comment-affordance -R '^qt_conversation_cards_view$' --output-on-failure
python3 _coding-standard/scripts/format.py --dry-run src/adapters/qt/include/bpr/qt/markdown_body_view.hpp src/adapters/qt/src/markdown_body_view.cpp src/adapters/qt/src/conversation_open_card.hpp src/adapters/qt/src/conversation_open_card.cpp src/adapters/qt/src/conversation_cards_view.cpp tests/adapters_qt/conversation_cards_view_tests.cpp tests/end_to_end/diff_zoom_public_path_tests.cpp
git diff --check
python3 scripts/verify_documentation.py --root .
```

Results: 3/3 focused checks pass; 6/6 integration checks pass, including Markdown readers at 100%, 125% and 200% scale. After adding the actual collapsed-note context event, the card suite rebuilt and passed again. Clang-format 22.1.8 and whitespace check pass. The documentation gate prints `DOCUMENTATION_OK`; its unchanged 67-test script suite passed during BPR-1383 and is reused.

Agent inspected three actual composed renders under `/tmp/bpr-discuss-inline/bpr1384-after/`: `discuss-review-button.png` shows the explicit button and adjacent file:line; `discuss-reply-context-menu.png` shows the reply's native text/source controls plus Open in Review; `discuss-open-in-review.png` shows Review, Comments and the clicked reply highlighted beside line 38. These are local inspection artifacts, not committed product assets. Offscreen evidence does not claim native appearance, screen-reader or other-platform acceptance. Existing broader RFC native/human criteria remain separate; this bounded follow-up has no outstanding human criterion.

## Continuation and command state

`checkpoint.json` binds the authored milestone and mandate; `observation.json` records passing obligations and exit criteria, exact candidate and quiescence, with `checkpoint-result.json` retaining the helper's logical completion check. Self-review confirmed native text/source actions remain present, eligibility reuses the thread menu's existing anchor predicate, and an exact post callback is copied before navigation can replace its card. Qt-owned receivers disconnect on destruction; the shared locator validates the current post identity.

The task-owned build is removed after verification. No review server, protected candidate, background command or task-owned repository writer remains at handoff. Shared `build/dev`, unrelated work and `docs/proposals/` are preserved. No live desktop capture/control, browser-profile reading or remote mutation was used. Required pending obligations: none. Next action: report BPR-1383 and BPR-1384 complete under the original request and the owner's discoverability amendment; no RFC closeout is asserted.
