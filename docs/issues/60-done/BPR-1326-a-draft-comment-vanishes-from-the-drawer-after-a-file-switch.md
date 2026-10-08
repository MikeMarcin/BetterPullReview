---
schema: "implement-rfc.card.v1"
id: "BPR-1326"
title: "A draft comment vanishes from the drawer after a file switch until its line is clicked"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"owner, chat 2026-10-08, on the 0.1.5 prerelease or main","reservation":"docs/issues/_reservations/BPR-1326--BPR-1326--f35928231211.json","related_cards":["BPR-1085","BPR-1290","BPR-1293","BPR-1303","BPR-1305"]}
acceptance: [{"criterion":"BPR-1326:reproduce","outcome":"pass","revision":"4181ca720","evidence":{"kind":"command","command":"better_pull_review --github-fixture conventional-comments-v1 --published-file-switch-probe --published-file-switch-timing draft (offscreen, network-isolated runner), with the saved drafts withheld","detail":"Before leaving renderer.cpp the draft showed as a note, a gutter marker and a scroll-bar tick on line 1. After renderer.h and back: kept=1 note=0 drawn=0 marker=0 tick=0, selection on line 6 (the first change). A refresh after that did not bring it back (refresh-restored=0). Published comments were unaffected in all three timings."}},{"criterion":"BPR-1326:cause","outcome":"pass","revision":"4181ca720","evidence":{"kind":"observation","path":"src/adapters/qt/src/conversation_cards_view.cpp","detail":"The drawer drew one draft note, from review_draft_editor::shown_draft(), which is the selected line's draft. The drafts themselves stay in the shared draft host and the private ledger. A file shown again selects its first change, so the editor holds that line's empty draft and the written draft has no note, marker or tick until its line is chosen again."}},{"criterion":"BPR-1326:fix-and-control","outcome":"pass","revision":"4181ca720","evidence":{"kind":"command","command":"BPR_1326_CONTROL=1 ctest --test-dir build/dev -R '^application_published_file_switch_contract$' (3 runs; temporary switch that withheld the saved drafts, removed)","detail":"3 of 3 failed in the draft course with PUBLISHED_FILE_SWITCH_EVIDENCE:11110 and kept=1 note=0 marker=0 tick=0. The same binary without the switch passed."}},{"criterion":"BPR-1326:stable","outcome":"pass","revision":"4181ca720","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^application_published_file_switch_contract$' --repeat until-fail:10","detail":"10 of 10 passes on the final candidate (three courses per run), macOS arm64 Debug, offscreen."}},{"criterion":"BPR-1326:neighbours","outcome":"pass","revision":"4181ca720","evidence":{"kind":"command","command":"ctest --test-dir build/dev -j1 -R '<13 neighbouring rows>'","detail":"13 of 13 passed: architecture_boundaries, test_network_isolation_manifest, application_shared_draft_application_host, application_document_tab_host, qt_conversation_cards_view (new saved-draft case with a negative control), qt_workbench_conversation_reveal, qt_review_draft_editor, application_large_review_benchmark, application_review_round_trip_composition_contract (resealed, mutation pin unchanged), application_review_behaviour_contract, application_threaded_reply_contract, application_label_authoring_contract (3 of 3) and application_published_file_switch_contract."}}]
---

# BPR-1326: A draft comment vanishes from the drawer after a file switch

## Defect

The owner, 2026-10-08:

> I published a draft comment, went to a different file, came back and it
> went away

and then:

> The draft comment still exists it just doesn't show up in the drawer
> unless you actually click on the line

## Outcome

Reproduce the sequence on a fixture through the composed application, find
the cause, fix it, keep the course as a control that fails without the fix,
and run the neighbouring rows. Offscreen only.

## Acceptance

- `BPR-1326:reproduce`: the owner's sequence, and whether the comment is gone
  from the model or only not drawn, and whether a refresh brings it back.
- `BPR-1326:cause`: the cause, with the facts that show it.
- `BPR-1326:fix-and-control`: the fix, and a control that fails without it.
- `BPR-1326:stable`: 10 consecutive passes of the course.
- `BPR-1326:neighbours`: label authoring, review behaviour, threaded reply,
  the inline drawer and the round-trip pins.

## Reproduction

`application_published_file_switch_contract` drives
`--published-file-switch-probe` on `conventional-comments-v1`. In Review it
opens `src/engine/renderer.cpp`, writes a comment on the first line no thread
is on (line 1), opens `src/engine/renderer.h`, opens `renderer.cpp` again and
checks the comment: kept (in the accepted pull request, or still saved as my
draft), its note in the drawer (measured, placed at its line, drawer shown),
its gutter marker and its scroll-bar tick. Three courses:

| Course | What it writes | When it leaves | Before the fix |
|---|---|---|---|
| `settled` | publishes | once the thread shows | passes |
| `late` | publishes | at once; the application's own refresh answers only once `renderer.h` shows, as a slow server's does | passes |
| `draft` | keeps it as my draft | once the draft note shows | fails: `kept=1 note=0 drawn=0 marker=0 tick=0` |

A published thread comes from the accepted pull request and showed again in
every timing. The draft is the owner's case: it is still saved, it is only not
drawn, and a refresh does not draw it (`refresh-restored=0`). Choosing its
line again loads it into the editor and draws it, as the owner found.

## Cause

The drawer drew one draft note, from `review_draft_editor::shown_draft()`
(`conversation_cards_view::set_draft`, fed by
`workbench_window::refresh_cards`). That is the selected line's draft. Every
written draft stays in the window's `shared_draft_application_host` and, with
private drafts, the ledger. When `renderer.cpp` showed again, its selection
was its first change (line 6), the editor held that line's empty draft, and
nothing drew the draft on line 1. By the same path, choosing another line in
the same file hid it too (read from the code; the course does not exercise
that).

None of the listed suspects is the cause. BPR-1303's re-show, BPR-1293's
Review after Publish, BPR-1290's per-file notes and BPR-1305's kept reading
all leave a published thread drawn after the switch in this fixture.

## Fix

- `github_application.cpp`: `present_saved_drafts()` hands the window every
  unpublished draft the attached shared draft host holds
  (`recovery_snapshot()`, read only), after each accepted draft, each ledger
  adoption, attach and retirement.
- `workbench_window::present_saved_drafts` and
  `conversation_cards_view::set_saved_drafts`: each saved draft on the shown
  comparison is a dashed note at its line, with its gutter marker and tick,
  whichever line is selected. The editor's draft is still drawn as the editor
  holds it and is never drawn twice. Empty, reply and published drafts draw
  nothing.
- Publish, Edit and Delete on another line's saved draft choose that line
  first and act on nothing else, so they can never act on the editor's draft
  by mistake.
- `application_label_authoring_contract`'s refused-patch check skips draft
  notes. The kept text is now drawn as my draft, and no thread holds it, which
  is what that check asks.
- `application_review_round_trip_composition_contract` is resealed over the
  BPR-1090 reseal; the mutation pin is unchanged.

## Evidence

macOS arm64 Debug, `build/dev`, in a worktree off `origin/main` `02d869dc0`,
using the main checkout's CPM cache. Every run was offscreen and
network-isolated. Nothing drove or captured the desktop.

| Run | Result |
|---|---|
| Draft course, saved drafts withheld (temporary switch, removed), 3 runs | 3 failed, `PUBLISHED_FILE_SWITCH_EVIDENCE:11110`, `kept=1 note=0 marker=0 tick=0` |
| Candidate, contract `--repeat until-fail:10` | 10 of 10 passed |
| Candidate, 13 neighbouring rows serially | 13 passed |
| `qt_conversation_cards_view` | saved-draft case passes, with a negative control |

## Workaround on 0.1.5

Nothing is lost. Click the draft's line and its note comes back. Refresh does
not help. Published comments are not affected.

## Left alone

- `qt_composed_inline_drawer_scale_{100,125,200}` fail on this Mac ("each
  post's body fits its width and shows all of its lines", then a crash) with
  this change's Qt sources reverted too, so this change does not cause them.
  No card was filed; the ID budget was one.
- `application_composition_contract` did not finish here: its nested Release
  build ran out of disk (1.8 GiB free). Its mutation-inventory case had failed
  only on the stale pin, which is now resealed.
- Publishing and then choosing another file before that file's diff shows can
  return the window to the published thread's file when the refresh lands
  (BPR-1293's pick). On a live server the local diff usually shows first,
  which cancels the pick. Not changed here.
- Linux and Windows did not run these rows. The change is shared Qt and
  composition logic.
