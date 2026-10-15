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

Review Cycles ([RFC-BPR-0053 through RFC-BPR-0061](../rfcs/RFC-BPR-0053-review-cycles-domain-and-local-authority.md))
add no path into this protocol. A cycle's notes, suggestions, rounds, replies
and verifications are never published as comments, reviews or replies. Its one
GitHub write, creating a new branch and pull request from a selected result, is
a separate publication catalog with its own finite plan, journal and recovery
([domain model](domain-and-state-model.md#review-cycle-identities-rfc-bpr-0053)),
not this composer, and its description is the reviewer's Problem and Solution.
Adding a linked cycle's notes to that pull request's Review Tour is a local tour
edit; the tour reaches GitHub only through its own preview and confirmation.
Once published, the pull request is reviewed here like any other, and its
comments are this page's.

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

The tests-disabled production composition constructs the live provider for the
exact accepted authenticated handoff and snapshot. The acting account and
transport lease are bound to that target and re-observed before submission.
The ordinary editor's core-accepted Publish gesture invokes the private
`review_mutation_user_authority_issuer`. Its grant binds the complete proposal
(including draft/body/disposition), selection and retained diff publication,
session, generation, target and actor. It expires after five minutes and is
consumed once, after host endpoint validation and before effect admission.
Changing any bound request field or replaying the grant refuses without a write.
Each subsequent deliberate Publish needs another exact grant. Cancellation and
retired-host shutdown still stop queued work, and the existing durable provider
reconciles an uncertain publication without a second POST.

Automated builds refuse the production issuer. The public editor fixture uses
request-bound testing grants against its isolated provider. The separately
selected attended mode keeps the compiled designated target and bootstrap;
it cannot fall back to ordinary issuance when its approval is spent or absent.
That bootstrap consumes one approval bound to the exact executable, source,
target and replay namespace. Ordinary application use requires none of those
attended artifacts. These paths share the host and provider; Qt never issues
credentials, grants or HTTP requests.

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
- **Delta** — `reconciliation_entry`: unchanged, added, updated, removed,
  conflicted, or removed_conflicted.

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

A thread observation that returns an explicit `node: null` reports
`target_unavailable`: the selected target is not visible to the account. Invalid
JSON, missing required fields and structurally invalid answers report
`malformed_response`. The GitHub adapter retains that distinction for both
thread preflight and reply reconciliation; malformed native HTTPS responses
retain it too. The application forwards the fixed diagnostic, and Qt explains
account access versus an invalid server response without showing provider text.
Both failures preserve the existing refusal, draft and safe retry behavior;
neither admits a write or treats a failed reconciliation as server truth.

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
Windows enters each test into a zero-capability AppContainer, with the exact
set of tests that need something a lowbox token cannot do - DOS volume names,
a batch stub, or ninja's named pipe - pinned to run uncontained there. The boundary therefore includes Python, CMake,
arbitrary helper descendants, and test-driven production-application launches.
The structural provider inventory remains defense in depth.

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
path identity before resuming it. The application then walks every approval and
replay path component through retained native descriptors without following
symlinks. It validates the approval's exact ordered subject and SHA-256 and the
replay namespace's private mode, schema, path, and random identity. Spending
creates one approval-ID record exclusively in that namespace, syncs the record
and directory, and revalidates the retained record against its name, inode, and
single link before atomically consuming the approval inode and minting one
private 15-minute carrier. The namespace has a fixed record capacity. Absence,
unsafe permissions, links, substitution, replay, capacity exhaustion, failed
durability, timeout, malformed or oversized content, remote failure, a closed
PR, or a moved head yields no grant.

Namespace initialization or reset is an explicit no-path command separate from
attended startup. It creates a fresh random namespace identity, so every prior
approval becomes invalid and a later run needs a newly bound attended approval.
An ordinary or attended launch never silently repairs lost state. The one-use
claim follows the approved local-authority threat model: every detected unsafe
condition fails closed, while intentional same-user or administrator deletion,
rollback, restoration, or mutation of replay state is outside the supported
guarantee.

Durability is a real barrier rather than a hand-off. `fsync` on macOS only
gives the write to the drive, so the record and its directory are committed
with `F_FULLFSYNC` and a grant exists only after that barrier returns. A spend
therefore survives power loss or a kernel panic. Loss the filesystem never
reports, such as media corruption that removes a record while leaving the
namespace intact, is the one accidental case the guarantee does not cover.

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
live lane. BPR-0641 adds the separate ordinary user-publish issuer described above. The designated regression
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
contract. It compiles only four canonical operations: observe the current
viewer, observe the exact pull request, list the bounded first 100 review
comments without pagination, and create an ordinary REST pull-request review
comment with `body`, `commit_id`, `path`, `side`, and `line`. GitHub.com
and GHES 3.18 use their exact REST roots. The core's zero-based semantic line is
translated with a checked `+1` to the REST API's one-based `line`, and response
parsing reverses that translation within the core line type's range. Canonical
request equality rejects a changed method, path, header, or byte of JSON.
Responses are bounded and pass a
duplicate-key and structural preflight before JSON parsing; redirects,
malformed values, wrong identities, wrong anchors, and a wrong author fail
closed.

BPR-0636 migrates these line-comment contracts to private typed Glaze records,
matching the whole-review parser. Database identities are positive exact uint64
values; fractional forms and overflow cannot normalize through a double.
Comment-list reconciliation validates each parsed record directly, without a
serialize/reparse step. Required fields, bounded structural preflight, full-input
consumption and validation of skipped extension values remain enforced. The
line-comment writer preserves the previous canonical member order and lowercase
Unicode escape spelling, including literal escaped backslashes, so existing
request fingerprints and recovery records retain their byte identity.

`validate_outbound_review_mutation_request` checks canonical consistency: it
recompiles the supplied semantic request for the selected server and compares
the entire wire request. It rejects a method, path, header, body or operation
that disagrees with that compilation. It does not independently establish that
the compiler describes a valid GitHub endpoint or document, and does not prove
that the semantic request itself was unchanged or authorized. BPR-0904's
contract tests therefore pin method, exact GitHub.com/GHES path and GraphQL
operation independently for every `review_mutation_operation`; the provider
fixtures answer only matching method/path/GraphQL-operation keys. An unexpected
request fails the fixture immediately. Unconsumed scripted answers in refusal
courses do not establish operation coverage.

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

For a new write attempt, the protocol first records the sorted, unique node IDs
of every exact actor/anchor/path/commit/line/body match in the bounded comment
snapshot and durably admits that baseline with the canonical write fingerprint.
Only then does it observe the acting viewer and re-observe the complete
pull-request identity and head as the final network operations before POST.
Base OID/ref and head ref remain exact identity fields;
a well-formed changed head OID is returned as the observation and produces the
typed `stale_head` result before POST rather than being collapsed into a generic
identity mismatch. The provider serializes attempts and retains at most
1,024 idempotency records. Each record binds the idempotency key to a fingerprint
of the complete canonical write request, including the resolved path. A known
success returns the same GitHub comment identity as a duplicate only when that
fingerprint also matches; a same-key request with another resolved path is
refused before observation or POST. An admitted row begins `prepared`. It
remains in that non-recoverable phase for pre-dispatch refusal, cancellation,
and process death before transport creates the POST operation. Only the native
transport can advance it to `possibly_transmitted`: immediately after
`QNetworkAccessManager::post` returns a reply, a one-shot callback must commit
that phase before response processing continues. Failure to commit the witness
aborts the reply and deliberately risks a false negative rather than
attributing another client's comment. Once any proxy candidate returns a POST
reply, a retryable proxy failure does not advance to another candidate; doing so
would create a second POST after the first had become indeterminate. Read-only
GET operations retain bounded proxy fallback. A prepared or baseline-incomplete
attempt performs no recovery network operation and is never resubmitted.

Post-attempt and restart reconciliation is available only for a durably
`possibly_transmitted` attempt. It subtracts the durable baseline from all exact
matches and resolves only when exactly one new node ID remains. Zero or multiple
new IDs, pagination, an oversized snapshot, baseline-integrity failure, or a
missing legacy baseline remain guarded without another POST. A successful POST
response remains a direct receipt only when it reproduces the exact path,
commit, side, line, body, and acting identity and its node ID was absent from
the durable baseline. The SQLite authority repeats that last baseline check
before recording the receipt.

The markerless protocol cannot distinguish BetterPullReview's write from an
identical comment another client creates after baseline capture. If POST is
indeterminate and exactly one such new identity exists, it satisfies the same
delta rule. Avoiding that attribution limit would require hidden metadata or a
provider idempotency field, neither of which ordinary GitHub review comments
offer.

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
BetterPullReview publishes no private marker, so an indeterminate POST is never
automatically resubmitted. The exact-scope SQLite recovery store persists the
canonical request plus its pre-attempt exact-match baseline. The originating
process and every later process use the same GET-only baseline subtraction;
schema-v1 unresolved rows have no trustworthy baseline and therefore remain
permanently guarded, while their already successful receipts remain readable.
Schema v2 binds the encoded baseline to its SHA-256 digest and verifies the
digest before decoding; admission commits the attempt, baseline, digest, and
prepared phase in one transaction. This detects corruption inside the
owner-private database. It is not a claim of tamper resistance against an actor
who can rewrite both data and digest.

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

A deletion is a change too. An edited comment the server no longer reports
leaves the projection like any other, and its entry is reported
`removed_conflicted` rather than `removed`, so the reader learns that the edit
has nowhere to go ([[BPR-0888]]). `conflicts()` lists both kinds.

No shipped feature holds an unpublished edit of a published comment yet.
Drafts are new comments and thread replies, and a published comment is out of
every draft command's reach (see [Drafts](#drafts)). So the production refresh
passes an empty edited set, and neither conflict kind can occur there today. A
feature that adds editing of published comments must pass its edited set to
`reconcile`, and must give both kinds a surface that shows the reviewer the
text they were holding.

Refreshing the same state twice produces an identical result, not merely an
equivalent one: the projection and entries are ordered by identity.

## Capability fallback

A provider that cannot resolve threads or record viewed state reports that
rather than skipping the operation. Silently skipping would leave a reviewer
believing they had resolved something.

## Credential acquisition has no impact here

RFC-BPR-0016 adds a second way to acquire the credential and changes nothing
about what this document describes. The round trip's authority is unchanged:
publication still runs under the acting user's identity, through the same
provider, over the same native HTTPS transport, using a credential the
platform vault holds. A token the device grant enrolled and a token the
operator pasted are the same `github_credential_material` by the time the
mutation host sees one, and the mutation host cannot tell them apart
([[BPR-0281]], `application_device_authorization_registered_course`).

Nothing in the draft, anchoring, safe-target, retry, or reconciliation rules
consults how enrollment happened, and no device-flow type appears on this
path. This is an evidence-backed no-impact region, not an untested one: the
registered course drives a real enrollment through the composed application
and the round-trip composition contract is pinned across it
([[BPR-0516]], `application_review_round_trip_composition_contract`).

## What is not proven here

The remaining mixed-client and whole-review attribution criteria require the
live product path and scoped write authority. BPR-0142 already completed the
source-comment/web-reply round trip and owner appearance check. BPR-0143 has
a designated disposable PR; BPR-0144 still needs a target that permits approval
by the chosen reviewer. Neither remaining live course is claimed complete.

| Card | What it needs |
|---|---|
| BPR-0143 | The same repository plus a second client, to confirm GitHub produces the external shapes the reconciliation handles |
| BPR-0144 | A real account whose login is known in advance, so the observed review author can be compared to it rather than merely being non-empty |

The fixture proves what BetterPullReview sends and how it reacts. Only GitHub
can show what GitHub makes of it.


## Whole-review application host

BPR-0615 adds `review_submission_application_host` beside the source-line host.
It accepts one operation at a time and owns its stop source and exact session,
generation and request context. Provider construction, storage opening,
discovery, execution and destruction occur on the effect worker. Each accepted
operation posts one terminal to the serialized decision lane, including explicit
cancellation and shutdown. A busy, closed or unauthorized call is not accepted
and queues no effect. The composition must retain the decision executor and
projection through quiescence and reject retired session/generation before
inspecting a terminal's summary.

Whole-review authority is an independent, opaque, revocable, one-use grant. It
binds session, generation, draft/attempt, complete authenticated target/actor,
summary, disposition and retry key. Source-line grants cannot be substituted.
The private whole-review issuer is selected by the normal authenticated Submit
review handler after its core command is accepted for the exact displayed draft.
This is the implementation's product assumption; no owner verdict is fabricated
and it does not authorize an agent to run a live publication. Test builds refuse
production issuance and use separate fixture grants. Production composition
selects the live provider and SQLite store; test composition selects an explicit
typed fake server and real SQLite. Each provider/store opens and closes on the
whole-review effect worker. Authentication invalidation retires this workflow
independently of the file-review ledger.

Read-only discovery/reconciliation need no new publication grant and cannot
select a submit effect. Discovery validates every recovered request's stable
actor/PR scope before publishing the bounded list. Provider results with stale
draft/attempt, a different request or an invalid receipt become an indeterminate
result for the original request. A stop after confirmed publication cannot erase
its receipt; a provider exception or timeout cannot establish nonpublication.

## Whole-review workbench controls

The conversations drawer contains a separate Review pull request editor with
Comment, Approve and Request changes choices, a summary, Submit review, Discard,
Check status and New review. It presents the host, owner/repository, PR number,
original expected head and actual acting account before submission. Identity and
outcome text are literal, selectable and keyboard-copyable. Width follows the
visible drawer rather than the wider scroll content; the native palette and font
remain inherited. Presentation emits core commands and receives decided controls.

`review_submission_workflow` composes this editor, the core draft and the async
host. File navigation and identical accepted refreshes preserve the draft.
Changed heads freeze the original draft until discard/new review; a different
account or PR receives a new empty draft, without copying the old body into a
new authority. An admitted pending attempt stays in its original durable scope.

Submit creates one request/key after the core accepts the exact displayed draft.
Pending controls cannot submit or discard it. Reopening discovers the original
request and reconciles it read-only, including after head movement. An unknown
outcome stays submitting and exposes Check status. Only a validated receipt
completes that attempt; a definite fresh-submission refusal retains the summary.
New review is explicit, and remaining restored attempts are inspected before
starting a fresh review. Retired host terminals cannot replace current state;
shutdown retains the projection and decision lane until all hosts quiesce.

`application_review_submission_composition_public_path` drives the normal PR
open, file selection and review controls against a typed fake server and real
SQLite. It covers all three dispositions, duplicate gestures, refusal, stale
results, lost response, reconstruction at a newer head with zero additional
submissions, fake-account invalidation and shutdown with a held terminal.
Independent provider fresh-process recovery evidence remains in BPR-0614/0615;
the application reconstruction fixture does not claim abrupt-kill durability or
live GitHub attribution. The Qt editor is checked at 100%, 125% and 200% scale.

## Whole-review provider boundary

BPR-0610, BPR-0613 and BPR-0614 add a separate whole-review path. Its core
`review_submission_draft` owns the PR/head, acting account, summary, disposition
and exact attempt. It has no source-line anchor. Comment and Request Changes
require nonblank summaries; Approve permits an empty summary. A definite
nonpublication can return to drafting; an indeterminate write stays submitting.
The application request and result retain the original draft/attempt and
idempotency key, with a distinct review receipt rather than a comment receipt.

The GitHub session records a complete pre-send review-ID baseline, durably
admits the exact request, re-observes the viewer then the PR/head, and permits
one POST only after its durable transmission witness. It reads server reviews
after a possible send regardless of the POST response. Success requires one
exact new review outside the baseline and an atomic durable resolution. An
already admitted key never causes another POST. Prepared or baseline-incomplete
records cannot adopt a later review; stored success returns its original receipt.

The review-list compiler/parser uses private typed Glaze DTOs. It collects all
review IDs while matching only submitted reviews with the exact actor, head,
summary and disposition. Pending and dismissed reviews cannot count as success.
The initial supported list is one complete page of at most 100 reviews;
Link headers or response limits refuse submission/reconciliation rather than
pretend that a partial baseline is complete.

SQLite schema v3 adds `submission_attempt` and `submission_baseline` beside the
existing comment tables in `review-mutation-recovery.sqlite3`. A versioned,
bounded binary snapshot preserves the complete request and full uint64 IDs.
Both factories validate the shared schema; transactional v1/v2 migration
preserves source-line records. Admission, witness and resolution are atomic.

The recovery port discovers unresolved records atomically
within its exact account/repository/PR scope, bounded by the existing 1,024-record
capacity. Provider discovery validates the complete actor and target identity
and returns each original request and idempotency key. Discovery is an effect-lane
operation; a fresh draft with the same prose is not a recovery request. Prepared
records remain unresolved because another process may still own their send.

For an already admitted, possibly-transmitted request, read-only reconciliation
re-observes the authenticated actor and stable repository/PR identity. It can
then find the original-head review even if head, base, refs or server open state
have changed. The original body, disposition, head, baseline and key still decide
the match. This path never sends a POST. Missing local recovery records or
wrong recovery authority cannot establish definite nonpublication. A new request continues to require the
full current expected target and head immediately before publication.

Focused evidence: `github_mutation_contract`, `live_review_submission_provider`,
`sqlite_review_submission_recovery_storage` and
`live_review_submission_restart_recovery`, with existing source-line regression
selectors. Restart fixtures use fresh processes and normal exits, including a
prepared-state exit; they do not simulate abrupt termination or power loss.
The three new SQLite-backed fixtures use the existing Windows DOS-volume-name
exception because they call the unchanged retained-ancestor path. That cause
reuses BPR-0362 evidence; no new native Windows run is claimed. macOS/Linux
registrations remain network-contained.

## Semantic source-line publication policy (RFC-BPR-0022)

The provider-free `review_publication_policy` contract It
consumes exact host eligibility and bounded placement/capability/body facts.
Legacy placement always selects native publication; unknown placement also
selects native even when the repository's off-hunk capability is unsupported.
Fresh scoped unsupported evidence plus proven off-hunk placement first asks
for durable evidence admission. It cannot directly prepare a fallback child.

Durable native rejection and first-attempt evidence admission are distinct
states. Only either committed authorization can prepare a fallback. A native
transmission witness selects native reconciliation and cannot enter either
fallback admission path. Unknown results, cancellation, timeout, malformed
success and GET absence supply no terminal event. An exact non-creation terminal
may retire a witness; only the exact unsupported event authorizes fallback.
Success is terminal and retains its representation for duplicate receipt replay.

Fenced suggestion bodies require a line subject. The core conservatively
recognizes backtick/tilde fences and whitespace in the info string; a required
fallback returns typed incompatibility. Native publication remains available.
The provider and transactional recovery compose this contract. Publication continues
through RFC-BPR-0008's coordinator and exact user or attended authority.

### Immutable permalink codec (BPR-0564)

The GitHub adapter's `review_comment_permalink` codec accepts an exact retained
right-side endpoint minted by the epoch host, its file-comparison key, the
connected target, and the text line count observed for that blob. It checks the
endpoint's authority, comparison, algorithm, path and blob before interpreting
a remote line. Compilation additionally requires the current target head;
rehydration can use an exact historical retained endpoint. The caller still
proves changed-file membership and a file-subject root in the expected PR.
Parsing alone never grants publication authority.

The visible grammar is one bare canonical HTTPS permalink, two LF bytes, then
non-empty UTF-8 reviewer prose. The codec preserves accepted prose byte for byte
and caps the combined body at 64 KiB. It rejects leading extra blank lines,
control bytes, and a second code-permalink-shaped URL in the prose, preserving
one unambiguous semantic anchor. It never follows a URL or performs a mutation.

Path segments use uppercase percent escapes, with slash only between segments.
Dot segments, backslashes, controls, malformed UTF-8 and paths over 4 KiB are
refused. The parser compares the complete authority/path/commit/query prefix to
its canonical compiler form; alternate ports, userinfo, escaped separators,
foreign repositories, abbreviated OIDs and query/fragment additions therefore
cannot normalize into a trusted anchor. Fragments are canonical positive decimal
`#L` numbers, converted to zero-based semantic lines only after the exact blob's
line bound passes.

The closed Markdown-renderer suffix set is `.md`, `.markdown`, `.mdown`, `.mkdn`,
`.mkd`, `.mdwn`, `.mdx` and `.litcoffee`, matched without case. These use
`?plain=1`; other source paths have no query. This set follows the [GitHub
Markdown renderer](https://github.com/github/markup/blob/master/lib/github/markup/markdown.rb)
observed on 2026-09-26. Provider publication, recovery and read normalization use this one codec.

The review-tour extension (BPR-0998) calls the same canonical path/prefix builder
through a separate read-only codec. It admits full commit-pinned single lines or
inclusive `#Lfirst-Llast` ranges, LF/CRLF first-line separation and historical
source. The old single-line publication parser retains its existing contract.
Tour parsing has no retained blob or comparison authority: the host binds the
address to exact admitted material before selecting source lines. See
[review tours](review-tours.md#exact-source-and-compatibility).

### RFC-0022 logical journal (schema 4)

Schema 4 adds `logical_publication` transactionally after exact validation of
schemas 1–3. Existing native attempts, baselines, whole-review attempts and
receipts keep their original bytes and semantics. Legacy recovery remains the
native path; a migrated unresolved attempt is never turned into fresh admission.
The new journal stores one versioned bounded binary parent with at most two
children and a SHA-256 digest covering the complete record, including baselines.
Both journals share the 1,024-publication limit per authority scope.

A parent retains its logical fingerprint and legacy alias. The alias is unique
within its scope: an existing legacy row refuses new logical admission, and a
logical parent refuses legacy admission under that alias. Each child key is
SHA-256 over `bpr-review-child-v1:`, the parent key and either
`:native-wire-v1` or `:file-wire-v1:permalink-v1:body-v1`. Child fingerprints bind
the exact canonical wire; the parent fingerprint binds the logical contract.
All integers in the stored binary format are unsigned 64-bit little-endian;
text is an eight-byte length followed by exact bytes and digests are 32 bytes.
The binary format starts with revision 1. Stored enum and text bounds are checked
before use; a digest-valid but invalid state is still refused.

Compare-and-swap commits exactly one core-legal state transition and its child
witness or receipt. Fallback requires a previously committed exact native
non-creation rejection, or an initial scoped-evidence admission with no native
child. Its authorization stores the contract/scope digests and, for cached
observations, generation and observed/expiry seconds. A native transmission
witness cannot be replaced by cached evidence. A succeeded child and its receipt
commit together; a baseline ID cannot become that receipt. Prepared children
that survive interruption remain conservatively non-retryable. The session may
reconcile only a possibly transmitted child; it never treats GET absence as
permission for another POST.

### RFC-0022 serialized negotiation

The mutation host passes the retained endpoint, comparison key, text extent and
placement only after its exact-source check. The existing coordinator carries
that source through the consumed gesture grant to the serialized provider
session. New root publications use the logical journal; replies retain the
existing thread path and legacy identities resolve through their old rows.

Each child observes actor and target before admission and again immediately
before POST. The transport's one-shot witness commits the corresponding
possibly-transmitted state before permitting the write. A complete exact
non-creation response commits native rejection before the file child can be
prepared. First-attempt cache evidence instead commits its own admission without
a native child. Prepared children found on restart do not POST; possibly-sent
children reconcile through GET and their persisted baseline. Successful parents
return their exact receipt. Neither a lost response nor a generic 422 can enable
fallback. Cancellation after transmission retains one bounded reconciliation.

Capability evidence is session-owned, expires within 300 monotonic seconds, and
is keyed by the full authority, operation/API/classifier revisions and identity
generation. The key also binds the exact commit, path and line: a successful or
unsupported anchor does not establish repository-wide behavior. A new session
starts unknown for new logical attempts; already committed evidence admission
remains authority for that same immutable attempt after restart.

`github_review_comment_permalink` composes the actual session and SQLite journal
with canonical fixture wire exchanges, including GitHub.com/GHES routes,
negotiation, exact-anchor cache reuse, interruption at each durable boundary,
legacy/logical alias protection, cancellation and lost-response recovery.
`application_review_mutation_application_host` pins the retained-source handoff;
`native_https_transport_fixture` pins complete 422 delivery and truncated 422
refusal. These fixtures are separate from the r3 owner-approved recorded
contract, whose exact identity/source/response bounds are owned by the
[provider contract](github-provider-contract.md#off-hunk-publication-wire-contracts-rfc-0022-milestone-b).
Revision2 cannot reinterpret an existing unresolved native journal.

### File subjects and selected source placement (RFC-BPR-0022)

The accepted read model retains GitHub's `FILE` or `LINE` subject. The
application's `review_comment_placement` adds a separate semantic anchor only
for a file thread with one undeleted root whose visible canonical permalink
matches the selected, sealed retained text endpoint and changed-file catalogue.
It decodes the retained raw Markdown observation and keeps display text and
byte-preserved reviewer prose separate. Missing raw Markdown leaves the thread
at file level; rendered `bodyText` cannot establish or retain an anchor.
Replies inherit root placement without decoding their own bodies. Closed pull
requests remain readable, while the permalink publication compiler still
requires an open target.

The per-window decoder cache is bound to the complete selected source and each
root's canonical identity/raw body. Refresh starts from server truth: link edits
remap or remove placement, deletion removes the thread, and missing source
material leaves file-level placement. No local database or publication journal
is required to reconstruct the link. The Qt model preserves canonical IDs and
checks a compatibility anchor's exact comparison before revealing its line.
Native historical revision mapping remains RFC-BPR-0021's obligation.

The normal conversation reader presents reviewer prose for an admitted semantic
file comment. Its optional, collapsed Support details explains the remote file
subject and code-link placement. The selected source and conversation preserve
one identity across refresh, including retained endpoints that have an exact
semantic anchor but no native current-diff coordinate. Qt indexes those admitted
anchors for reverse selection; it does not decode a permalink or decide a
publication representation.

A successful root publication requests an ordinary pull-request refresh. The
receipt supplies only the identity to select once the accepted snapshot names
that post. Selection is bound to the exact target, acting identity and retained
comparison and is abandoned on a mismatch. Both native and file-subject
publications use this path; the server read owns the remote subject, body and
canonical thread identity.

### Current designated attended target

BPR-0573 rebinds only the compiled attended course to owner-designated
`MikeMarcin/BPR-Test#2`, exact base/head and node identities recorded in the
[RFC-0022 ledger](../verification/RFC-BPR-0022.md). Historical PR1 packets and
results remain evidence for their original builds and targets; they do not
authorize this candidate or a new PR1 launch. Any queued PR1 course needs its
own source/artifact reconciliation before execution. The ordinary user-publish
route and dynamic actor validation are unchanged.

## Durable composition drafts (RFC-BPR-0023 M7)

`review_composition_application_host` performs bounded local operations on the
private store's serialized effect lane. It reloads exact selected identities,
creates reviewer/copy candidates, returns exact target partitions and unassigned
sources, and accepts a selected batch in one transaction. Accepted candidates
are the body authority for the existing `revisioned_review_draft`; an edit adds
an immutable reviewer successor and advances the draft CAS in the same commit.
No mutation or process port is available to this host.

A `durable_draft_commit` is the store acknowledgement passed to
`shared_draft_application_host::adopt` on the decision lane. The existing shared
owner binds the runtime scope, validates profile/account/PR/head and ledger
incarnation, then fans out accepted revisions. It refuses stale acknowledgements,
volatile edits and volatile allocation once durable storage is selected. New
ordinary inline drafts use `allocate_draft` on the same monotonic store frontier.
Storage refusal produces no accepted acknowledgement or projection. Restore
reattaches exact durable values; a `publishing` value remains pending and carries
no execution grant. M8 supplied asynchronous production wiring and UI translation.

Agent-assisted composition obtains an exact workspace intent and uses the one
`agent_invocation_host`. Admission revalidates workspace version, source set and
revision binding before retaining the run. A correlated complete output can
append only a proposal; a changed/deleted workspace or advanced source retains
the run terminal without attaching a candidate. Accept and Publish remain
separate operations. The store and private run port outlive all invocation
claims and effects, as specified by the M6 lifecycle contract.


### Separate durable publication join

`private_publication_application_host` loads an already publishing draft into
the existing core proposal. Its optional local identity binds the private ledger
incarnation and exact draft revision into the idempotency key. Legacy proposals
keep their existing keys. These local fields never enter a GitHub request body.
A separate explicit Publish still traverses the existing coordinator, exact
source/actor/target validation and RFC-BPR-0022 logical journal.

Before a child is admitted, a storage-only lineage port commits the exact draft
and composition revision, private binding, actor, logical ID, idempotency key,
approved-prose digest and compiled wire-body digest. Native-to-file negotiation
updates that same pending row. Pending rows are not successful publication.
An exact server reread must agree with the receipt before the join and matching
publishing draft are completed in one transaction. A failed local commit leaves
the remote success visible with lineage pending.

The provider's explicit `reconcile_publication` operation loads an existing
logical journal and performs only reconciliation reads and local commits. It
cannot admit or POST a child. A reopened caller supplies the retained exact
original request and source endpoint; fresh text alone cannot reconstruct
recovery authority. Repeated repair uses the original canonical server identity.
Production also queues a read-only pending-join pass when the live mutation host
opens or reports pending lineage. It reads the current PR and viewer identity,
then the bounded comment collection. Each historical pending row must match its
own journal child, wire digest, actor, path, accepted commit, subject and line;
an outdated line can use the server's exact original commit and original line;
an unresolved child also needs its pre-write comment baseline. This pass may
finish a local join after the PR head advances. Missing, ambiguous or mismatched
observations leave the join pending, and the pass has no POST branch.
The local host exposes bounded lineage pages by draft and duplicate-source/body/
target advisories, with server IDs. An advisory cannot suppress intentional Publish.

The store, lineage host and all borrowing providers must share one serialized
effect lane and outlive its drained invocations. The existing mutation terminal
carries lineage state to the decision lane; M8 supplied production construction,
recovery discovery and rendering. M7 fixture courses exercise the actual store,
coordinator, provider and journal, but do not supply designated live-target or
signed-recording acceptance. Public prose stays byte-preserved; the authorized
file representation adds only its canonical visible permalink.


### Durable workbench drafts (RFC-BPR-0023 M8)

A source-line selection first opens one empty runtime placeholder per window consumer. It is not a durable ID, a recovery record or publication authority. Browsing lines writes nothing. The first edit is applied through the existing core reducer and committed with the newly allocated durable draft in one transaction. A refused first save keeps the editor text with an explicit retry; publication remains unavailable until an accepted durable revision exists. Text typed while that save is pending continues on the acknowledged identity.

The workbench serializes private draft persistence before adopting its projection
into the shared in-process draft owner. Each incoming gesture must still name the
current runtime scope, draft and revision. A text edit awaiting acknowledgement
cannot be published; conflicting local text remains available for explicit
Reapply or Reload. Reply captures the exact thread and comparison line while a
draft loads, and navigation clears that continuation. Ordinary replies may name
an exact left-side thread; composition acceptance continues to require a current
right-side target.

A definite preflight failure returns a draft to editing. A submitted operation
whose result is uncertain keeps its durable publishing revision and read-only
body. Retry publication reuses that exact identity through the existing logical
journal. A request retired before delivery to the mutation host is compensated
on its original storage context. A successful terminal adopts the committed
receipt before requesting ordinary shared conversation synchronization.


The **Saved comments** action on Finish review reads the exact account/PR store independently of current-head placement. Earlier-comparison text remains read-only and names its original head; using it on current code requires an explicit copy and new comment gesture. An uncertain publication is labelled separately and directs the reviewer to Check status before reusing its text. This inspection never rebinds an anchor or admits a remote write.

### Conventional Comments header edits (RFC-BPR-0048, BPR-1082)

A label is text at the start of the draft body. No store, draft record or
window keeps a label, decoration or annotation beside that body; every reader
recognizes the saved bytes again. `core::plan_feedback_draft_edit` turns one
chip, menu or shortcode edit into the command the draft owner already accepts:
an ordinary whole-body `edit` carrying the revision the author was shown and no
composition link. It then travels the path any typed edit takes. A durable draft
is committed by `review_composition_application_host::change_draft`, which
records a linked draft's reviewer successor, and adopted by the shared owner. A
volatile draft goes through `shared_draft_application_host::apply`.

The plan refuses before any write when the draft has another revision or body
than the author saw (`stale_draft`, with the current body's annotation as the
conflict candidate), and when the draft is not `drafting` (`not_editable`). A
publishing draft therefore keeps the exact body its proposal, grant and
idempotency key were prepared with, including across a restart and a retry
after a lost answer. A new label after a definite failure is a new revision, a
new proposal and a new key, and needs a new publish gesture. An edit that races
another window or store connection fails the existing revision check, and the
newer text is kept.

`core::decide_feedback_composition_edit` is the header decision for a body
being written. It adds one case to `decide_feedback_header_edit`: a blank body
takes the header alone, as in `issue: `, and a body that is exactly such a
header can be relabelled, redecorated or emptied, so a label may be chosen
before the subject is typed. A local tour stop is planned by
`plan_feedback_tour_stop_edit` as the existing `edit_stop` command, so the tour
owner's revision check, undo journal and request receipts serve the desktop
and `bpr tour` alike. The Qt controls are BPR-1083's. The headless
comment-draft command is BPR-1092's; until it lands, an ordinary comment draft
has no CLI path.

## Strict CLI ordinary publication (RFC-BPR-0079)

A deliberate Comment, Approve, Request changes or inline Publish checks the exact
current actor, host, repository, PR, head and prepared content before issuing its
one-use grant. CLI admission requires the retained wire's strict acquisition
capability; provider admission repeats that gate. Whole-review and inline paths
repeat current admission after asynchronous durable draft/comment holding,
immediately before grant issuance. Read trust, discovery and remembered startup
supply no publication authority. Tour Publish/Resume retains its immutable preview
and separate description choice, without an additional CLI modal.

`credential_unavailable` means the chosen credential is absent or inaccessible;
`credential_tool_failed` means the tool failed before publication dispatch.
Both keep the draft and name the failed operation's account/host in the existing
status. The latter comes directly from acquisition, without collapsing through
an account-observation wrapper. Generic cancellation and attempted unknown
outcomes retain their existing recovery semantics. Unknown writes reconcile
GET-only, never a second POST. Prior staged/partial tour records survive a fresh
acquisition refusal; their status says no further publication rather than
claiming that earlier successful writes did not occur. Confirmed receipts and
unknown journal outcomes take precedence over a fresh failure.

Inline draft completion combines typed prior/current publication knowledge in
core. A retained publishing draft represents an unconfirmed earlier attempt;
a retry refused during strict admission or the coordinator's target observation
cannot return it to editable drafting. The host keeps its exact body, anchor,
durable revision and retry identity even when that check never reaches journal
loading. The status describes the earlier result as unknown and separately names
the current credential/tool/cancellation/capability refusal. A fresh credential
refusal keeps an editable draft. Only an exact confirmed receipt resolves the
locked unknown draft; restored credentials use the existing same-key GET-only
reconciliation without another ordinary comment POST.

The native inline status uses its measured wrapping height at the current card
width. Changed notice content invalidates editor/footer/card measurements; native
layout requests notify the existing drawer height callback. This updates preferred
height without rebuilding the card or model, moving focus, or changing the locked
publication state.

Whole-review reconciliation settles retained comments from the complete durable account/PR scope, including earlier heads absent from the editable projection. Core requires the original held key, draft identity and revision, head, path, side, range and exact body; the host also checks the proposal actor and target against the store scope. Restore discovers held review keys before filtering the current head, so a restart can retry a failed local settlement through the existing read-only publication lookup. No new publication grant is created.
