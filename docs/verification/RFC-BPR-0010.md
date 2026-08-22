# RFC-BPR-0010 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: **observer-rule enrollment, the landing workflow, durable queue
filter/sort preferences, notification requests, and activation selection are
composed.**
BPR-0159 through BPR-0170 delivered the deterministic queue,
refresh, enrollment, model, notification request, and durable store components.
Done BPR-0239 adds the RFC-allocated filters and explicit alternate sorts above
the core-ordered incremental model, persists only their preferences, and proves
the real gesture/restart path.
The public application now supplies user-configured local observer rules,
persists the exact rule and enrollment reason, and incrementally projects the
result through the shipped landing model. It deliberately does not infer a
GitHub role from review or comment history: the accepted snapshot does not
carry the provider's explicit assignment authority. BPR-0223 now retains an
exact first accepted head baseline, forwards accepted queue projections to the
real workbench before requesting a newly warranting metadata-only notification,
and routes only the process-lifetime sink's attributable first target back
through the notifier to exact visible-row selection.
Done BPR-0251 prepares the production tray with a non-null icon and adds a
shipped-executable system-sink course whose only substitution is the native
display boundary. Its tray signal, notifier activation, and exact focused
landing-row selection remain production code. The owner has now accepted
BPR-0168's final-head v006 checkpoint after personally reading and clicking the
native banner; BPR-0168 retains terminal identity replay behind BPR-0169.
The status vocabulary calls Qt's unreadable owner permission
`platform_managed`, never `granted`; it permits a submission attempt without
claiming the owner's answer.

RFC-BPR-0010 stays `Proposed`. No card here makes a closure claim, and closure
remains ordered behind RFC-BPR-0002.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0007 and RFC-BPR-0008. Both remain incomplete and
`Proposed` behind implementation, composition, owner-supplied, and ordered-
closeout gates. Done BPR-0088 records the product owner's direction to advance
the implementation frontier despite that open work.

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
| Observer rules enroll locally | Core plus composed Qt/host/store path | Match repository, author, and path rules, including exclusions for generated and vendor paths; add and remove a repository rule through the shipped window | Enrollment is a pure function; the public gesture persists locally and has no provider mutation path | Verified. BPR-0159's `enrollment_for` takes no provider and `queue_membership` has no field a request could occupy. BPR-0231's two-process `application_queue_composition_contract` adds the rule through the Qt action, persists it, restores its match, and removes it to an empty projection; provider-mutation counters remain zero |
| Every item explains itself | Core plus composed landing model | Read the bucket and exact observer reason after a public rule gesture and restart | The reason is carried by the item and store rather than recomputed by presentation | Verified. BPR-0160's `queue_item` carries the membership and bucket. BPR-0231 persists the exact matching rule and the incremental Qt model presents that retained reason after a separate-process reopen |
| Decisions are clock and network independent | Core under `core-only` | Replay one observation set twice | Identical buckets and ordering, with no clock read and no timer scheduled | Verified. BPR-0160 orders by bucket then identity, so 64 items supplied ascending, descending, and rotated give one queue. The observation carries no timestamp: "new since I last looked" is a comparison against what the ledger records, not against a wall clock |
| Refresh respects the host's limits | Host with manual executor | Drive simulated primary and secondary rate limits, ETags, offline state, and cancellation | Bounded backoff, conditional requests reused, and no work issued while offline | Verified. BPR-0161 doubles backoff to a 900-second cap, prefers the provider's own `Retry-After`, reuses validators so an unchanged pull request costs a conditional request, reports offline explicitly, and treats cancellation as neither a failure nor a reason to back off |
| A completion cannot cross sessions | Host | Deliver a stale completion after an account, repository, or pull-request switch | The completion is refused by identity rather than by ordering luck | Verified. BPR-0161 checks identity before reading anything else, so a stale answer changes no validator, no backoff, and no online state. A session is host, account, and generation: an account switch reusing the generation is still refused |
| Enrollment survives the path it matched | Host plus store | Enroll by path rule, project without re-deriving membership, and read it back from a separate process | The item stays enrolled with its original reason until closed or removed | Verified. BPR-0162 retains membership as enrolled rather than recomputing it, so the pull request a path rule caught stays when the path leaves it. BPR-0170 makes that survive a restart: a forked child writes and this process reads back the rule that brought the item in and the departure reason of one that left |
| The queue updates incrementally | Qt | Replace a queue projection with one differing in a few items | Exact insert, remove, move, and data signals by stable identity, and no routine reset | Verified. BPR-0163's `review_landing_model` matches rows by `pull_request_ref`, so an item that becomes urgent moves and keeps its persistent index rather than being replaced. Re-applying an unchanged queue emits nothing at all. A positional diff — which RFC-BPR-0007's model uses for a different job — would have reported a reorder as a run of changes on rows that did not change |
| Filters and sorting remain deterministic and explainable | Composed Qt landing surface | Filter by role, observer reason, bucket, repository, and quiet/current state; switch directly between each supported sort and reopen the application | Stable identities survive without reset, the default retains core priority, alternate sorting is explicit and total, empty states explain active filters, and only preferences survive restart | Verified. BPR-0239 places a `QSortFilterProxyModel` above the unchanged core order, covers every filter plus a 2,048-row bounded update, directly switches between repository and PR-number orders whose answers disagree, and drives all controls through the shipped window into SQLite schema v3 and a separate-process reopen with zero provider mutation. BPR-0272 retains the exact named Add and Remove presenters, proves disabled prerequisites omit only their unavailable stops, and walks Exclude → Add → Remove → technical details through the declared and live Qt chains when both are enabled |
| Notifications carry no content | Host | Request a notification for a review with comments and a matched path rule | The request type has no field a body, a title, or a path could occupy | Verified. BPR-0164 reads the whole notification and requires it to be exactly identity, the bucket's fixed sentence, and a count — not merely free of a list of bad words. Leaking the matched rule's path takes three deliberate edits, and the test catches it |
| A notification links to the right review | Composed queue, notifier, system sink, and landing proxy | Enroll the existing two-head GitHub.com fixture through the real rule gesture, observe head one as quiet, use the real Refresh action for head two, replace only the operating-system display call, establish the workbench as Qt's logical active window, emit the real tray click signal, and cut the request and activation-selection wires independently on that same refresh | The exact `github.com/octo-reviewer/octo-org/review-repository#17` row moves from quiet to changed-since-my-review and is selected and focused; later submissions are suppressed because Qt supplies no per-message activation identity; hidden or absent identities fail visibly without changing preferences or opening provider navigation | **Verified at the v006 checkpoint.** The owner personally read the exact title/body, observed no private content, clicked the real banner, and the log records exact focused landing selection and exit zero. BPR-0220 later changed only an unrelated appearance-probe expectation; the owner explicitly declined a redundant notification replay. |
| Closure and permission loss are honest | Host | Close, reopen, transfer, and lose permission to an enrolled pull request | Each produces an explicit queue state rather than a silent disappearance | Verified. BPR-0162 keeps forbidden and gone distinct — one is a permission a reviewer may be able to ask for, the other a pull request that is not there — retains a departed item with its reason while dropping it from the queue, and restores the same record on reopening. Only a withdrawal moves an item out, so an outage does not empty the queue |

## Standing composed-host linkage audit

BPR-0248 requires exact demangled constructors for
`bpr::application::review_queue_application_host` and
`bpr::application::review_queue_notifier` in both the development-fixture and
production binaries. This proves that the implementations are linked, not that
they are constructed or invoked. `application_queue_composition_contract` and
the notification activation contract remain the runtime evidence for rule
enrollment, refresh, request submission, and exact-row activation.

The review-mutation linkage inventory is profile-specific. Both profiles
require the asynchronous application-host and coordinator constructors plus
the generic `bpr::application::reconcile` decision used after mutation.
Development fixture additionally requires the exact test-only provider and
testing-grant factory while forbidding the live provider and factory.
Tests-disabled production requires the live provider and factory while
forbidding the fixture component type and testing-grant factory. Independent
construction, grant, selection, and
host-dispatch cuts are the runtime complements; production's empty grant
performs no live observation or submission. The verifier cannot skip a
missing binary or supported inspector, and nonzero tools, failed demanglers, or
malformed or zero-defined tables fail closed. Only parsed defined records count;
`U`/`UND` references and anchors embedded in surrounding template text do not.
Windows declaration prefixes are accepted only through the first allowlisted
calling convention outside balanced template, parameter, and array delimiters;
nested conventions do not select the anchor boundary and unbalanced
declarations fail closed. Absolute, common, debug, and unknown symbol kinds or
sections do not count. CTest treats the verifier's exit status as authoritative.

## Deferred gates

| Card | Gate | What is missing |
|---|---|---|
| BPR-0168 | A native notification is delivered and its deep link opens the right local review | v006 is accepted. The owner waived an interim replay after BPR-0220's unrelated appearance-probe change. BPR-0168 still waits behind BPR-0169 for terminal identity replay |
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
| `sqlite_queue_enrollment_storage` | `dev` |
| `qt_review_landing_model` | `dev` |
| `application_queue_composition_contract` | `dev` |

Both core suites run under `core-only`, which links neither Qt nor SQLite, so
"the core reads no clock and reaches no provider" is a property of the build
rather than of a review.

## Remaining public work

BPR-0223 closes the unattended notification request and activation-selection
composition path. BPR-0251 adds the non-null production icon and repeats that
path through the system sink with only native display captured. BPR-0168's
v006 checkpoint remains accepted, and the owner waived an interim replay after
BPR-0220's unrelated appearance-probe change. Terminal identity replay still
waits behind BPR-0169's distinct live GitHub non-mutation evidence and the
remaining ordered closeout chain.

## Documentation ledger

| Topic | Document | State |
|---|---|---|
| Role semantics, observer grammar and precedence, enrollment retention and departure, the refresh schedule and its limits, notification privacy, offline behaviour, ordering, queue explanations, and keyboard access to observer-rule actions | [Review Queue](../architecture/review-queue.md) | Updated. The composed request/dedup/activation path and exact Add/Remove presenter focus positions are documented, and the remaining attended/live gates stay named without overclaiming |
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
| BPR-0168 | Target-native native-banner display, privacy, activation, and exact landing selection checkpoint |
| BPR-0165 | Documentation: `docs/architecture/review-queue.md` |
| BPR-0166 | Simplification pass |
| BPR-0167 | Closeout preparation |
| BPR-0231 | Public observer-rule enrollment, durable matched reason, restart, and removal |
| BPR-0239 | Durable queue filters and explicit sorting through the composed Qt path |
| BPR-0223 | Composed notification requests, active-transition deduplication, and exact activation selection |
| BPR-0272 | Exact observer-rule button identities, accessibility metadata, and available focus order |
| BPR-0392 | Tests-enabled macOS bundle identity required by Notification Center |
