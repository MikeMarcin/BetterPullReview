---
schema: "implement-rfc.card.v1"
id: "BPR-1597"
title: "Settings by task: Home & queue, Review and Comments replace Reviews & queue"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"owner-decision","found_by":"Owner pick in attended-review session 0e5dbaff (decision 3229df02), 2026-10-17, and the owner's amendment naming the middle page Review","governing_rfc":"RFC-BPR-0048","related_rfcs":["RFC-BPR-0018"],"amendment":"RFC-BPR-0048 owner amendment 3 (2026-10-17)","related_cards":["BPR-1088","BPR-1289","BPR-1566","BPR-1568","BPR-1584"],"reservation":"docs/issues/_reservations/BPR-1597--BPR-1597--2851f28bd65b.json","acceptance":["BPR-1597:pages","BPR-1597:workshop","BPR-1597:search","BPR-1597:kept","BPR-1597:gates","BPR-1597:owner"]}
acceptance: [{"criterion":"BPR-1597:pages","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"Ten Settings pages, per-group resets and all selected scale checks pass on the merged tree."}},{"criterion":"BPR-1597:workshop","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"All six composed workshop courses pass; guidance is in Comments/Comment workshop and preserves its typed owner."}},{"criterion":"BPR-1597:search","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"Four Settings scale rows pass, including moved-row search and Return without changing values."}},{"criterion":"BPR-1597:kept","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"No schema/store/key/src-app change; main schema23 and exact composition pins retained. Behaviour/tour courses pass."}},{"criterion":"BPR-1597:gates","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"All 28 focused tests pass; format22/docs/diff pass; unchanged tidy evidence reused. Owner handoff schedules single full suite and repository tidy at item9."}},{"criterion":"BPR-1597:owner","outcome":"pass","revision":"c6c4eeff36e4d243c0f33c3a86cc7bcfe294ccc8","evidence":{"kind":"observation","detail":"Human Accept cec0a456-a98a-4d02-8eda-45507c5c5458 for frozen-956172a57-v001 confirmed through event6."}}]
---

# BPR-1597: Settings by task: Home & queue, Review and Comments replace Reviews & queue

## Owner decisions

- BPR-1568 v002 (a Comment workshop group on Reviews & queue) was rejected in
  session fbb0fdf5 (decision 97a50021), verbatim: "I think reviews & queues is
  too much of a big grab bag of settings and we should find a better
  organization."
- From three Settings organisations shown as mockups in Light and Dark, the
  owner picked **A, by task** in session 0e5dbaff (decision 3229df02), with no
  note. Then, through the coordinator: name the middle page **Review**, not
  Reviewing.
- This card folds in [[BPR-1568]] (the label helper's move) and closes it.
  Recorded as RFC-BPR-0048 owner amendment 3.

## Design

Designed with the `frontend-design` skill in the existing Settings row grammar.

- **Sidebar:** General · Accounts & hosts · Repositories · Home & queue ·
  Review · Comments · Appearance · Notifications · Agents & context · Storage &
  privacy. Repositories stays right after Accounts & hosts (owner direction
  2026-10-07); the mockup had shown it after Home & queue, and the final
  question says so.
- **Home & queue** (Reviews & queue's remainder): queue defaults, observer
  rules, the way to Repositories. Home's Filter menu says "Open Home & queue".
- **Review:** Opening (A pull request opens in; On return, compare), Reading
  (Mark a file read; Comments drawer opens on; and BPR-1584's Reading width
  once it lands), Layout (Drawer controls; Line details; Center ribbon width).
- **Comments:** Labels (About Conventional Comments; Label marks; Overview
  counts; Mark colours; Overview chips), Comment workshop (Feedback guidance:
  "Send with comment workshop requests" with an info glyph; Guidance text: a
  link to Agents & context › Review feedback), Descriptions and comments
  (Images).
- Every group's Reset covers its own rows. The writing hooks panel keeps only
  its On/Off line, naming Comments › Comment workshop; Restore default there
  restores only the text.
- Stored preferences and keys are unchanged. The `settings_page` value is the
  sidebar row and is not stored.

## Acceptance

- **BPR-1597:pages** The sidebar lists the ten pages in order; each page's
  title is its row; Review, Comments and Home & queue host their rows; each
  group's Reset covers its rows (negative control: the old page title fails the
  sidebar check).
- **BPR-1597:workshop** The switch sits on Comments › Comment workshop only,
  follows the published record, writes the stored value, is named for
  assistive technology; Guidance text opens the Review feedback hook; the
  composed courses still carry the guidance only while it is on.
- **BPR-1597:search** Settings search finds every moved row on its new page and
  section, and nothing under Reviews & queue; Return opens the row's page and
  changes nothing.
- **BPR-1597:kept** No store, schema or key change; `bpr tour hooks` unchanged.
- **BPR-1597:gates** Focused rows, clang-format 22, clang-tidy, the
  documentation gate and one full dev ctest before landing.
- **BPR-1597:owner** The owner accepts the built Settings in Light and Dark.

## Evidence

Automated criteria pass (front matter); clang-format 22 and clang-tidy are clean
on the changed sources and the documentation gate passes. The branch merges
`bpr-1581-thread-navigation` (BPR-1581/1582/1584, accepted, not yet on main) so
Reading width sits in Review › Reading; BPR-1597 lands only after it.

Full dev ctest on 956172a57: 767/775. live_mutation_isolation and
application_agent_materialization_crash_recovery pass alone.
qt_github_conversation_panel ("unavailable exact endpoints never offer Open in
code") and qt_profile_management_window ("Find, file and change navigation …
on the window's keys") fail alone too; they test sources only the merged
BPR-1581 branch changes (conversation_cards_view.cpp, desktop_menu_bar.cpp).
application_large_review_pathological, application_large_review_history,
application_epoch_ordinary_update_recreated and application_private_review_retry
failed again alone at load about 90 and are rerun at landing.

The owner's look is pending in attended-review session
a090236dc78e408d1bd6fd6752c2dfbe (state
~/.mask/attended-review-sessions/bpr-1597-v001-20261017), candidate
BPR-1597-settings-by-task-v001, revision frozen-956172a57-v001, frozen build
/Users/jaeger/bpr-1597-v001.mu3ytfkc (executable sha256 bff218187f35...).

Next: on Accept, wait for BPR-1581 on main, merge main, run one full dev ctest on
that tree, record the verdict, finish this card, close BPR-1568 into it, land.

## Codex closeout — 2026-10-17

Owner **Accept**, no note, decision `cec0a456-a98a-4d02-8eda-45507c5c5458`,
for `BPR-1597-settings-by-task-v001` in session `a090236d…`; handback consumed
through event 6. This accepts the built ten-page Settings and folds BPR-1568
into Comments › Comment workshop. Its earlier rejected versions remain rejected.

Main `c38432adf` merged at `c6c4eeff3`. Reconciled only two conflicts: the
settings inventory retains the new Review row and main's Status drawer row;
the composition contract retains both histories and main's exact pins. No
schema step is added; main's schema 23 remains intact. The architecture and
settings inventory now describe ten pages and the separate group resets.

All 28 focused rows pass (514.66 s): composed GitHub at 100/125/200%, Settings
at 100/125/150/200%, workbench and Home at 100/125/200%, label settings and
threads, all six workshop courses, workbench/review-behaviour/label-authoring,
tour and setup destination public paths, and both application composition
contracts. clang-format 22 checks all 16 touched C++ files; documentation and
diff gates pass. Existing focused tidy and negative-control evidence are
unchanged. Loaded frontend-design for the accepted UI landing.

The owner's ordered handoff replaces the full-suite-per-card gate with one
integrated full suite and repository format/tidy at item 9; those are still
mandatory queue work, not a passing observation claimed by this landing.
