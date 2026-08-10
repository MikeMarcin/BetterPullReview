# Review Round Trip

BetterPullReview's interactions go through the ordinary pull request. A comment
this tool publishes is a GitHub comment: readable, replyable, editable, and
deletable by anyone with access, from any client, with no marker identifying
where it came from. There is no parallel collaborative protocol, no bot, no
app, no label scheme, and no hidden metadata.

That constraint is the point of the whole RFC. Everything below is in service
of it.

Inspection-style defect semantics are layered over these threads as a local
projection; see [Defect Workflow](defect-workflow.md). Nothing there changes
the shared protocol described here.

## Ownership

| Region | Owns |
|---|---|
| Functional core | Draft and publish state, review command legality, the shared/private distinction, target and head revalidation policy |
| Application host | Synchronization, mutation coordination, acting identity, request execution, idempotency, retries, reconciliation, safe-target enforcement |
| Qt adapter | Editors, pending-review presentation, target confirmation, progress, errors, and action binding |

The core emits **proposals**, never requests. It carries no host, no endpoint,
and no credential, and its suites run under the `core-only` preset, which links
no provider. Keeping those out of the core is what stops a policy decision from
quietly becoming a network call.

The Qt editor is constructed with dispatch callbacks and nothing else. There is
no provider to hold, so a widget calling a mutation endpoint is not something
to guard against — it is something the type cannot express.

The shipped `workbench_window` composes that editor. Its text and Publish
actions enter the core transition path, and accepted proposals reach the
application coordinator. The automated product path constructs a provider only
inside the test-build fixture and fixes the coordinator to the exact accepted
handoff target and account identity. After every mutation outcome the
application re-reads the fixture's server-comment projection; Qt never treats
the submitted request as server truth.

## Typed vocabulary

- **Command** — `core::review_draft_command`: edit, discard, begin publish,
  complete publish, fail publish.
- **Decision** — `core::review_draft_outcome`, carrying the resulting draft, a
  typed rejection, and whether anything changed.
- **Proposal** — `core::review_publication_proposal`: what is to be said, where,
  and with which disposition.
- **Request** — `application::review_mutation_request`, adding the target, the
  acting identity, and the idempotency key.
- **Completion** — `std::expected<review_mutation_receipt,
  review_mutation_failure>`.
- **Observation** — `review_comment_projection` records the server reports.
- **Delta** — `reconciliation_entry`: unchanged, added, updated, removed, or
  conflicted.

## Drafts

A draft is private until it is published. `shared()` is derived from reaching
`published` rather than being a settable flag, so there is no state in which a
draft is partly shared, and a draft in flight — out of the reviewer's hands but
not yet on the pull request — cannot be mistaken for a public one.

`publishing` is a state rather than a moment. Publication is not instantaneous
and the interval is observable; without that state a draft would be either
editable or published, and a second publish issued while the first was in
flight would look legal. During the interval the body cannot change, the draft
cannot be discarded out from under the request, and a second publish is
refused.

A failed publication returns the draft to composition **with its text intact**.
Throwing away what a reviewer wrote because the network misbehaved would be the
tool losing their work.

Once published, no draft command touches the comment. Editing a published
comment is a different operation against a different authority; letting a draft
command reach it would create a second, local truth about text GitHub owns.

A body of whitespace is refused. Publishing one would put an empty bubble on
someone's pull request.

## Anchoring

Comment and draft anchors use RFC-BPR-0004's semantic source-line, boundary,
mapping, or change-group identities. A draft that remembered where a line
appeared on screen would attach itself to the wrong text as soon as anything
above it changed. Mapping uncertainty stays visible; the UI cannot attach a
comment to an arbitrary neighbouring line.

## Stale heads and wrong targets

Before publishing, the target the draft was prepared against is compared to the
one the server reports. The two possible mismatches are **not equally
recoverable**, and conflating them would be the dangerous simplification here.

A **moved head** is the right pull request at a newer revision. The reviewer
can be shown the new revision and say to go ahead, so an explicit
acknowledgement permits it.

A **changed target** — a different host, repository, or pull request — can never
be acknowledged away, whatever the reviewer clicked. The acknowledgement they
gave was about a different pull request, and treating it as consent for this
one would be inventing agreement they never expressed.

An **unnameable head** is refused outright: a revision nobody can name is not
one a reviewer can have agreed to.

The target is compared **before** the head, because a comment aimed at the
wrong pull request is a worse mistake than one aimed at the right pull
request's newer revision, and reporting the head first would hide it.

## Safe targets

The mutation coordinator is built for exactly one designated target, fixed at
construction and never replaceable. `publish` takes a proposal and nothing
about where it goes, so no caller can name a destination.

The target is observed **twice** — once to decide whether publication is
permitted, and again immediately before submitting. A check performed only at
the first point passes for a target that changes afterwards, and that window is
what an automated run could slip through. The test makes the window real: the
fake reports the designated target on the first observation and a different
repository on the second, and the statistics show two observations with zero
submissions.

**No automated test may mutate an undesignated live repository or pull
request.** Every test in this RFC runs against the fixture provider. The three
criteria that genuinely need a live target are carded and unstarted; see below.

The executable boundary no longer depends on mutation-port names alone. Every
CTest registration crosses one process launcher before its original command.
On macOS, the trusted runner derives a one-test sandbox profile from a template
that grants only runner-selected free loopback ports and one fresh mode-0700
Unix-socket directory; an occupied-port race makes the fixture bind fail before
its client runs. Unrelated loopback and Unix listeners remain denied. Linux has
a fail-closed user/network-namespace launcher awaiting target-native replay, and
Windows refuses all original test commands until a validated whole-suite
boundary exists. The boundary therefore includes Python, CMake, arbitrary
helper descendants, and test-driven production-application launches. The
structural provider inventory remains defense in depth.

There is currently no network-enabled CTest exception. The eventual one named
live lane cannot exist until the repository identity below is supplemented by
an immutable pull-request identity and both are validated before its child is
created.

Weakening the coordinator's target check to let a test through would remove the
property the test exists to exercise. It is not an acceptable shortcut under
any deadline.

## The live provider contract

The designated repository is **`MikeMarcin/BPR-Test`**, GitHub database ID
**`1328277631`** and node ID **`R_kgDOTyvofw`**, named by the product owner on
2026-08-08. It is the only repository any BetterPullReview run may mutate.
Naming it here rather than in a card is deliberate: BPR-0142 requires the
target to be named in documentation *before* the first live run, not after.
The live-lane identity check must match both the canonical owner/name and these
immutable identifiers.

A read-only GitHub API inventory on 2026-08-10 found no pull request in the
designated repository. That observation authorizes no mutation and supplies no
pull-request identity. A pull request must be created and explicitly designated
before the live lane can be registered; its number, immutable pull-request ID,
head OID, base-repository ID, and open state must all match before child launch.

Until BPR-0142 lands, "no automated test can mutate an undesignated live
repository" holds for a structural reason — `review_mutation_port` has test
implementations only, and the shipped composition constructs its coordinator
only in the test-build fixture path. The fixture provider has no transport,
endpoint, credential, or production build path. That argument dies the moment
a live implementation exists. The `live_mutation_isolation` gate replaces it
with one that survives:

| | Fixed by contract |
|---|---|
| Class | `live_review_mutation_provider` |
| Factory | `make_live_review_mutation_provider` |
| Namespace | `bpr::adapters::github` |
| Files | `src/adapters/github/src/live_review_mutation_provider.{hpp,cpp}` |

These names are load-bearing, not conventional. The gate asserts three things
against them, and a live implementation that uses any other name or path fails
the first one rather than quietly becoming reachable:

1. **Inventory.** The set of files implementing `application::review_mutation_port`
   is a subset of `{the test fake, the live provider's two files}` — and the
   fake must be found, because an inventory that reads nothing satisfies a
   subset check perfectly.
2. **Unreachability.** No first-party non-production executable contains a
   `live_review_mutation_provider` symbol. The binaries are enumerated by the
   build itself: every `bpr_add_executable` target not prefixed `app_` is
   collected into a generated manifest, so a future `bench_`, `demo_`, `fuzz_`,
   or `tool_` target is covered the day it is added. The `review_mutation_tests`
   binary must be in that manifest and must contain `review_mutation_coordinator`
   symbols, which is what distinguishes a clean symbol table from an unread one.
3. **Construction.** `make_live_review_mutation_provider` appears nowhere under
   `tests/`, and under `src/` only in the live provider's own two files and the
   composition root `src/app/src/github_application.cpp`.

The production binary is deliberately excluded from clause 2. End-to-end tests
execute `app_better_pull_review`, so after BPR-0142 the shipped application will
contain live code while being driven in fixture mode. The runtime complement is
BPR-0142's own work: a fixture-driven run emits `REVIEW_MUTATION_PROVIDER:fixture`,
asserted in the end-to-end pass expressions. Symbol absence and runtime
selection are different claims and neither substitutes for the other.

BPR-0235 adds the runtime complement at the process boundary: a test-build-only
application course invokes the shipped native HTTPS adapter against an ordinary
external read endpoint and must receive a typed network failure from the
sandbox. Outside that boundary the endpoint returns an HTTP response, so the
course cannot pass merely because the application selected a fixture provider.

The safe-target check itself stays in the composition root, immediately before
the coordinator is constructed. Not in the coordinator, which cannot know what
is documented; not in the provider, which must not police itself.

## Identity

The acting identity travels in the request, and the request carries the
coordinator's own designated target. Reviews are published under the
authenticated user with ordinary GitHub semantics and no additional metadata.

That the identity is *carried* is proven against the fixture. Whether GitHub
attributes the review to that account is GitHub's behaviour and needs a real
account — see BPR-0144.

## Retries

A request whose answer was lost is indistinguishable from one that never
arrived, so retrying is unavoidable and duplicate comments are the risk.

`idempotency_key_for` is a pure function of the draft identity, the target, the
anchor, the disposition, and the body. Two retries of one publication produce
the same key and the provider recognises the repeat; two genuinely different
comments cannot collide, because the draft identity is part of it. A key that
ignored the body, the anchor, or the head would let different comments share
one.

## Synchronization and the mixed-client contract

GitHub IDs are canonical. BetterPullReview keeps a projection for its own use
and re-reads server state after mutations; it never invents an identity,
because an identity this tool made up is one GitHub could not confirm.

Where the server and the projection disagree about text, **the server wins**.
The projection exists to mirror the server, not to argue with it. A comment the
server no longer reports leaves the projection: retaining it would show a
reader a comment that is not there.

An external edit **updates** rather than duplicating. GitHub keeps the identity
across an edit, so the projection records an edit version as well — identity
alone cannot tell a reader that the text changed under them.

The one case that is not resolved automatically: when a comment carries an
**unpublished local edit** and the server has also changed it, the server's
text still becomes the projection, and the entry is reported `conflicted` so a
person decides what happens to the edit they were holding. That is a refusal to
resolve rather than a resolution — discarding either side quietly would lose
someone's words. Holding an unpublished edit on a comment the server did not
touch is *not* a conflict, or every unsaved draft would raise a false alarm on
every refresh.

Refreshing the same state twice produces an identical result, not merely an
equivalent one: the projection and entries are ordered by identity.

## Capability fallback

A provider that cannot resolve threads or record viewed state reports that
rather than skipping the operation. Silently skipping would leave a reviewer
believing they had resolved something.

## What is not proven here

Three acceptance criteria cannot be met without an immutable designated pull
request within the documented repository, a write-scoped token, and in two
cases a person. The repository is designated, but no immutable PR identity or
session authority is available today.

| Card | What it needs |
|---|---|
| BPR-0142 | A designated safe repository, a write-scoped token, and a human to confirm the comment is readable, replyable, and unmarked in the GitHub web UI |
| BPR-0143 | The same repository plus a second client, to confirm GitHub produces the external shapes the reconciliation handles |
| BPR-0144 | A real account whose login is known in advance, so the observed review author can be compared to it rather than merely being non-empty |

The fixture proves what BetterPullReview sends and how it reacts. Only GitHub
can show what GitHub makes of it.
