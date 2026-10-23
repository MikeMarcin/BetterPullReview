---
schema: "implement-rfc.card.v1"
id: "BPR-1745"
title: "An outdated thread takes a reply from the drawer, bound to the thread, as GitHub's does"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1745--BPR-1745--fdd0e4184b52.json","kind":"feature","origin":"owner decision, 2026-10-22, relayed by the coordinator","related_cards":["BPR-0657","BPR-1046","BPR-1290","BPR-1374","BPR-1581","BPR-1733","BPR-1740","BPR-1741"]}
acceptance: [{"criterion": "AC1: The core decides where a reply goes from GitHub's thread data: a current thread on its line, an outdated one by the thread, with the outdated note; a thread GitHub says the viewer cannot reply in, one on a locked pull request, an anonymous read and a pull request comment take none; a thread-bound reply cannot become a comment, answer another thread, take a range or be kept as a comment; the row fails without the change", "outcome": "pass", "revision": "1de2add52025b79479f6ed1a482ac4748b0a6925", "evidence": {"kind": "command", "command": "ctest -R '^(core_thread_reply|core_reply_draft_standing|core_revisioned_review_draft)$'", "detail": "core_thread_reply: routes, refusals, anchors, reopen, reducer, standing. With the old rules restored (outdated refused, the reducer and standing guards removed) seven expectations fail by name."}}, {"criterion": "AC2: The host publishes a thread-bound reply by the thread through the existing reply path, journal and idempotency key, and reconciles it, including an unclear outcome; GitHub's answer with no line on the thread's commit is confirmed, an answer with a line is not; a new comment never takes the thread-bound anchor; the rows fail without the change", "outcome": "pass", "revision": "1de2add52025b79479f6ed1a482ac4748b0a6925", "evidence": {"kind": "command", "command": "ctest -R '^(application_review_mutation_application_host|live_review_mutation_provider|github_mutation_contract|gh_cli_transport)$'", "detail": "an_outdated_threads_reply_is_bound_to_its_thread runs the whole course over the real SQLite journal (written once, receipt recorded with the thread-bound anchor and the thread's commit), a lost answer then a restarted session that confirms the reply from the thread without writing, and the two mismatches. test_an_outdated_threads_reply_is_held_to_its_thread holds the request to the thread's path and original commit and refuses five wrong shapes. Both fail with the old rules."}}, {"criterion": "AC3: In the Comments drawer an outdated thread's Reply opens the reply field in place with no refusal and the quiet line 'Outdated: this thread's line has changed since it was written'; Discuss and Threads answer it by the thread too; the rows fail without the change", "outcome": "pass", "revision": "1de2add52025b79479f6ed1a482ac4748b0a6925", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest -R '^(qt_conversation_cards_view|qt_workbench_conversation_reveal|qt_workbench_line_draft|qt_github_conversation_panel)$'", "detail": "test_an_outdated_thread_takes_a_reply_with_a_quiet_note (menu Reply enabled, note text, secondary ink, smaller, above the field, accessible name; none on a current thread). The BPR-1740 long-thread course's outdated half now expects the thread route in Review, Discuss and Threads, with no notice. Both fail with the old rules."}}, {"criterion": "AC4: A composed course over the fixture makes a thread outdated by a push, replies from the drawer and publishes it once, bound to the thread and its commit; a written reply survives a restart and Reply opens it; a reply is carried to a new head and survives it; the rows fail without the change", "outcome": "pass", "revision": "1de2add52025b79479f6ed1a482ac4748b0a6925", "evidence": {"kind": "command", "command": "ctest -R '^(application_reply_draft_contract|application_reply_draft_survival|application_threaded_reply_contract|application_private_review_host)$'", "detail": "application_reply_draft_contract adds outdated-publish, outdated-write and outdated-restart (REPLY_DRAFT_EVIDENCE:1111111 each, 4 runs of 4 stable); the seven earlier courses still pass. application_reply_draft_survival adds the restart and head-change scenarios over the real SQLite private store. With the old rules outdated-publish fails at not-outdated and the head change's four expectations fail."}}, {"criterion": "AC5: A live course on BPR-Test makes a thread by the second identity, pushes a commit that makes it outdated, and replies through the decided path: the core's decision, the application host, the shipped session, the SQLite journal; a second process answers from the journal; a reply whose answer is lost is confirmed from the thread; each reply reads back once, with no line, on the thread's commit; everything made is closed or deleted", "outcome": "pass", "revision": "1de2add52025b79479f6ed1a482ac4748b0a6925", "evidence": {"kind": "command", "command": "python3 tests/scripts/outdated_reply_live_course.py --tool build/dev/tests/github_contract/outdated_reply_live_course --work <scratch>", "detail": "BPR-Test#135, 2026-10-23T00:27Z: decision on_thread; --reply writes once (PRRC_kwDOTyvof88AAAABA5L9tQ), journal succeeded threadBound=1 commit e53cacebb974; --again duplicate, 0 writes; --lose-answer writes once, the answer dropped, reconciled from the thread (PRRC_kwDOTyvof88AAAABA5MBHw); --recover duplicate, 0 writes; read-back each once, line null. X-RateLimit-Remaining 4982 at the end. #133 (scratch probe), #134 (a first run) and #135 closed; their branches deleted."}}, {"criterion": "AC6: Format (clang-format 22), clang-tidy on the changed units, the core-only build, the reseals and the documentation gate pass", "outcome": "pass", "revision": "1de2add52025b79479f6ed1a482ac4748b0a6925", "evidence": {"kind": "command", "command": "fmt.sh --check; tidy.py --warnings-as-errors; core-only preset; reseal_round_trip_composition.py --check; reseal_command_correctness.py --check; verify_documentation.py", "detail": "See Verification."}}]
---

# BPR-1745: An outdated thread takes a reply from the drawer

## Owner decision

2026-10-22, relayed by the coordinator, verbatim: "Allow replying to an
outdated thread from the drawer, like GitHub does; open a card for it."

[[BPR-1740]] had found that an outdated thread could not take a reply in the
drawer. The reply was a draft on the thread's current line, which an outdated
thread no longer has, and Reply was refused with "This thread's line isn't in
the code shown, so it can't take a reply here." GitHub accepts a reply to an
outdated thread, because a reply names only the thread it answers.
RFC-BPR-0028 is amended ("Owner decision, 2026-10-22: an outdated thread
takes a reply").

## Design

- **Core decides** (`bpr/core/thread_reply.hpp`). `decide_thread_reply` takes
  GitHub's thread data: a review thread or not, outdated or not, an anonymous
  read, `viewerCanReply` and the pull request's lock. A current thread is
  answered on its line, as before. An outdated one is answered by the thread,
  with the outdated note. A thread GitHub says the viewer cannot reply in, a
  locked pull request, an anonymous read and a pull request comment are
  refused.
- **A reply bound to the thread.** Its anchor is
  `thread_reply_anchor(comparison)`: the comparison it was written in, the
  right side, and `thread_reply_line` (the 32-bit maximum). No document has
  that line, because a line count is a 32-bit value. Nothing that looks for a
  draft at a line finds it. The private store, the journal and the
  idempotency key keep it as they keep any line, so there is no schema
  change. The reducer refuses to turn it into a comment, point it at another
  thread, give it a range or mark it pending. Its standing is the thread's,
  and when the thread is gone it is kept but not offered as a comment.
  `reopen_thread_reply` opens the latest unsent one for the thread.
- **Host.** `review_mutation_publication_request::ThreadReply` carries the
  thread's path and its first comment's original commit. `validate` holds
  such a request to the thread: a reply, the thread-bound anchor, a portable
  path, and a commit that is not the head. It reads no selection or source.
  The coordinator and contract admit the anchor only for a reply
  (`thread_bound_binding`). The reply goes through the existing reply path:
  the `ObserveThread` pre-flight, the `ObserveThreadComments` baseline, the
  journal admission, `addPullRequestReviewThreadReply` and the post-write
  reconciliation. Live, GitHub answers a reply to an outdated thread with
  `line: null` on the commit the thread was written on, and its thread
  observation shows it the same way. The answer parser and the
  reconciliation's exact match accept exactly that for a thread-bound reply.
  For a line-bound reply they still require the line. The head is
  acknowledged, as for a resolution: a thread belongs to the pull request,
  not to a commit.
- **Persistence.** `review_composition_application_host::select_thread_reply`
  opens or makes the reply under the bound comparison.
  `carry_thread_replies` runs in the private workflow's restore and before
  `select_thread_reply`. In one transaction it gives each unsent thread-bound
  reply from an earlier head a new draft with the same words under the bound
  head, and discards the earlier copy. A reply has no line, so a head change
  has nothing to move. A Refresh holds the reply again without choosing a
  line.
- **Composition.** The window's new thread-reply dispatch reaches
  `begin_thread_bound_reply`. It decides the route again from the accepted
  snapshot's thread and opens the reply privately. While drafts are being
  restored or a line's draft is being opened, it waits
  (`PendingThreadReply`). Publish sends `ThreadReply` under the one-shot
  gesture grant, which binds to the accepted handoff when there is no
  selected diff.
- **Qt presents** (designed with the `frontend-design:frontend-design`
  skill). The card asks the core for the route (`thread_card::ReplyOnThread`).
  The menu's Reply is enabled. In Review the reply field opens in place, as
  for a current thread. In Discuss and Threads the composer opens. Above the
  field there is one quiet line: "Outdated: this thread's line has changed
  since it was written". It is set at 0.92 of the card's size in the card's
  secondary ink, with no icon or colour, because the header already says
  "Outdated". The field is named "Reply to the outdated thread". The
  composer's heading says "Outdated thread" and shows no code excerpt. The
  refusal stays only for a current thread whose line cannot be selected.
  The offscreen renders were reviewed in the session and are not committed.

## Verification

On the rebased candidate (main `27d17fea5`), macOS arm64, Debug with `-g0`,
offscreen:

| Level | Rows | Result | With the old rules |
|---|---|---|---|
| Core | `core_thread_reply`, `core_reply_draft_standing`, `core_revisioned_review_draft` | pass | 7 expectations fail |
| Host and journal | `application_review_mutation_application_host`, `live_review_mutation_provider`, `github_mutation_contract`, `gh_cli_transport` | pass | host and provider rows fail |
| Store | `application_reply_draft_survival`, `application_private_review_host`, `application_shared_draft_application_host` | pass | 4 head-change expectations fail |
| Qt | `qt_conversation_cards_view`, `qt_workbench_conversation_reveal`, `qt_workbench_line_draft`, `qt_github_conversation_panel` | pass | both outdated rows fail |
| Composed | `application_reply_draft_contract` (10 courses, 4 of 4 stable), `application_threaded_reply_contract`, `application_review_round_trip_composition_contract`, `architecture_boundaries` | pass | `outdated-publish` fails at not-outdated |
| Live | BPR-Test#135 | pass | — |

Gates on the same candidate: `coding-format-check` passed (clang-format
22.1.8, 1756 files). `tidy.py --warnings-as-errors` passed on the 21 changed
translation units, the live course tool included. The core-only preset built
and passed `core_thread_reply`, `core_reply_draft_standing` and
`core_revisioned_review_draft`. The round-trip pins were resealed (full and
manifest, with a note; mutation unchanged), and so were the command registry
and workbench case inventory; `command_correctness_registry_tests.py` passed.
The documentation gate reported only this card's provisional ID, and
`verify_documentation_tests.py` passed 67 of 67.

`github_mutation_contract`'s overflow row moves to the last line a document
can have, one below the 32-bit maximum, and adds that a new comment never
takes the thread-bound line.

## Not in scope, and left open

- **`viewerCanReply` and the lock are not read yet.** The thread read selects
  neither field. Adding `viewerCanReply` means a new schema probe fact, a
  read-shape flag and new recorded fixtures. Until then the core gets them as
  unknown and refuses nothing on them, and GitHub's answer to Publish
  decides, as it already does for a current thread. The live course read
  `viewerCanReply` and `locked` through `gh` and passed them in.
- **The GitHub CLI connection writes no thread reply**, for current and
  outdated threads alike: `gh_cli_transport` refuses `reply_review_thread`,
  as before this card. The native connection writes it. The live course runs
  the shipped session over `gh`, as [[BPR-1741]]'s course does, so the
  request and answer are the product's. Admitting the reply write on the CLI
  connection is a separate change.
- **A reply being published when the head moves** stays on its own head and
  is not carried. Its idempotency key names the head, so carrying it could
  send it twice. A reply on a line written on an earlier head is not carried
  either; that is BPR-1374's open point.
- **A file-level thread** has no line either. This card does not route it by
  the thread; its reply is unchanged.
- A thread on the left side is journalled on the right, the side the journal
  keeps, because the thread and not the side says where the reply goes.

## Result

Landed in `1de2add52` on the implementer's branch; the provisional ID is the coordinator's
to allocate.
