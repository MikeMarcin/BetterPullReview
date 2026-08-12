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

The shipped `workbench_window` composes that editor and emits its exact
`semantic_selection`. Its text and Publish actions enter the core transition
path. The application host admits only a current right-side selection whose
comparison, selected file, workbench generation, selected-diff identity,
epoch-sealed resolved endpoint, repository path, commit, and designated head
all agree. A caller cannot replace that endpoint with another plausible rename,
copy, or arbitrary third path. The host then owns one drain task, so coordinator
and provider effects remain serial even when its executor runs tasks in
parallel. It also owns one stop source per accepted request, supersession,
stale-terminal withholding, and shutdown quiescence.

If selection changes while a publication is in flight, composition retains the
publishing draft, retains the latest emitted exact selection, requests stop on
the first divergence, and waits for the matching terminal. A sequence that
returns from A to B to A therefore adopts A, not the earlier B. Every matching
terminal clears pending state and refreshes server comments before the deferred
selection is presented. An
indeterminate result retains one exact retryable draft identity without
replacing the deferred draft; it is restored only for the same semantic anchor,
comparison, and workbench authority, and cleared after success or authority
retirement. Typed Qt clearing removes stale body, receipt, actions, and terminal
status without emitting an edit. Qt never treats a submitted request as server
truth.

The tests-disabled production composition retains the same asynchronous host
shape. It can construct the live provider only after the accepted handoff and
snapshot match the compiled designated host, owner/name, repository and
pull-request database and node IDs, number/open state, base repository, base
OID/ref, and head OID/ref. The acting database ID, node ID, and login are
deliberately not compiled allowlist constants. They are taken from the exact
authenticated session, cross-bound to its account/profile/host lease and
accepted snapshot, and re-observed by the provider. An ordinary production
launch has no execution authority and refuses before queuing an effect. The
only production issuer is the attended bootstrap described below: it consumes
one owner-private approval bound to the exact shipped executable, source
surface, designated target, and public target observation. It then retains one
opaque authority until the host validates and atomically admits one exact
request. No boolean, environment variable, credential, raw nonce, persisted
setting, IPC message, or ordinary Qt gesture can mint that authority.

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
- **Request context** — request, application session, and workbench generation
  identity assigned by the asynchronous host.
- **Execution authority** — an opaque capability scoped in this slice to the
  application session, workbench generation, designated target, and acting
  identity. The host separately validates the exact comparison, file, anchor,
  resolved endpoint, and commit. The attended carrier is atomically consumed
  immediately before the first exact effect is queued; invalid attempts do not
  consume it, and cancellation, invalidation, timeout, or shutdown revoke it.
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
construction and never replaceable. The asynchronous host is the only composed
caller. It derives the provider anchor from the exact selected-diff right path
and current-head commit; callers cannot name a different destination through a
proposal.

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

The default configured CTest manifest has no live exception. An explicitly
enabled build may register exactly one exception,
`live_review_mutation_designated_target`, with the sole label
`bpr_live_mutation`. Configuration requires four already-resolved absolute
paths: an existing approval artifact, the signed-capsule supervisor, the
capsule inventory, and the separate Release producer directory. Registration
proves that the supervisor and inventory are inside the exact producer tree,
that the producer cache says `BUILD_TESTING=OFF`, `BPR_BUILD_QT=ON`, `Release`,
and this source root, and that neither artifact predates product source or
build configuration. The test command is only the supervisor plus
`--attended-launch`, the inventory, and the approval path. It has a fixed
30-minute timeout, no pass/skip/disable inversion, and no credential
environment.

The supervisor first verifies every signed capsule file, dependency, resource,
symlink, mode, Qt lookup rule, Mach-O load path, and signature against the
immutable inventory. It then performs the narrow unauthenticated public target
observation before creating the GUI process. The app is created suspended from
the owner-private capsule; the supervisor compares the child's dynamic signing
identity with the prevalidated static identity and revalidates the retained
path identity before resuming it. The application then opens the approval and
its owner-private parent through retained native descriptors, bounds and
validates the exact ordered subject and its SHA-256, descriptor-relatively
moves the checked entry to an exclusive claim name, compares and unlinks the
same inode, proves that the retained descriptor has zero links, durably records
the random approval ID as spent, and mints one private 15-minute carrier.
Substitution, replay, timeout, malformed or oversized content, remote failure,
a closed PR, or a moved head yields no grant.

BPR-0235 requires that public observation to finish before the Qt application
process exists, so it cannot use `native_https_transport`: that adapter owns a
Qt network manager and requires the application runtime the check is meant to
precede. The prelaunch observation is therefore a separate, fixed GitHub
adapter effect implemented with Apple's `/usr/bin/curl`. This is not an
authentication or mutation transport. The production operation has no target,
argument, environment, proxy, credential, or trust-store input. It opens
`/usr/bin` and `curl` without following links, requires root-owned non-writable
native identities, validates every architecture against `anchor apple and
identifier "com.apple.curl"`, retains the file identity, starts the absolute
path suspended with an empty environment, compares the child's dynamic code
digest to the retained static digest, revalidates the path identity, and only
then resumes it. There is no shell or path search.

The command is an exact HTTP/1.1 `GET` of the compiled GitHub.com REST URL with
TLS 1.2 through 1.3, redirects disabled, both proxy selection mechanisms
disabled, curl configuration disabled, and fixed accept, API-version,
identity-encoding, language, and user-agent headers. It never enables netrc,
cookies, authentication, or compression. Production uses curl's Apple system
trust; a private CA and loopback resolution exist only in the tests-enabled
translation unit. Curl's own connect/total limit is 15 seconds, the parent
kills and reaps after 17 seconds or cancellation, headers are bounded at 64
KiB, body at 256 KiB, and stderr at 16 KiB. Typed failure distinguishes
unavailability, network failure, timeout, cancellation, redirect, response
limit, HTTP failure, malformed input, and identity mismatch. The parser then
matches the complete immutable public target. Architecture inventory makes
this the only source allowed to name the curl executable or its Apple trust
requirement and leaves every other shell/process API forbidden. Authenticated
reads and all writes remain owned by the existing native HTTPS adapter.

The asynchronous host binds that carrier to the application session, workbench
generation, designated target, authenticated actor, comparison, file,
right-side semantic anchor, resolved path, and current head. Validation occurs
before supersession, and the one atomic consume is the final admission step
immediately before effect enqueue. Invalid requests neither consume the grant
nor cancel an already admitted request; the first exact request consumes it and
every later request refuses. Shared revocation state survives retained copies
and is triggered by cancellation, invalidation, timeout, and shutdown.

The earlier BPR-0235 runner, gate, fork protocol, and harmless canary remain
unchanged as hermetic regression evidence. They are not the live test command
and cannot be selected as its shipped application. The default graph and all
other tests retain the process-level network-denied wrapper. Neither the live
lane nor any credential or mutation was executed while preparing this path.
Target-native Linux and Windows evidence remains deferred, and the Windows
approval bootstrap remains fail-closed; this work does not close BPR-0142 or
BPR-0235.

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

On 2026-08-10 the product owner authorized creation of the disposable live-test
pull request and designated the resulting identity before any BetterPullReview
live child or review mutation ran. The only designated pull request is
**`MikeMarcin/BPR-Test#1`**, GitHub database ID **`4246446933`**, node ID
**`PR_kwDOTyvof879G6NV`**, head OID
**`17990ba0c2fb50338141f35c0717f0f99639c7ba`**, and base-repository ID
**`1328277631`**. Its recorded state is open, its base is `main` at
`e2a44aa8fef704de7373463222b1872e796918c6`, and its head branch is
`codex/disposable-live-review-20260810`. The branch contains only an expressly
disposable text fixture and no production or private content.

Those values are the immutable allowlist input, not evidence that the live lane
has been implemented. Before creating a child, the lane must re-read and match
the canonical owner/name, repository database and node IDs, pull-request number,
pull-request database and node IDs, base-repository ID, open state, and current
head OID. Any mismatch, closed PR, or moved head is a no-launch result. The head
must be revalidated immediately before any write as well.

BPR-0252 now provides the first production implementation of
`review_mutation_port`, so the former structural argument — that only test
implementations existed — is intentionally gone. The implementation is linked
into the production GitHub adapter. BPR-0254 first composed it as a dormant
exact-target object. BPR-0255 now places that object behind the asynchronous
application host and coordinator described above. BPR-0256 adds the sole
attended one-shot issuer and registers a signed-capsule prelaunch supervisor
around a separately built tests-disabled shipped application as the default-off
live lane; ordinary launches still have no grant. The designated regression
runner still contains only its fixed
harmless canary, and no live lane, credential, or write has been used. The
`live_mutation_isolation` gate is now the standing proof that survives the
implementation:

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

The safe-target check itself stays in the composition root immediately before
the provider and asynchronous host are constructed. The host repeats exact
request binding before admission, and the coordinator/provider re-observe the
target during execution. The provider does not decide its own designation.

The application host exposes only an opaque mutation transport lease minted for
one accepted snapshot. Replacing the pull-request identity, profile, account, or
accepted snapshot and shutting down the host revoke the shared monotonic state
seen by every retained copy. A revoked lease makes both the live factory and the
provider protocol return `unavailable` before transport use. Separately, each
accepted asynchronous publication owns a monotonic stop source. Supersession,
explicit cancellation, and shutdown request stop; the same token reaches both
coordinator observations, the live provider session, and native HTTPS. A lease
revocation remains an admission denial rather than the cancellation mechanism.

### Portable provider protocol

BPR-0252 adds a provider-free live binding to the application request. It binds
the core target to the canonical owner/name, repository and pull-request
database and node identities, pull-request number, base-repository identity,
open state, expected head, exact acting account database/node/login identity,
and one semantic source-line anchor resolved to an exact path and commit. The
coordinator accepts that binding only when the resolved semantic anchor equals
the core proposal and the commit equals the expected head. The old incomplete
request form remains solely for the fixture provider; the live provider refuses
it before any transport operation.

`github_mutation_contract.{hpp,cpp}` is separate from the read-only request
contract. It compiles only three canonical operations: observe the current
viewer, observe the exact pull request, and create an ordinary REST pull-request
review comment with `body`, `commit_id`, `path`, `side`, and `line`. GitHub.com
and GHES 3.18 use their exact REST roots. The core's zero-based semantic line is
translated with a checked `+1` to the REST API's one-based `line`, and response
parsing reverses that translation within the core line type's range. Canonical
request equality rejects a changed method, path, header, or byte of JSON.
Responses are bounded and pass a
duplicate-key and structural preflight before JSON parsing; redirects,
malformed values, wrong identities, wrong anchors, and a wrong author fail
closed.

`native_https_transport.cpp` remains the only mutation network-effect owner. It
adds one narrow mutation operation to the existing account-bound transport,
reuses its TLS, proxy, deadline, cancellation, credential, response, and manual-
redirect controls, and accepts `201` only for the comment POST. Neither the
provider nor its contract constructs a network manager, reads the environment,
or acquires credentials. Their outcomes contain only typed failure categories,
never credential material or provider diagnostics. The mutation port carries
the asynchronous host's stop token through viewer observation, exact-target
observation, and the POST, so cancellation before or between phases prevents
every later operation.

For a write attempt, the protocol observes the acting viewer first, then
re-observes the complete pull-request identity and head as the final network
operation before POST. Base OID/ref and head ref remain exact identity fields;
a well-formed changed head OID is returned as the observation and produces the
typed `stale_head` result before POST rather than being collapsed into a generic
identity mismatch. The provider serializes attempts and retains at most
1,024 idempotency records. Each record binds the idempotency key to a fingerprint
of the complete canonical write request, including the resolved path. A known
success returns the same GitHub comment identity as a duplicate only when that
fingerprint also matches; a same-key request with another resolved path is
refused before observation or POST. An attempt whose POST outcome is
indeterminate is retained as guarded and is never resubmitted by that provider
instance; avoiding a duplicate takes precedence over an automatic retry. This
protection is conservative and process-local. Durable restart reconciliation
remains composition work. The POST response must reproduce the exact path,
commit, side, line,
body, and acting identity before its GitHub node ID becomes the receipt.

Hermetic provider tests exercise this production protocol through its
separately linked session object rather than linking the concrete live-provider
symbol into a test executable. This is deliberate: the BPR-0235 binary guard
continues to reject that symbol from every non-production executable while the
tests still drive the same validation, observation, idempotency, and response
logic the thin concrete adapter delegates to.

## Identity

The acting identity travels in the request as the application account plus the
exact GitHub database ID, node ID, and login observed for the authenticated
credential authority. The request also carries the coordinator's own complete
designated target. The live protocol re-observes and compares that viewer before
every POST. Reviews use ordinary GitHub semantics and add no marker or hidden
metadata.

That the identity is *carried* is proven against the fixture. Whether GitHub
attributes the review to that account is GitHub's behaviour and needs a real
account — see BPR-0144.

## Retries

A request whose answer was lost is indistinguishable from one that never
arrived, so retrying is unavoidable and duplicate comments are the risk.

`idempotency_key_for` is a pure function of the draft identity, the target, the
anchor, the disposition, and the body. Two retries of one publication produce
the same key and the provider recognises a known success; two genuinely
different comments cannot collide, because the draft identity is part of it. A
key that ignored the body, the anchor, or the head would let different comments
share one. GitHub exposes no ordinary review-comment idempotency field, and
BetterPullReview publishes no private marker, so an indeterminate POST is
guarded against automatic resubmission for the life of the provider instance
rather than guessed to have failed. Reconciliation across process restart is
not provided by this uncomposed slice.

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

Three acceptance criteria still require the live product path, session-scoped
write authority, and in two cases a person. The repository and disposable PR
identity are now designated; that designation is an allowlist input, not a
completed live or attended run.

| Card | What it needs |
|---|---|
| BPR-0142 | Composition of the implemented provider into the exact-target live lane, a write-scoped token, and a human to confirm the comment on designated PR `#1` is readable, replyable, and unmarked in the GitHub web UI |
| BPR-0143 | The same repository plus a second client, to confirm GitHub produces the external shapes the reconciliation handles |
| BPR-0144 | A real account whose login is known in advance, so the observed review author can be compared to it rather than merely being non-empty |

The fixture proves what BetterPullReview sends and how it reacts. Only GitHub
can show what GitHub makes of it.
