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

The shipped composition owns one serialized queue host and a private SQLite
store. The Qt rule controls emit semantic add/remove intents; store and refresh
work run on the effect lane; only typed terminals update the retained rules,
enrollments, and incremental landing projection. A separate-process contract
adds a repository rule, observes its exact match, reopens the same store, and
deletes the rule to an empty queue. An accepted author/viewer actor match establishes presentation ownership for an
already enrolled PR, including one held only by Follow or an observer rule.
Ownership is separate from enrollment reasons, so Unfollow and rule removal keep
their original effect. Opening alone does not enroll a PR.
Review/comment history never substitutes for a direct review assignment.

## Typed vocabulary

- **Observation** — `core::pull_request_observation` (who, where, which paths)
  and `core::queue_activity_observation` (what has happened since).
- **Report** — `application::queue_activity_report`, binding one activity
  observation to the ref it was read about. Projection pairs activity to
  enrollments by ref in any order; a report about an unenrolled ref moves
  nothing, and an enrollment without a report is `detail unknown`, never
  quiet ([[BPR-0989]]).
- **Rule** — `core::observer_rule`: repository, author, or path, including or
  excluding.
- **Decision** — `core::queue_membership`, carrying the GitHub role, whether a
  rule matched, and which one, beside the personal reasons: authored and
  directly requested (found by discovery), followed (a local choice), and
  reviewed (the viewer reviewed a discovered pull request).
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

Home's My role filter checks all accepted membership reasons, not just the
single legacy role field. Authored and Requested discovery match their roles;
retained Reviewed membership or an observed submitted review matches Reviewer.
An item can match several roles; No role matches only when none applies.
Observed review membership affects the row projection without turning a
rule-only enrollment into a durable personal enrollment. Opening a PR computes
changed-since-review from its accepted head and submitted review commit on the
first read as well as later refreshes. The Reviewing chip records which PR is
open and does not clear attention ([[BPR-1414]]).

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
| detail unknown | Nothing about it has been read yet. |

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
review "observed" would hide that they were asked. A discovery reason counts
as a role here: a pull request found as authored or requested is never
"observed".

**Unknown is not quiet.** An enrolled pull request whose activity was never
read is `detail unknown`, last in the order, because nothing is known to ask
anything of the reviewer — and not `quiet`, because nothing is known to be
settled either. A directly requested pull request is `initial review
requested` whatever else is unknown: the request is itself the fact. An
authored pull request is `authored, awaiting reviewer action` when an accepted
request is outstanding or the server reports required review outstanding.
Unread activity retains its higher precedence. Concurrent merge/check blockers
remain visible and do not make a waiting row quiet.

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

Departures are explicit and distinct: `closed`, `access lost`, `gone`,
`removed`, and `undiscovered`. A pull request that simply vanished would leave the reviewer
wondering whether they imagined it. `access lost` and `gone` stay separate
because one is a permission a reviewer may be able to ask for and the other is
a pull request that is not there any more; collapsing them would send somebody
to the wrong person.

Only a withdrawal moves an item out. A deferred or refused refresh says nothing
about whether the pull request is still there, and treating it as a departure
would empty the queue during an outage.

A departed item is retained with its reason and dropped from the projection.
Reopening restores the same record.

**Discovery removes only what it read the absence of** ([[BPR-0989]]). A
discovery answer adds and refreshes the authored and requested reasons of one
saved repository. Only a complete answer for the current session may withdraw
a reason it no longer finds, and the pull request departs as `undiscovered`
only when no role, rule, follow or review still holds it — a claim about what
the last complete search found, not that the pull request closed. A partial,
failed or stale answer adds and never removes. A followed or reviewed pull
request stays when its request disappears.

## Personal discovery

`application::personal_discovery_host` ([[BPR-0989]]) is the one discovery
scheduler of a profile, beside its queue host; every window sees what it
finds through the queue's projection. It adopts one account session — host,
account and identity generation, named exactly as the queue names its
session — and searches the saved repositories of that server and account only,
both reasons for each through `personal_discovery_port` ([[BPR-0988]]), at
most two reads at a time.

It runs when a session is adopted, on activation when the last refresh that
ran to its end is older than five minutes, when a repository it has not searched is saved (at
the end of a running refresh if one runs), and on an explicit Refresh. Nothing a window opens or
selects starts it, and opening a pull request never waits on it. A rate limit
holds it off until the retry-after has passed (a secondary limit without one
waits 60 seconds); a Refresh inside that window is refused and says so. A
different session cancels what is in flight; every read is tagged with its
session generation and refresh, so a completion from an earlier session or
refresh, or for a repository no longer saved, is dropped. The queue checks the
session again when it applies an answer.

Activation measures freshness from the last refresh that ran to its end,
not only from the last success ([[BPR-1681]]). A refresh that ended
with an unreadable repository or pull request would fail the same way again,
so every return to the window (closing the diagnostic export's dialog, say)
re-ran the whole refresh; on a slow server that is minutes of reads. A refresh
a session-wide failure stopped (offline, authentication, rate limit) does not
count, so activation still retries it once any retry-after has passed. The
status stays stale with its problem either way, and Refresh is unchanged.

`queue_discovery_status` is published in the queue projection, per server and
account: refreshing, current, partial (with why: GitHub's incomplete results,
the one-page result limit, or the detail limit), stale (with the failure and,
for a rate limit, the retry-after), or unavailable (no account session, or no
saved repository). Cached rows stay through every failure. The last successful
refresh is kept for the session only: after a restart the stored rows return
at once and the first refresh under the adopted session supplies it again.

A discovery answer is stored before it is shown. A refused write rolls the
update back and reports the queue's store unavailable, exactly as any other
queue write; it is never answered from another profile's store.

**One read per pull request per refresh** ([[BPR-1680]]). A refresh reads its
retained pull requests before it searches, so a pull request both retained and
found was read twice. A search now carries the pull requests of its repository
that this refresh's retained reads read in full (`personal_discovery_known`,
decided by `core::discovery_known_pull_requests`: found open, with the Home
summary unbounded). The adapter returns a found row for one of them `Reused`,
with no requested-reviewer, pull request or summary read and no detail read
spent; the host fills it from the retained read. A requested row stays only
when that read named the viewer as a user (`core::keeps_reused_row`), exactly
as the requested-reviewer read would have decided, and a `Reused` row for a
pull request the request did not know is dropped. Each reused row saves the
six requests of its own detail and summary reads on a real server.

**Diagnostics** ([[BPR-1680]]). Each refresh records a `github.discovery`
start and end in the profile's diagnostics log, and each read a start
(verbose detail) and an end. The detail is one `core::discovery_diagnosis`
line of fixed keys, enums and counts:
`discovery=SCOPE refresh=N read=N reason=REASON state=STATE repositories=N
pull_requests=N reads=N failed=N requests=N wait_ms=N run_ms=N`. A read's end
carries the wire requests it sent, how long it waited for its turn on the
provider's lane and how long it then ran, as the provider measured them
(`personal_discovery_cost`); on a GitHub CLI connection each of those requests
also costs the account lease's viewer reads, which are not counted. A
refresh's end says how it ended (current, partial, stale, stopped, or
cancelled by a session change) and sums its reads, failures, requests and lane
waits, with its whole duration. Refreshes and reads are numbered, not
identified, so the lines correlate without a request identity, and nothing in
them names a host, repository, account or pull request. The logger drops a
discovery event whose detail is anything else, and the anonymized export keeps
the line only when it parses back exactly.

**Composition** ([[BPR-0990]]). The profile runtime owns the discovery host
beside its queue host, over the authenticated provider's personal reads. It
adopts `personal_discovery_session_of(accepted_profile_session())` after each
GitHub presentation — the same session again changes nothing, a public-preview
or signed-out session adopts none — and after each queue projection posts one
coalesced `reconcile()`, so a repository saved from any window is searched at
once and a forgotten one only stops being searched. Each window sends its
Home's Refresh and Home's activation as `queue_discovery_intent` values, and
the Pull Request menu's Follow as `queue_follow_intent`; no widget reaches the
provider.

**Forgetting a repository withdraws what was found only there.** The queue
host treats a forgotten saved repository as a complete answer that finds
nothing for either reason, so its discovered pull requests depart as
`undiscovered` unless a role, rule, follow or review still holds them. Kept,
they would be rows no search would ever refresh.

## Notifications

Title: repository and number. Not the pull request's own title, which routinely
names the customer, the incident, or the unreleased feature.

Body: the bucket's own fixed sentence, and a count when there is one. A count
says there is something to read without saying what.

Link: a local reference. Not a URL — a URL opens a browser, which is the one
thing a native review tool exists to avoid, and a link the operating system can
hand to any registered handler is one another application can claim.

`quiet`, `detail unknown` and `authored, awaiting reviewer action` do not
notify. Neither asks
anything of this reviewer, and a tool that interrupted for them would teach the
reviewer to ignore its notifications — after which the ones that matter are
ignored too.

Every accepted handoff reports `ReviewedByMe` when it carries a current-viewer
submitted review, including the first open in a new process.
`HeadMovedSinceMyReview` compares that review's commit with the accepted head.
Opening or reopening cannot erase that comparison; it stays true until accepted
provider facts make it false. The composition retains the exact session and PR
identity for notification deduplication; replacement or invalidation clears that
identity and activity. No review fact is inferred from opening or viewing a PR.

The composition root forwards each accepted queue projection unchanged to the
real workbench first, then considers a warranting `(queue session,
pull-request identity, bucket)` transition only when the projection agrees
with that exact accepted activity. A transition is requested once while it
remains active. This deduplication is bounded to the active process projection
and is never durable state. The request uses the exact known unseen-comment
count, or zero when the composed observation has no such count.

`QSystemTrayIcon` does not identify which displayed message produced its
`messageClicked` signal. The production sink therefore fails closed for its
entire process lifetime: it submits at most the first actionable payload,
sets a non-null application or Qt fallback icon before exposing the tray, and
retains that exact payload for the one attributable activation. Qt does not
report the owner's Notification Center permission; the sink records the
attempt as `platform_managed`, leaves prompting and display to the operating
system, and never reads `supportsMessages()` as permission. Every later payload
is suppressed without
calling `showMessage`; duplicate or delayed clicks do nothing, and activation
never rearms the sink.

The shipped application's deterministic native-notification course replaces
only `showMessage` with a capture boundary. The real system sink still receives
the composed request,
the real tray emits `messageClicked`, the notifier receives the retained exact
target, and the shipped landing proxy selects and focuses the complete
host/account/repository/pull-request identity. Independent cuts remove the
request wire and activation-to-selection wire. That course proves composition,
not that macOS displayed a banner; the latter remains BPR-0168's attended fact.
Because an unattended Cocoa command-line process may remain natively inactive,
the testing-only course establishes the real workbench as Qt's logical active
window before emitting `messageClicked`. It then requires the unchanged
notification activation and landing proxy to produce the exact queue-row focus;
an independently connected testing-only observer records exactly one tray
signal, and an observation cut proves that evidence is required. This does not
claim native foreground activation.

A submitted native activation returns through `review_queue_notifier::activate` and asks
the real landing proxy to select the exact visible identity. The queue row is
selected and focused without changing presentation preferences. A filtered or
departed identity fails in the queue status text; activation never clears a
filter, invents a provider route, or opens a browser.

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

`qt::review_landing_filter_model` is a presentation layer above that ordered
source. Its default disables proxy sorting and therefore preserves the core's
exact priority order. The reviewer may explicitly select repository or pull
request number order; both include the complete pull-request identity as a
deterministic tie-break. Direct switches between alternate sorts force the
comparator to run again rather than relying on an unchanged Qt sort column.

Role, retained observer-rule kind, core-decided attention bucket, repository
owner/name, and quiet/current filters compose without changing the source
queue. Repository filtering deliberately groups the same owner/name across
host and account authorities; the item identity remains complete, but this
presentation preference answers “show this repository name” rather than
selecting an authentication authority. Empty filtered results say that rows
remain in the queue. Controls are named, described, and included in focus order.

The workbench retains the exact Add and Remove `QToolButton` presenters rather
than rediscovering them through their actions. Their stable widget identities
and explicit accessible descriptions join the declared shell sequence after
Exclude and before pull-request technical details. The structural declaration
retains both positions; available-stop evidence filters visibility,
enablement, and `TabFocus`, so a disabled action is absent without a second
focus-order authority. Add remains available only for a ready queue, a typed
rule dispatcher, and a valid trimmed pattern. Remove additionally requires an
accepted persisted-rule selection. Enabling either prerequisite restores the
same presenter at the same position rather than creating another stop.

The application host stores only the bounded preference vocabulary after the
effect succeeds, then publishes the durable choice. SQLite schema version three
adds one singleton preference row; it stores no derived membership. Version
seven ([[BPR-0989]]) adds the authored, requested, followed and reviewed
reasons and the `undiscovered` departure to each enrollment, and the `detail
unknown` bucket to the stored filter; version six migrates in place. The
two-process public contract changes every filter and the alternate sort through
the real Qt controls, reopens the same store, and observes the exact preference
with zero provider mutations.

## Acceptance scope and pending observations

Owner amendment r4 closes the implemented local queue and notification
composition scope separately from the following standalone observations.
The accepted native checkpoint remains bound to its recorded executable; local
fixtures do not establish live server state or a new platform banner.

| Card | What it needs |
|---|---|
| BPR-0168 | v006 native banner text/privacy/click/selection is accepted for executable `sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28`. Terminal candidate identity replay and any required successor verdict remain Backlog behind BPR-0169 |
| BPR-0169 | A disposable PR on which the actor is neither author nor reviewer, plus an independent client able to read reviewer, requested-reviewer, subscription and label state before and after each local rule. Remembered sign-in exists; the live comparison remains Parked and unobserved |

## Public-preview boundary

A public-preview snapshot supplies no current-viewer role, account-relative
review baseline, or triage authority. The host invalidates the preceding queue
identity when entering or leaving that mode. Qt labels queue-role and private
review-progress controls unavailable; an absent role is never guessed from
review/comment history. Reading public metadata or REST comment groups does not
create an actionable review queue or authorize publishing.

## Home waiting observations (RFC-BPR-0070)

Home retains a bounded typed summary independently of private activity. It names
up to eight code-owner actors, eight other requested actors and eight failed
checks, with separate people/team request totals and the server's review decision.
The complete roster remains in Overview. The summary uses current-head checks;
test-merge evaluation and private-organization/GHES runtime parity remain unqualified.

Background discovery adds metadata, request/assessment and check/status reads,
without files, commits, conversations or diffs. Added discovery reads share a
32-attempt/8 MiB response budget across one search answer. Existing search limits
(100 rows/30 detailed PRs), retained limits (30 PRs/400 reviews) and two concurrent
reads remain. Retained reads use one aggregate 32-attempt/8 MiB budget; a review
continuation after 400 records is refused before dispatch. Exhausted discovery
summaries are partial. A limited retained summary also reports partial detail
while preserving its complete title, review evidence and REST request count. Every call uses the selected account transport and retains
its resource rate observations.

The profile queue host reserves dispatch order for background and window reads.
An older result cannot replace newer waiting facts. Local read receipts and defect
updates preserve unobserved fields. A newer complete roster can establish empty;
an unknown review decision cannot settle a retained requirement. Changed revisions
or unavailable observations label retained assessments/checks as last known.

Enrollment schema 9 stores length-framed bounded Home metadata and its numeric
account alongside existing reasons and Draft fields. Older tables migrate with
unknown Home facts. Accepted provider facts remain available in memory if their
optional metadata write fails, with the storage failure still explicit. Restored
facts are last known and admitted only under the same accepted numeric account;
legacy authored membership needs a new bound observation.
That freshness includes a retained failed check from a partial collection even
when the complete check set, requests and review assessment were never observed.
The codec and host restoration cannot turn that partial failure into a current
result. A complete REST request fallback replaces prior names/counts independently
of unavailable GraphQL code-owner origin, including a known empty fallback.
Qt derives its native wrapped row reason, accessible description and collapsed
blocker counts from the projection. Quiet filters retain known blockers, and My
role includes authored discovery. Existing notification categories, local
Follow/observer intent and Recently viewed authority remain unchanged.
