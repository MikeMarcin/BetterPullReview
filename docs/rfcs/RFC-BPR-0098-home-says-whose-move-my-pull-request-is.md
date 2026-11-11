---
id: "RFC-BPR-0098"
title: "Home says whose move each of my pull requests is"
status: "Proposed"
metadata: {"origin":"owner notes, 2026-11-10, note 1","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
---

# Home says whose move each of my pull requests is

## Problem and grounded current behavior

The owner cannot tell from Home which of their own pull requests have come
back to them, or why ([note 1](assets/v0-5-0-prerelease-targets-v1/SOURCE_NOTES.md)). The inspected code shows
why the information is missing rather than hidden:

- `home_section_for` in
  [review_queue_attention.cpp](../../src/core/src/review_queue_attention.cpp)
  puts an authored pull request in **Waiting on others** when its bucket is
  `authored_awaiting_reviewer_action`, and in **My pull requests** otherwise.
  So My pull requests already holds exactly the authored pull requests that
  are not waiting on a reviewer.
- The reasons a row there can give come from `home_why_sentence` in
  [review_landing_model.cpp](../../src/adapters/qt/src/review_landing_model.cpp):
  "My pull request", an unread-comment count, a failed check, "Merge
  blocked", or "Nothing here is waiting on anyone". An approved pull request
  that can merge reads as Quiet. A pull request with changes requested says
  nothing about it.
- Home already fetches `reviewDecision`, `mergeStateStatus`, the latest
  reviews and the check rollup for each pull request
  ([github_read_contract.cpp](../../src/adapters/github/src/github_read_contract.cpp),
  `home_pull_request_fragment`). `queue_waiting_facts` in
  [review_queue_facts.cpp](../../src/application/src/review_queue_facts.cpp)
  keeps only a request count, `RequiredReview` as a boolean, `MergeBlocked`
  and failed check names. Changes requested, approved-and-clean and who
  reviewed are read and then dropped before the core.
- Unresolved review conversations are not in the Home read at all;
  `isResolved` is read only when a pull request is opened.

[RFC-BPR-0066](RFC-BPR-0066-home-authored-pull-requests.md) keeps
authored pull requests out of Needs me and states that authorship is not a
new attention state.
[RFC-BPR-0070](RFC-BPR-0070-preserve-home-waiting.md) adds "truthful
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

**Established in implementation, 2026-11-10 ([[BPR-1989]]).** The core
decides the move (`core::next_move_for` in
[review_queue_attention.hpp](../../src/core/include/bpr/core/review_queue_attention.hpp)).
"Changes requested" and "approved" are GitHub's review decision when it gives
one; when it reports none (no review is required), they are the reviewers'
standing verdicts, each decided as `my_review_for` decides mine. A merge
GitHub refuses (`BLOCKED`, `UNSTABLE`) is Blocked only when no outstanding
review, running check or draft status explains it, because GitHub reports
every pull request awaiting a required review as blocked and Waiting would
otherwise never be reached; a conflict, or a base that must be merged in
first, is Blocked whatever else is pending. "Required checks passed" is every
check of the head's rollup passed or skipped, since the rollup does not say
which are required. Unassigned needs the requests and the reviews to have been
read; a row whose activity was never read keeps the Unknown word it has today
and sorts after Quiet. Quiet itself needs the reviews to have been read: a
pull request a search has just found has its title, draft status and
requests but not yet its reviews, decision or checks, so until the next
refresh reads them (one query, as for every listed pull request) it is
Unknown, "Its reviews and checks have not been read yet.", never Quiet. A pull request Failing, Blocked, Ready or Unassigned
while reviewers are also awaited is mine to act on, so it sits in My pull
requests; only Waiting rows sit in Waiting on others.

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
[design round](../design/v0-5-0-targets-v1/index.html#home).

"Hide quiet" continues to hide only Quiet rows.

**Established in implementation, 2026-11-10 ([[BPR-1989]]).** The remaining
sentences: Failing "build-linux failed; approved by alice.", Blocked "It
conflicts with the base branch.", "The base branch must be merged in first."
or "Merge blocked.", Checking "Approved by bo; 2 checks are running.", Draft
"Still a draft.", Unread "2 new comments I have not read.", Waiting keeps
today's "Waiting on 2 reviewers.", Quiet "Nothing here is waiting on anyone."
Unread comments on a row whose move is not Unread are appended as today.
Waiting rows keep the Waiting on others tone. The header gives only the
counts named above, and an unknown count as today; it no longer says how many
need action, are awaiting review or are blocked. "Hide quiet" reads the
move, so an approved, mergeable pull request of mine is never hidden as quiet.
The word keeps the place Home gives every row's word, after the sentence
([[BPR-0776]]), where the design round drew it first; moving it for authored
rows alone would give one list two anatomies, so the owner's look asks.

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

**Established in implementation, 2026-11-10 ([[BPR-1989]]).** The 50 threads
read are the last 50 (`reviewThreads(last:50)`), the most likely to await a
reply, in a fragment that only an authored pull request's alias of Home's
batched query asks for; a batch of one is still sent for a pull request of
mine, and a return to Home re-reads one of mine the same way. When a pull
request has more threads than were read, the sentence says "N or more
conversations await my reply", which reads "50 or more" when every thread read
awaits one. A read that could not see the conversations (a pull request read
the REST way because it has more than a page of reviews, requests or checks,
or a server without the batched query) keeps the count an earlier read made,
said as last known; a pull request never read through the batch has no count
and is Returned only on a change request. The facts are stored with the row
(queue facts version 4) and read back as last known.

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

## Acceptance criteria

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
