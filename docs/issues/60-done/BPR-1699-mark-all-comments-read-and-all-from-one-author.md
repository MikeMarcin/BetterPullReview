---
schema: "implement-rfc.card.v1"
id: "BPR-1699"
title: "Mark all comments as read, and all from one author"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1699--BPR-1699--04640ad1f543.json","origin":"Owner request, 2026-10-21: We have some automated review bots that are very spammy on reviews. I think I want the ability to Mark all comments as read and Mark all comments from a particular user as read.","related_cards":["BPR-0656","BPR-1582","BPR-1601","BPR-1668"],"frontend_design_skill":"loaded before the menu items and their wording were designed"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"66710c9c7a11f07de1dd505e9c4bc52bc9ad98e0","evidence":{"kind":"command","command":"cmake --build build/dev --target test_better_pull_review_review_ledger_core_tests && build/dev/tests/core/better_pull_review_review_ledger_core_tests","detail":"review-ledger-core-tests:pass. mark_conversations_read over four conversations with every post in scope marks the three new posts someone else wrote, writes receipts only for the two conversations that moved, in conversation order, keeps acceptance, and a second mark returns no receipts. One author (review_author_of(\"coderabbitai[bot]\"), one author in any case, distinct from \"coderabbitai\" and from another bot): the bot's singleton, its thread with my reply inside, and its root before a person's answer are read; the person's answer, a bot reply after a person's new post and the interleaved tail stay new and are counted kept (2); a person's mark and my own mark follow the same rule. A later bot post and a later edit are new after the mark. A conversation observed twice is decided once."}},{"criterion":"AC2","outcome":"pass","revision":"66710c9c7a11f07de1dd505e9c4bc52bc9ad98e0","evidence":{"kind":"command","command":"build/dev/tests/application/better_pull_review_review_ledger_host_tests && build/dev/tests/sqlite_contract/better_pull_review_sqlite_review_ledger_tests","detail":"review-ledger-host-tests:pass and review-ledger-storage-tests:pass. Host: a refused batch answers its failure, stores and keeps nothing, and the read state is exactly as before; one author's mark is one batch of two receipts and leaves the person's posts new at once; Mark all reads the rest in one more batch; a repeat reaches no store; a new host over the same store reads the bulk mark back, and the bot's next post is new (no mute). SQLite: store_conversation_receipts commits in one BEGIN IMMEDIATE transaction; a batch whose second receipt the database refuses after the first upsert rolled back (the earlier receipt unchanged, none added); a valid batch survives reopening; the port refuses a batch naming no conversation before the database."}},{"criterion":"AC3","outcome":"pass","revision":"66710c9c7a11f07de1dd505e9c4bc52bc9ad98e0","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen build/dev/tests/adapters_qt/better_pull_review_qt_conversation_cards_view_tests; QT_QPA_PLATFORM=offscreen build/dev/tests/adapters_qt/better_pull_review_qt_workbench_window_tests; ctest --test-dir build/dev -R '^application_active_changed_scaling$'","detail":"conversation-cards-view-tests:pass: a bot's thread menu offers Mark all from coderabbitai[bot] as read, disabled with its reason before read state, counting the bot's two new posts across the pull request (login without case), dispatching the login; my thread offers none; the keyboard body menu on a reply offers Mark all from tbutler as read; a new state disables the bot's item with its reason. workbench-window-tests:pass: the Pull Request menu holds &Mark All Comments as Read, disabled until something is new; triggering it dispatches every author, and the committed state moves the counts, Status drawer, Next new comment and the action at once; a thread's author item dispatches the login and leaves the person's two posts counted; the status bar says what was marked, what stayed new and why, and a refusal. Threads' row menu ends with Mark All from <author> as Read, its count the window's over the whole pull request. qt_composed_github and qt_github_conversation_panel pass with the thread menu's indices unchanged. application_active_changed_scaling passes through the shipped composition: the real menu action is one durable ConversationWrites step, every card presents read, Next new comment and the action disable, and Overview is not rebuilt."}},{"criterion":"AC4","outcome":"pass","revision":"66710c9c7a11f07de1dd505e9c4bc52bc9ad98e0","evidence":{"kind":"observation","detail":"Read state stays local. The composition's mark_conversations_read calls only the review ledger host; no GitHub port, mutation or provider read is reached. The round-trip composition contract's production mutation-branch pin is unchanged (16d135419d39); only the full and manifest pins move, for github_application.cpp. No author identity is persisted and the ledger schema is unchanged."}},{"criterion":"AC5","outcome":"pass","revision":"66710c9c7a11f07de1dd505e9c4bc52bc9ad98e0","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <changed translation units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"Format check passes with clang-format 22.1.8 over 1719 files; clang-tidy 22 passes with warnings as errors on every changed translation unit, except two performance-avoid-endl findings in workbench_window_tests.cpp's runCase lambda that main already has and this change does not touch; both documentation gates pass with the ID allocated (the provisional ID fails the card ID pattern until then). architecture_boundaries, command_correctness_registry_contract, application_review_ledger_composition_contract and application_review_round_trip_composition_contract pass. docs/ux/workbench-specification.md and RFC-BPR-0007's receipt semantics record the behaviour."}}]
---

# Mark all comments as read, and all from one author

The owner, 2026-10-21: "We have some automated review bots that are very
spammy on reviews. I think I want the ability to Mark all comments as read and
Mark all comments from a particular user as read."

Read state is RFC-BPR-0007's conversation receipt: one seen cursor per
conversation, a comment identity and its edit version ([[BPR-0656]]), with
pull-request comments and review summaries as one-post conversations
([[BPR-1601]]). This card adds two bulk gestures over it, decided in the core
and committed as one durable change, and changes nothing on GitHub.

## Acceptance

- AC1: The core decides both bulk marks over every conversation of the pull
  request at once, per author when asked, idempotently, and a comment that
  arrives afterwards is new.
- AC2: The ledger host commits a bulk mark's receipts together or not at all;
  a refused commit keeps nothing; the result survives a restart.
- AC3: The Qt actions are offered where the existing conventions put thread
  and post actions, with keyboard access, accessible names and reasons when
  disabled, and the counts and badges move at once when the change is durable.
- AC4: Read state stays local; nothing reaches GitHub.
- AC5: Format, tidy and documentation gates pass.

## Design

- **Where the actions live.** The Pull Request menu's **Mark All Comments as
  Read** (`markAllCommentsReadAction`, mnemonic M), offered while anything on
  the pull request is new. **Mark all from <login> as read** in a thread's
  menu (More, right click, menu key, Shift+F10) for the first post's author,
  after the thread's own items; in a post's body menu, in Discuss and in
  Review, for that post's author; and **Mark All from <login> as Read** in
  Threads' row menu for the post under the pointer, else the first post's
  author. None is offered for my own posts. The shared
  `qt::add_author_read_action` makes the item and its tooltip.
- **The core transition.** `core::mark_conversations_read(receipts, threads,
  scope)` returns `conversation_bulk_read`: the receipts that change, ordered
  by conversation, the posts it marks, and the posts in scope it leaves new.
  An empty `conversation_read_scope` is every post; an author scope is
  `core::review_author_of(login)`, ASCII case-folded, so `name[bot]` is one
  app whatever its case and distinct from the person `name`. The cursor
  passes the author's new posts and mine and stops at the first new post by
  anyone else. `conversation_post_observation` gains `Author`;
  `application::conversation_thread_observation` is now the core's type.
- **Persistence.** `review_ledger_application_host::mark_conversations_read`
  decides against the held receipts at the head of the queue and commits
  through the new port call `store_conversation_receipts`, which SQLite runs
  as one `BEGIN IMMEDIATE` transaction over the existing upsert; it keeps the
  receipts only after the commit succeeds. No schema change.
- **Presentation.** The composition publishes the read state once the write
  is durable (`publish_read_state`, then the queue's unseen count), and the
  window's status bar says what was marked, what stayed new and why, or that
  nothing was saved.

## Not built, and follow-up options

The cursor cannot skip someone else's unread post. One author's mark
therefore leaves new an author's post that follows a newer reply by someone
else; the status bar counts those. Two follow-ups would change that or go
further, and neither is built:

1. **Per-post read marks.** A ledger table of posts read out of order would
   let one author's mark read every post of theirs, interleaved or not. It is
   a schema migration and changes `unread_posts` for every surface.
2. **Always treat an author as read.** A per-profile, per-repository list of
   logins whose posts are read on arrival (a mute), decided in the core when
   the read state is observed and undone from the same menus. The owner asked
   for a one-shot mark, and no existing design calls for a mute.

## Checkpoint

- Candidate: branch `worktree-agent-ae85733d4ebe83d52`, code commit
  `66710c9c7`, rebased onto main `4271e2ce2`.
- Changed paths: `src/core/{include/bpr/core,src}/review_ledger.*`,
  `src/application/{include/bpr/application,src}/` (ledger host, port,
  observing store), `src/adapters/sqlite/src/review_ledger_{database,store}.*`,
  `src/adapters/qt/` (cards view, open card, finder, thread menu helper,
  workbench window), `src/app/src/github_application.cpp`, the tests named
  above, the round-trip composition contract and the command-correctness
  registry and case inventory, `docs/ux/workbench-specification.md` and
  RFC-BPR-0007.
- Shared with RFC-BPR-0089 M4: `workbench_window.{hpp,cpp}`,
  `conversation_cards_view.{hpp,cpp}`, `conversation_open_card.{hpp,cpp}`,
  `conversation_finder.{hpp,cpp}`, `github_application.cpp`,
  `workbench_window_tests.cpp` and the two reseal records. The edits there are
  confined to read state and these menus.
- Pending: none. The ID is provisional; the coordinator allocates it on main.
