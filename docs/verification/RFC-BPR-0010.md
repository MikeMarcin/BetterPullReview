# RFC-BPR-0010 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: available-target implementation complete. BPR-0159 through BPR-0164
delivered the core, host, and Qt slices; BPR-0165 documented them; BPR-0166
removed the surface nothing read; BPR-0170 added the durable enrollment
store.

RFC-BPR-0010 stays `Proposed`. No card here makes a closure claim, and closure
remains ordered behind RFC-BPR-0002.

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
| Refresh respects the host's limits | Host with manual executor | Drive simulated primary and secondary rate limits, ETags, offline state, and cancellation | Bounded backoff, conditional requests reused, and no work issued while offline | Verified. BPR-0161 doubles backoff to a 900-second cap, prefers the provider's own `Retry-After`, reuses validators so an unchanged pull request costs a conditional request, reports offline explicitly, and treats cancellation as neither a failure nor a reason to back off |
| A completion cannot cross sessions | Host | Deliver a stale completion after an account, repository, or pull-request switch | The completion is refused by identity rather than by ordering luck | Verified. BPR-0161 checks identity before reading anything else, so a stale answer changes no validator, no backoff, and no online state. A session is host, account, and generation: an account switch reusing the generation is still refused |
| Enrollment survives the path it matched | Host plus store | Enroll by path rule, project without re-deriving membership, and read it back from a separate process | The item stays enrolled with its original reason until closed or removed | Verified. BPR-0162 retains membership as enrolled rather than recomputing it, so the pull request a path rule caught stays when the path leaves it. BPR-0170 makes that survive a restart: a forked child writes and this process reads back the rule that brought the item in and the departure reason of one that left |
| The queue updates incrementally | Qt | Replace a queue projection with one differing in a few items | Exact insert, remove, move, and data signals by stable identity, and no routine reset | Verified. BPR-0163's `review_landing_model` matches rows by `pull_request_ref`, so an item that becomes urgent moves and keeps its persistent index rather than being replaced. Re-applying an unchanged queue emits nothing at all. A positional diff — which RFC-BPR-0007's model uses for a different job — would have reported a reorder as a run of changes on rows that did not change |
| Notifications carry no content | Host | Request a notification for a review with comments and a matched path rule | The request type has no field a body, a title, or a path could occupy | Verified. BPR-0164 reads the whole notification and requires it to be exactly identity, the bucket's fixed sentence, and a count — not merely free of a list of bad words. Leaking the matched rule's path takes three deliberate edits, and the test catches it |
| A notification links to the right review | Host | Resolve the target of a requested notification | The link resolves to the host, account, repository, and pull request the item names | Verified for the target. BPR-0164's target is a local reference rather than a URL: a `https://` address would open a browser, and a link the operating system can hand to any handler is one another application can claim. That following it from a delivered banner selects that review is BPR-0168 |
| Closure and permission loss are honest | Host | Close, reopen, transfer, and lose permission to an enrolled pull request | Each produces an explicit queue state rather than a silent disappearance | Verified. BPR-0162 keeps forbidden and gone distinct — one is a permission a reviewer may be able to ask for, the other a pull request that is not there — retains a departed item with its reason while dropping it from the queue, and restores the same record on reopening. Only a withdrawal moves an item out, so an outage does not empty the queue |

## Deferred gates

| Card | Gate | What is missing |
|---|---|---|
| BPR-0168 | A native notification is delivered and its deep link opens the right local review | A person to grant Notification Center permission once and to see the banner. The harness proves the request's content and destination; it cannot prove the operating system showed it |
| BPR-0169 | Enrolling by observer rule leaves GitHub's reviewer and subscription state untouched | A designated repository and token, inheriting BPR-0142. That the correlator cannot issue a request is structural and provable here; that the *server* also shows no change is a different claim and needs a server |

## Closeout replay

Run at commit `8903b4d`, on macOS 15.7.7 arm64 with Qt 6.11.1.

| Command | Result |
|---|---|
| `ctest --preset dev --output-on-failure` | 118/118 in 698.90 s |
| `ctest --preset core-only --output-on-failure` | 53/53 in 81.72 s |
| `cmake --build --preset dev --target coding-format-check` | 272 files, passed |
| `cmake --build --preset dev --target coding-tidy` | passed |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
| `python3 scripts/verify_conditional_branches.py --root .` | `CONDITIONAL_BRANCH_IDENTIFIERS_OK:272` |
| `python3 scripts/verify_binary_dependencies.py …` | `BINARY_DEPENDENCIES_OK` |
| `python3 scripts/verify_runtime_surface.py …` | `RUNTIME_SURFACE_OK` |
| `git ls-files \| grep -c '^.github/workflows/'` | 0 |
| `git diff --check` | clean |

The RFC-BPR-0010 suites specifically:

| Suite | Preset |
|---|---|
| `core_review_queue` | `dev` and `core-only` |
| `core_review_queue_attention` | `dev` and `core-only` |
| `application_review_queue_refresh` | `dev` |
| `application_review_queue_enrollment` | `dev` |
| `application_review_queue_notification` | `dev` |
| `qt_review_landing_model` | `dev` |

Both core suites run under `core-only`, which links neither Qt nor SQLite, so
"the core reads no clock and reaches no provider" is a property of the build
rather than of a review.

## Not built

Filters and sorting presentation. BPR-0163's card named them; the model
presents what the core ordered, and a filter is a view concern that would sit
above it. Nothing was built that only appears to filter.

## Documentation ledger

| Topic | Document | State |
|---|---|---|
| Role semantics, observer grammar and precedence, enrollment retention and departure, the refresh schedule and its limits, notification privacy, offline behaviour, ordering, and queue explanations | [Review Queue](../architecture/review-queue.md) | Written. Every number and behaviour checked against the source rather than from memory, and the three gaps named in the document itself |
| Local private review progress | [Review Ledger](../architecture/review-ledger.md) | No impact. The queue reads what the ledger records; it adds no receipt and changes no acceptance |
| Shared review round trip | [Review Round Trip](../architecture/review-round-trip.md) | No impact. The queue issues no mutation; enrollment is local and the correlator holds no provider |

## Cards

| Card | Slice |
|---|---|
| BPR-0159 | Core: role classification, observer rules, exclusions |
| BPR-0160 | Core: attention buckets, reasons, and queue navigation |
| BPR-0161 | Host: refresh scheduler, limits, offline, cancellation, identity isolation |
| BPR-0162 | Host: enrollment persistence and queue projection |
| BPR-0163 | Qt: landing model, ordered by the core, incremental by identity |
| BPR-0164 | Notifications: metadata-only content and deep links |
| BPR-0165 | Documentation: `docs/architecture/review-queue.md` |
| BPR-0166 | Simplification pass |
| BPR-0167 | Closeout preparation |
