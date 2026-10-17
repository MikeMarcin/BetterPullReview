---
schema: "implement-rfc.card.v1"
id: "BPR-1597"
title: "Settings by task: Home & queue, Review and Comments replace Reviews & queue"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
parked_on: "human-review"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"owner-decision","found_by":"Owner pick in attended-review session 0e5dbaff (decision 3229df02), 2026-10-17, and the owner's amendment naming the middle page Review","governing_rfc":"RFC-BPR-0048","related_rfcs":["RFC-BPR-0018"],"amendment":"RFC-BPR-0048 owner amendment 3 (2026-10-17)","related_cards":["BPR-1088","BPR-1289","BPR-1566","BPR-1568","BPR-1584"],"reservation":"docs/issues/_reservations/BPR-1597--BPR-1597--2851f28bd65b.json","acceptance":["BPR-1597:pages","BPR-1597:workshop","BPR-1597:search","BPR-1597:kept","BPR-1597:gates","BPR-1597:owner"]}
acceptance: [{"criterion": "BPR-1597:pages", "outcome": "pass", "revision": "956172a57", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_settings_window.*|qt_workbench_window_scale_(100|125|200)|qt_feedback_label_settings)$'", "detail": "test_settings_are_organised_by_task: the sidebar lists General, Accounts & hosts, Repositories, Home & queue, Review, Comments, Appearance, Notifications, Agents & context, Storage & privacy, each page's title is its row, and Review, Home & queue and Comments host their rows. Opening's Reset returns the modality and the return comparison, Reading's mark-read, drawer filter and reading width, Layout's drawer controls, line details and ribbon; Labels' Reset keeps the helper; Comment workshop's Reset turns only the helper off. Negative control: titling the first page Reviews & queue again fails the sidebar check."}}, {"criterion": "BPR-1597:workshop", "outcome": "pass", "revision": "956172a57", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_feedback_label_settings|application_agent_finding_workshop_.*_contract|application_review_tour_composition_public_path|application_label_authoring_contract)$'", "detail": "Feedback guidance ('Send with comment workshop requests') is on Comments › Comment workshop, follows the published record, Space asks for the whole record with only the helper on, its accessible description is its info glyph's; Guidance text opens Agents & context on Review feedback, whose line names Comments › Comment workshop; the hooks panel has no check box; the guidance courses carry the guidance with the helper on and the press course without it. Negative control: the helper back in the label rows' Reset fails."}}, {"criterion": "BPR-1597:search", "outcome": "pass", "revision": "956172a57", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_settings_window.*$'", "detail": "Search finds A pull request opens in and On return, compare on Review · Opening; Mark a file read, Comments drawer opens on and Reading width on Review · Reading; Drawer controls, Line details and Center ribbon width on Review · Layout; Label marks on Comments · Labels; Feedback guidance on Comments · Comment workshop; Images on Comments · Descriptions and comments; Observer reason on Home & queue · Queue defaults; no result is under Reviews & queue; Return on Feedback guidance opens Comments and changes nothing. Pre-made row controls are raised after their heading so search reads their section."}}, {"criterion": "BPR-1597:kept", "outcome": "pass", "revision": "956172a57", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(application_review_behaviour_contract|application_review_round_trip_composition_contract|application_workbench_composition_contract)$'", "detail": "No store, schema, key, tour_assist or tour_cli change; the behaviour contract and the composition contracts pass."}}]
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
