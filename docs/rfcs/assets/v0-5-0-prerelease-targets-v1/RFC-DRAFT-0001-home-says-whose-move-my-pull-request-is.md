---
id: "RFC-DRAFT-0001"
title: "Home says whose move each of my pull requests is"
status: "Proposed"
metadata: {"origin":"owner notes, 2026-11-10, note 1","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
---

# Home says whose move each of my pull requests is

## Problem and grounded current behavior

The owner cannot tell from Home which of their own pull requests have come
back to them, or why ([note 1](SOURCE_NOTES.md)). The inspected code shows
why the information is missing rather than hidden:

- `home_section_for` in
  [review_queue_attention.cpp](../../../../src/core/src/review_queue_attention.cpp)
  puts an authored pull request in **Waiting on others** when its bucket is
  `authored_awaiting_reviewer_action`, and in **My pull requests** otherwise.
  So My pull requests already holds exactly the authored pull requests that
  are not waiting on a reviewer.
- The reasons a row there can give come from `home_why_sentence` in
  [review_landing_model.cpp](../../../../src/adapters/qt/src/review_landing_model.cpp):
  "My pull request", an unread-comment count, a failed check, "Merge
  blocked", or "Nothing here is waiting on anyone". An approved pull request
  that can merge reads as Quiet. A pull request with changes requested says
  nothing about it.
- Home already fetches `reviewDecision`, `mergeStateStatus`, the latest
  reviews and the check rollup for each pull request
  ([github_read_contract.cpp](../../../../src/adapters/github/src/github_read_contract.cpp),
  `home_pull_request_fragment`). `queue_waiting_facts` in
  [review_queue_facts.cpp](../../../../src/application/src/review_queue_facts.cpp)
  keeps only a request count, `RequiredReview` as a boolean, `MergeBlocked`
  and failed check names. Changes requested, approved-and-clean and who
  reviewed are read and then dropped before the core.
- Unresolved review conversations are not in the Home read at all;
  `isResolved` is read only when a pull request is opened.

[RFC-BPR-0066](../../RFC-BPR-0066-home-authored-pull-requests.md) keeps
authored pull requests out of Needs me and states that authorship is not a
new attention state.
[RFC-BPR-0070](../../RFC-BPR-0070-preserve-home-waiting.md) adds "truthful
row state, not a new section".

## Proposed behavior and boundaries

### One next move per authored row

The core decides one **next move** for every open pull request the signed-in
account authored, from facts the host observed. The first matching row of
this table wins:

| Next move | Reason word | When | Whose move |
|---|---|---|---|
| Returned | Returned | The review decision is changes requested, or at least one unresolved review conversation's latest comment is not mine | Mine |
| Failing | Failing | A required check failed | Mine |
| Blocked | Blocked | GitHub reports the merge blocked or conflicting, and no failed check explains it | Mine |
| Ready | Ready | Approved, required checks passed, mergeable | Mine |
| Unread | Unread | Comments I have not read, and none of the above | Mine |
| Unassigned | Unassigned | Not a draft; no reviewer requested and no review submitted | Mine |
| Draft | Draft | The pull request is a draft | Mine |
| Waiting | Waiting | Reviewers are awaited (today's `authored_awaiting_reviewer_action`) | Theirs |
| Checking | Checking | Approved; required checks still running | Nobody's yet |
| Quiet | Quiet | None of the above | Nobody's |

A Returned row outranks Draft: a reviewer's reply on a draft is still mine
to answer. The existing staleness wording ("last known") applies to every
fact the move was decided from.

### Placement, order and wording

Sections do not change. Waiting rows stay in Waiting on others; every other
authored row stays in My pull requests, ordered by the table above and then
by the existing order within a move.

A row leads with its reason word and gives one sentence that names who and
what, bounded as today: "alice requested changes; 3 conversations await my
reply.", "Approved by alice and bo; checks passed. Ready to merge.",
"build-linux failed; approved by alice.", "No reviewer is requested." The
phrase "My pull request" is dropped from the sentence; the section says it.
Returned rows use the Needs me tone, Ready the confirmed tone, Failing and
Blocked the error tone; the rest keep the My pull requests tone. The section
header counts what came back and what can merge ("2 returned, 1 ready to
merge") and omits a zero count. The frames are in the
[design round](../../../design/v0-5-0-targets-v1/index.html#home).

"Hide quiet" continues to hide only Quiet rows.

### Facts

The decision, the merge state and each reviewer's latest review state are
carried from the existing Home read into queue facts; this needs no new
request. Conversations awaiting my reply need a bounded addition to the Home
read for authored pull requests only: at most 50 review threads per pull
request, each with its resolved flag and its latest comment's author. When a
pull request has more, the sentence says "50 or more". When the host cannot
provide the field (an older GitHub Enterprise Server, a refused query), a
row is Returned only on a changes-requested decision, its sentence gives no
conversation count, and nothing is guessed.

### Boundaries

- This amends RFC-BPR-0066 and RFC-BPR-0070 only in the reasons and order
  of authored rows. Authored pull requests still never appear in Needs me.
- No notification category is added. The 0.4.2 activity notification is
  unchanged.
- The next move is derived, not stored review state. It changes nothing on
  GitHub.

### Settled choice: a reason, not a section

The owner asked for a "returned to me" status or a clear reason inside an
existing section, and was not sure which. Asked for a suggestion, the
authoring session recommended the reason inside My pull requests, and the
owner answered "Go with that" on 2026-11-10. Placement is therefore as
stated above.

The alternative considered was a **Returned to me** section between Needs me
and My pull requests holding only Returned rows. It was not chosen because
Ready and Failing are also the author's move and would sit in a different
section from the rows they belong with. Only placement differs between the
two, so the owner can still switch after judging the rows on a frozen build
without changing the facts, the decision table or the sentences.

## Acceptance

- **AC01:** For each row of the decision table, a fixture pull request with
  exactly those facts yields that next move from the core, and a pull
  request matching several rows yields the first.
- **AC02:** On Home, authored rows appear in My pull requests in the table's
  order, Waiting rows remain in Waiting on others, and no authored row
  appears in Needs me.
- **AC03:** A Returned row's sentence names the reviewer who requested
  changes and the number of conversations awaiting my reply; a Ready row's
  sentence names its approvers. An approved, mergeable pull request never
  reads Quiet.
- **AC04:** With the conversation field unavailable, a changes-requested
  pull request is still Returned, no row states a conversation count, and a
  pull request with only unanswered conversations is not claimed Returned.
- **AC05:** The Home read for a profile with authored pull requests makes no
  more requests than before, and its added fields are bounded as stated.
- **AC06:** The owner accepts the rows on a frozen build against their own
  open pull requests.
