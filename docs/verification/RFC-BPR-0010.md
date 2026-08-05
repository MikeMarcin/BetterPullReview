# RFC-BPR-0010 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: in progress. BPR-0158 allocated cards BPR-0159 through BPR-0167 and
parked two gates as BPR-0168 and BPR-0169. BPR-0159 delivered the core's role
classification and observer rules; BPR-0160 delivered attention buckets,
reasons, and the queue's total order.

RFC-BPR-0010 stays `Proposed`. No card here makes a closure claim, and closure
remains ordered behind RFC-BPR-0002.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0007 and RFC-BPR-0008. Both are implemented on
available targets and stay `Proposed` behind owner-supplied gates and the
ordered closure chain. Done BPR-0088 records the product owner's direction to
advance the implementation frontier on available targets.

## What this RFC is, in one sentence

A landing page that answers what needs the reviewer's attention across reviews,
including pull requests observed because of an author or a path, without asking
GitHub to change reviewer assignment or subscription.

## The rule everything else serves

**Observing is local.** An observer rule enrolls a pull request in this
reviewer's queue and changes nothing on GitHub: no reviewer assignment, no
watch, no subscription, no label, no CODEOWNERS entry.

It is the criterion a convenience would erode first. "Just request review so it
shows up in the normal place" is a plausible feature request that would make
the tool speak on the user's behalf to their colleagues — the opposite of what
a reviewer avoiding a subscription-culture change wants.

The plan expresses it structurally: the correlation that decides enrollment is
a pure function with no provider, so there is no path by which matching a rule
could issue a request. A test watching for an absent request would be testing a
guard; there is nothing here to guard.

## The second rule

**A notification carries no source and no comment body.** It names the review
that wants attention and links to it. Anything more copies private repository
content into the operating system's notification store, which outlives the
application, is readable by other software, and on some systems is synchronised
to other devices — outside every boundary this product documents.

The notification request type has no field a body could occupy.

## Architecture allocation

| Region | Selected responsibility | Planned evidence |
|---|---|---|
| Functional core | Observer-rule matching and exclusions, role classification, enrollment and retention policy, attention-bucket derivation, and the reason each queue item carries. No clock, no timer, no provider | Qt-free and provider-free replay tests under `core-only`, including the proof that the same observations always produce the same buckets |
| Application host | Clocks and timers, periodic refresh, conditional requests, host rate-limit state, bounded concurrency, backoff, cancellation, queue and enrollment persistence, and execution of semantic notification requests | Manual-executor and fake-provider tests driving simulated primary and secondary limits, offline state, cancellation, and identity isolation |
| Qt adapter | Queue models, filters, sorting presentation, status and empty states, and native notification presentation, incremental by stable pull-request identity | Adapter tests asserting incremental signals by identity, no routine resets, and that no widget path issues a request |

Background refresh completions carry host, account, repository, pull-request,
and generation identity, and cannot update a different active session. That is
the same discipline RFC-BPR-0003's epoch host established, and it is restated
here because a queue refreshing many pull requests has more ways to deliver a
completion into the wrong one.

## Queue buckets

Initial review requested · changed since my review · new comments or author
replies · fixes awaiting verification · authored reviews awaiting reviewer
action · observed with new activity · quiet or current.

Observer enrollment persists locally until closure or manual removal, even if
the path that matched later disappears. Each observed item records the rule
that matched it, because a reviewer looking at an unfamiliar pull request in
their own queue is owed the reason it is there.

## Claim matrix

A row is claimed only when its card lands with direct evidence.

| Claim | Direct surface | Command or interaction | What will prove it | Result |
|---|---|---|---|---|
| Role classification matches GitHub | Core plus fixture | Classify author, reviewer, requested reviewer, participant, and observer from sanitized fixtures | Each role derived from the observation alone, with no provider reachable from the core | Verified. BPR-0159 carries every role through unchanged, keeps `requested_reviewer` distinct from `reviewer`, and gives each a distinct name |
| Observer rules enroll locally | Core | Match repository, author, and path rules, including exclusions for generated and vendor paths | Enrollment is a pure function; there is no request path to suppress | Verified. BPR-0159's `enrollment_for` takes no provider and `queue_membership` has no field a request could occupy. Path rules stop at a component boundary, and exclusions beat other rules but never a GitHub role — hiding an assigned review is a way to miss work nobody would think to look for |
| Every item explains itself | Core | Read the bucket and the observer reason for each enrolled item | The reason is carried by the item rather than recomputed by the presentation | Verified. BPR-0160's `queue_item` carries the membership and the bucket, and `ordered_queue` derives neither. Every bucket has a distinct name and a distinct sentence saying what is being asked, because a label alone leaves the reviewer to guess |
| Decisions are clock and network independent | Core under `core-only` | Replay one observation set twice | Identical buckets and ordering, with no clock read and no timer scheduled | Verified. BPR-0160 orders by bucket then identity, so 64 items supplied ascending, descending, and rotated give one queue. The observation carries no timestamp: "new since I last looked" is a comparison against what the ledger records, not against a wall clock |
| Refresh respects the host's limits | Host with manual executor | Drive simulated primary and secondary rate limits, ETags, offline state, and cancellation | Bounded backoff, conditional requests reused, and no work issued while offline | Unstarted (BPR-0161) |
| A completion cannot cross sessions | Host | Deliver a stale completion after an account, repository, or pull-request switch | The completion is refused by identity rather than by ordering luck | Unstarted (BPR-0161) |
| Enrollment survives the path it matched | Host plus store | Enroll by path rule, remove the path, reopen | The item stays enrolled with its original reason until closed or removed | Unstarted (BPR-0162) |
| The queue updates incrementally | Qt | Replace a queue projection with one differing in a few items | Exact insert, remove, move, and data signals by stable identity, and no routine reset | Unstarted (BPR-0163) |
| Notifications carry no content | Host plus Qt | Request a notification for a review with comments | The request type has no body field, so no path can carry one | Unstarted (BPR-0164) |
| A notification links to the right review | Host plus Qt | Follow the deep link from a requested notification | The link resolves to the host, repository, and pull request the item names | Unstarted (BPR-0164) |
| Closure and permission loss are honest | Core plus host | Close, reopen, transfer, and lose permission to an enrolled pull request | Each produces an explicit queue state rather than a silent disappearance | Unstarted (BPR-0162) |

## Deferred gates

| Card | Gate | What is missing |
|---|---|---|
| BPR-0168 | A native notification is delivered and its deep link opens the right local review | A person to grant Notification Center permission once and to see the banner. The harness proves the request's content and destination; it cannot prove the operating system showed it |
| BPR-0169 | Enrolling by observer rule leaves GitHub's reviewer and subscription state untouched | A designated repository and token, inheriting BPR-0142. That the correlator cannot issue a request is structural and provable here; that the *server* also shows no change is a different claim and needs a server |

## Cards

| Card | Slice |
|---|---|
| BPR-0159 | Core: role classification, observer rules, exclusions |
| BPR-0160 | Core: attention buckets, reasons, and queue navigation |
| BPR-0161 | Host: refresh scheduler, limits, offline, cancellation, identity isolation |
| BPR-0162 | Host: enrollment persistence and queue projection |
| BPR-0163 | Qt: queue model, filters, sorting, empty states |
| BPR-0164 | Notifications: metadata-only content and deep links |
| BPR-0165 | Documentation: `docs/architecture/review-queue.md` |
| BPR-0166 | Simplification pass |
| BPR-0167 | Closeout preparation |
