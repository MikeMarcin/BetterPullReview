# Review Queue

The review queue is a landing page that answers one question: what needs this
reviewer's attention across every review they have a stake in. It includes pull
requests GitHub put in front of them, and pull requests they chose to watch
because of an author or a path.

It asks the organization to change nothing. No reviewer assignment, no watch,
no subscription, no label, no CODEOWNERS entry.

## The rule the rest of this document serves

**Observing is local.** An observer rule enrolls a pull request in this queue
and changes nothing on GitHub.

It is the criterion a convenience would erode first. "Just request review so it
shows up in the normal place" is a plausible feature request, and it would make
the tool speak on the user's behalf to their colleagues — precisely what a
reviewer who is avoiding a subscription-culture change is trying not to do.

The expression is structural rather than guarded: `core::enrollment_for` takes
an observation and a list of rules, holds no provider, and returns a
`queue_membership` with no field a request could occupy. There is no path to
guard.

## The second rule

**A notification carries no source and no comment body.** The operating
system's notification store outlives the application, is readable by other
software, and on some systems is synchronised to the user's other devices.
Repository content placed there has left this product's control permanently.

`application::queue_notification_request` has no field a body, a title, or a
path could occupy. Redacting on the way out was the alternative and is worse:
a redaction step is a place a silent mistake can live.

## Ownership

| Region | Owns |
|---|---|
| Functional core | Role classification, observer-rule matching and exclusions, enrollment policy, attention buckets, the reason each item carries, and the queue's total order |
| Application host | Clocks and timers, refresh scheduling, conditional requests, rate-limit state, backoff, cancellation, session identity, enrollment retention, and notification content |
| Qt adapter | The landing model, its badges and reasons, and incremental updates by pull-request identity |

The core reads no clock and schedules no timer. Its suites run under the
`core-only` preset, which links neither Qt nor SQLite.

## Typed vocabulary

- **Observation** — `core::pull_request_observation` (who, where, which paths)
  and `core::queue_activity_observation` (what has happened since).
- **Rule** — `core::observer_rule`: repository, author, or path, including or
  excluding.
- **Decision** — `core::queue_membership`, carrying the GitHub role, whether a
  rule matched, and which one.
- **Item** — `core::queue_item`: identity, membership, bucket.
- **Request** — `application::queue_refresh_request`, carrying the session and
  the validator; `application::queue_notification_request`, carrying identity
  and a count.
- **Completion** — `application::queue_refresh_completion` with a typed status.
- **Delta** — `application::refresh_disposition` and
  `application::enrollment_departure`.

## Roles

`author`, `reviewer`, `requested_reviewer`, `participant`, and `none`. A role
is what GitHub says it is; this tool never promotes someone into one the server
has not given them.

`requested_reviewer` and `reviewer` stay distinct. Asked-and-not-yet-answered
is not answered, and collapsing them would lose the difference between work
waiting and work done — which is most of what a queue is for.

## Observer rules

A rule matches a repository, an author, or a path. Path patterns are prefixes,
optionally ending in `*`.

Without a trailing star a path rule stops at a component boundary: `src/a`
matches `src/a` and `src/a/x.cpp` and not `src/about.cpp`. A reviewer who wants
the prefix writes `src/a*`. Without the boundary a rule silently enrolls files
nobody named, and the reviewer has no way to tell which rule was responsible.

**Exclusions beat other rules and never a role.** A reviewer who excluded the
generated tree said something more specific than "this repository", so an
exclusion wins against an inclusion. It does not win against being asked: a
reviewer who missed an assigned review because of a path rule would have no
reason to go looking, since as far as they knew they were never asked. The
excluded item still records the rule, so the absence is explicable.

When several inclusion rules match, the first in the reviewer's own list is
recorded. The list is theirs and its order is a precedence they can edit. What
the order never changes is *whether* an item is enrolled.

## Attention buckets

In order of attention:

| Bucket | What it asks |
|---|---|
| initial review requested | You have been asked to review this and have not yet. |
| changed since my review | The code moved after you reviewed it. |
| new comments or author replies | There is something here you have not read. |
| fixes awaiting verification | Somebody says a defect you filed is fixed. You have not checked it. |
| authored, awaiting reviewer action | You are waiting on a reviewer, not the other way round. |
| observed, with new activity | Something happened on a pull request you watch. |
| quiet | Nothing here is waiting on anyone. |

The enumeration order *is* the order of attention. If the two disagreed, every
surface reading the enumeration would present a different order from the one
the queue produced.

An item can be true of several buckets at once and lands in the most urgent. A
queue that placed it in all of them would ask the reviewer to notice the same
work repeatedly.

Three of these placements are decisions rather than mechanics:

**An unanswered request outranks everything.** It is the only state where
somebody is blocked on this reviewer having done nothing at all.

**Authored comes after the reviewer-facing buckets.** Waiting on somebody else
is not work this reviewer can do, so it must not displace work they can.

**`observed` is only for items no role brought in.** Labelling an assigned
review "observed" would hide that they were asked.

## Ordering

Bucket, then pull-request identity. The identity is what makes the order
*total*: ordered by bucket alone, items sharing a bucket would sit wherever the
host happened to read them, and a queue that reshuffles between refreshes is
one a reviewer cannot keep their place in.

Identity is a tiebreak and not a ranking. Nothing here can say which of two
equally urgent reviews matters more, and it does not pretend to; it only makes
the answer the same every time.

## Refresh

`application::queue_refresh_scheduler` holds no provider and no clock. The host
says what to ask for and reports what came back.

**Backoff is a count of seconds, not a moment to wait until.** A moment would
need a clock to compare against, and the scheduler has to stay replayable: the
same answers in the same order give the same state, whenever they arrive. It
doubles from 15 seconds to a 900-second cap, and the provider's own
`Retry-After` is preferred when it gives one, because the provider knows when
its allowance resets and the scheduler does not.

**A session is host, account, and generation.** Generation alone would let an
account switch that reused it deliver one account's answers under another's
authority. Generation is the host's counter and never a clock, because two
sessions started in the same millisecond must still be distinguishable.

**Identity is checked before anything else is read.** A completion from a
replaced session changes no validator, no backoff, and no online state — those
belong to the session that is current, and an answer from a session that is
gone knows nothing about it.

**Switching account discards every validator.** Carrying an entity tag across
would send it under a different authority, and "not modified" would then be an
answer about a pull request this account may not be able to read.

Primary and secondary rate limits are distinct statuses: GitHub means "you have
used your allowance" by one and "slow down" by the other, and one word for both
would send a reader to the wrong place. Cancellation is neither a failure nor a
reason to back off, or a reviewer switching quickly between reviews would slow
their own queue down.

Rate limits are simulated. What is proven is what the scheduler does with each
answer; whether GitHub sends those answers under the documented conditions is
GitHub's behaviour, and no fixture settles it.

## Enrollment and departure

**Membership is retained as enrolled, never recomputed.** A reviewer watching
`src/parser` keeps the pull request when the last parser file leaves it,
because the reason they are watching has not stopped being true — it is why
they wanted to see the change in the first place. A queue that re-derived
membership on every refresh would drop exactly the pull requests a path rule
exists to catch, silently, at the moment the change became interesting.

Re-enrolling keeps both reasons. A rule match never overwrites a role, or
deleting the rule would silently drop a pull request GitHub assigned; a role
never overwrites the rule, or the reviewer loses the answer to why an
unfamiliar item is in their queue.

Departures are explicit and distinct: `closed`, `access lost`, `gone`, and
`removed`. A pull request that simply vanished would leave the reviewer
wondering whether they imagined it. `access lost` and `gone` stay separate
because one is a permission a reviewer may be able to ask for and the other is
a pull request that is not there any more; collapsing them would send somebody
to the wrong person.

Only a withdrawal moves an item out. A deferred or refused refresh says nothing
about whether the pull request is still there, and treating it as a departure
would empty the queue during an outage.

A departed item is retained with its reason and dropped from the projection.
Reopening restores the same record.

## Notifications

Title: repository and number. Not the pull request's own title, which routinely
names the customer, the incident, or the unreleased feature.

Body: the bucket's own fixed sentence, and a count when there is one. A count
says there is something to read without saying what.

Link: a local reference. Not a URL — a URL opens a browser, which is the one
thing a native review tool exists to avoid, and a link the operating system can
hand to any registered handler is one another application can claim.

`quiet` and `authored, awaiting reviewer action` do not notify. Neither asks
anything of this reviewer, and a tool that interrupted for them would teach the
reviewer to ignore its notifications — after which the ones that matter are
ignored too.

## Presentation

`qt::review_landing_model` matches rows by pull-request identity. An item that
becomes urgent *moves*; it is not removed and re-inserted, and every other
row's persistent index survives.

A positional diff would report a reorder as a run of data changes on rows that
did not change. That is correct only in the sense that a reset is correct: the
view re-reads everything, the selection means nothing, and assistive technology
is told the list it was reading no longer exists.

Re-applying an unchanged queue emits nothing at all, so a periodic refresh does
not look like constant activity.

Badges are textual and pairwise distinct, so a bucket reads in greyscale and
through a screen reader. The reason is the core's sentence; a model composing
its own would be deciding policy while looking like presentation. The matched
rule is named only when it is why the item is here — saying "you watch
src/parser" about a review someone asked for would bury the fact that they were
asked.

The model has no mutating entry point beyond `apply` and `clear`.

## What is not proven here

| Card | What it needs |
|---|---|
| BPR-0168 | A person to grant Notification Center permission once and to see the banner. The harness proves the request's content and destination; it cannot prove the operating system showed it |
| BPR-0169 | A designated repository and token. That the correlator cannot issue a request is structural and settled here; that GitHub's own reviewer state is also unchanged needs a server to answer |
| BPR-0170 | A durable store. Enrollment is retained for the life of the process and no longer; a queue rebuilt on every launch would re-derive membership from whatever the paths look like then, which is the recomputation this design exists to avoid |

Filters and sorting presentation are not built. The model presents what the
core ordered, and a filter is a view concern that would sit above it.
