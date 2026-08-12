# RFC-BPR-0008 Verification Plan and Ledger

RFC base: `7ea6c9ab7b0878706ca3df83390ad3a2e5b7e91a`

Status: **the fixture-safe product round trip is composed; macOS default-deny
test isolation is directly verified, while cross-platform and attended/live
acceptance remain incomplete.** BPR-0135 through
BPR-0139 delivered the draft policy, target revalidation, mutation coordinator,
synchronization, and Qt editor against fixture/component surfaces. BPR-0225
found no construction or invocation of those surfaces in the public
application. BPR-0229 now connects the shipped editor gesture through core,
the exact-target fixture coordinator, server refresh, and Qt projection.
The BPR-0221 structural guard remains defense in depth. BPR-0235 now places the
entire configured macOS CTest graph behind a process boundary and includes
generic network, script descendant, exact fixture endpoint, unrelated local
endpoint, and production-app counterexamples.
Linux code awaits native replay and Windows remains fail-closed before every
ordinary test command. The designated pull-request identity is now connected
to one default-off, pre-launch-validated canary lane; that lane has not been
executed and still contains only its harmless canary. BPR-0252 now implements
and hermetically proves the production review-comment provider protocol.
BPR-0254 first composed that provider as a dormant exact-target object.
BPR-0255 now retains it behind an asynchronous application host and coordinator,
propagates per-request stop authority, and binds publication to the workbench's
exact right/current-head semantic selection and resolved path. BPR-0256 now
adds the sole signed-capsule prelaunch supervisor, private in-application
attended one-shot issuer, and explicit fresh tests-disabled shipped-app live
registration. Ordinary production launches still have no grant. The lane was
not executed, no credential was acquired, and no mutation was submitted.

RFC-BPR-0008 stays `Proposed`. No card here makes a closure claim. The safe
repository is designated, but the live criteria still require session-scoped
write authority and attended/mixed-client execution. RFC-BPR-0008 stays
`Proposed`.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0007. All three
remain incomplete and `Proposed`; their live cards name the implementation,
composition, target-native, and attended work still open. Done BPR-0088 records
the product owner's direction to advance the implementation frontier despite
that work. Closure order is unchanged.

## Architecture allocation

| Region | Selected responsibility | Planned evidence |
|---|---|---|
| Functional core | Draft and publish semantic state, review command legality, the shared/private distinction, target and head revalidation policy, and acceptance or rejection of mutation results. It emits semantic proposals, never HTTP | Qt-free and provider-free replay tests; the `core-only` preset links no provider |
| Application host | Synchronization, mutation coordination, acting identity, exact selected-diff admission, opaque execution authority, asynchronous effects, per-request cancellation, supersession, idempotency, reconciliation, and shutdown quiescence | Focused host/provider tests plus fixture executable evidence |
| Qt adapter | Editors, exact semantic-selection emission, pending-review presentation, progress, errors, and action binding. A widget never calls a mutation endpoint | Qt adapter tests asserting distinct side/line selections produce intent and no widget holds a provider |
| Composition root | Wiring the asynchronous host into the real application; ordinary launches have no grant and the sole production issuer is an attended one-shot bootstrap | Selected public-path evidence through the real executable against the fixture provider, sealed issuer/source cuts, tests-disabled Release linkage, and static live-manifest proof |

GitHub remains the authority after every mutation: the host re-reads server
state rather than assuming its own request succeeded as sent.

## The safety rule this RFC turns into a test

"No automated test can mutate a non-designated live repository/PR" is an
acceptance criterion, and it is the one criterion that must hold *before* any
mutation code is trusted. It is not satisfied by intending to be careful.

Every slice here runs against a fixture provider. The coordinator refuses a
target that is not the designated one, and that refusal is asserted rather than
assumed — including when the target is altered between prepare and execute,
which is the case a check performed only at prepare time would miss.

No slice in this plan performs a live mutation. The cards that would require
one are listed under deferred gates and stay in `backlog/`.

## Claim matrix

A row is claimed only when its card lands with direct evidence.

| Claim | Direct surface | Command or interaction | What proves it | Result |
|---|---|---|---|---|
| Draft and publish legality is decided in the core | Core | Drive every draft, edit, discard, and publish transition | Illegal transitions carry typed reasons; the suite runs under `core-only` with no provider linked | Verified. BPR-0135 models `publishing` as its own state so a second publish in flight is refused, keeps a failed publication's text, derives `shared` from `published` rather than a settable flag, and refuses six distinct mistakes with six distinct reasons |
| Head movement cannot silently publish | Core | Move the head between draft and publish | Publication is refused with a distinct reason until explicitly revalidated or remapped | Verified. BPR-0136 refuses a moved head until acknowledged, refuses a changed host, repository, or pull request *even when acknowledged*, refuses an unnameable head outright, and reports the changed target ahead of the moved head when both are wrong |
| The wrong target is impossible | Host coordinator | Alter host, repository, pull request, and head between prepare and execute | Each substitution is refused, and refusal is proven for a change made *after* the prepare check | Verified. BPR-0137 observes the target twice; the fake reports the designated target first and another repository second, and the statistics show two observations with zero submissions, so the second check is proven to have run |
| Retries do not duplicate | Host coordinator | Replay a mutation whose result was lost | The second attempt is recognised rather than creating a second comment | Verified. BPR-0137 derives the key from the draft, target, anchor, disposition, and body; the retry returns the original comment identifier and the provider holds one comment, and four cases assert the key changes when body, anchor, or head does |
| External changes round-trip | Host synchronization | Apply an externally created, edited, and deleted comment to the projection | Each is reflected after refresh, and a local assumption never overwrites an unknown external change | Verified against the fake. BPR-0138 round-trips created, edited, deleted, resolved, and reopened; an edit updates rather than duplicating; and an unpublished local edit colliding with a server change keeps the server's text and reports a conflict rather than resolving it. That GitHub produces these shapes is BPR-0143 |
| The mutation path cannot bypass the host | Qt plus trace | Drive every publish and resolve action | Every action produces a core proposal, then a host coordination, then a result event; no widget holds a provider | Verified through the shipped fixture path. BPR-0139's editor is given dispatch callbacks and no provider. BPR-0229 drives real text and Publish actions through core proposal, exact-target coordinator, provider result, server refresh, and typed Qt projection; its source guard rejects direct coordinator/provider calls from the probe |
| Capability fallback is explicit | Host | Offer a provider without native resolve or viewed-state | The degradation is reported in typed form rather than silently skipped | Verified through the fixture product path. BPR-0137 defines the typed capability absence, and BPR-0229 projects the fixture provider's unsupported resolve/viewed capabilities into the shipped editor rather than pretending success |
| Acting identity is carried | Host | Publish under a fixture identity | The proposal and the request carry the active profile's identity | Verified for the request. BPR-0137 asserts the submitted request carries the acting identity and the coordinator's own designated target, so no caller can name a destination. Whether GitHub attributes the review to that account is BPR-0144 |
| A source-line proposal compiles to one exact ordinary GitHub review comment | Application coordinator plus GitHub mutation contract | `ctest --test-dir build/dev --output-on-failure -R '^(application_review_mutation|github_mutation_contract)$'` | The request binds canonical owner/name, repository/PR/base identities, number/open/head, acting database/node/login identity, semantic anchor, path, commit, side, line, body, and idempotency key; any method/path/header/body tamper or invalid UTF-8 is refused | Verified hermetically by BPR-0252. GitHub.com and GHES 3.18 exact REST paths and exact `body`, `commit_id`, `path`, `side`, and one-based `line` JSON bytes pass, including zero-based semantic-line translation and response-range refusal. The corrected selector selected 2 tests (nonzero) and passed 2/2 on 2026-08-10. Incomplete or mismatched live bindings produce zero provider/transport submissions. No network or credential is used. |
| The live provider revalidates immediately, guards retries, and observes host cancellation | Production provider protocol through its separately linked session object | `ctest --test-dir build/dev --output-on-failure -R '^live_review_mutation_provider$'` | A bounded exact-match baseline precedes durable admission; viewer observation then precedes target observation, the complete target/head check is the final network operation before POST, every tuple/actor/anchor mismatch produces no POST, cancellation before or between phases prevents later operations, known success returns one duplicate receipt only for the same canonical write fingerprint, and an indeterminate POST is not resubmitted | Verified hermetically by BPR-0252, BPR-0255, BPR-0256, and BPR-0263. The same host stop token reaches baseline, viewer, target, and native HTTPS operations; cancellation produces typed cancellation and zero later POST. The protocol preserves typed server refusal, binds same-key records to the resolved path, exposes all exact server matches deterministically, and accepts a direct POST receipt only when GitHub reproduces the exact anchor, body, and author identity and its ID was absent from the durable baseline. Indeterminate recovery subtracts the durable baseline and accepts exactly one new identity; C0 alone and multiple new identities remain unresolved. Prepared and baseline-incomplete rows perform zero recovery transport. The concrete provider remains absent from every non-production executable. |
| Authorized asynchronous publication is composed without enabling ordinary production writes | Asynchronous application host, Qt workbench, composition root, and tests-disabled Release application | `ctest --test-dir build/dev --output-on-failure -R '^(application_review_mutation_application_host|qt_diff_workbench_scale_100|application_review_round_trip_composition_contract|application_composition_contract|live_review_mutation_provider)$'` | The exact current right-side semantic selection and epoch-sealed resolved endpoint reach only the asynchronous host; missing grant, left/absent/stale/arbitrary-path selection, supersession, cancellation, and shutdown perform no later work or stale presentation; production links the host/coordinator but no testing grant issuer or direct bypass | Verified portably by BPR-0255 and retained by BPR-0256. Host units cover missing-grant zero effects, exact right/current-head binding, forward/reverse rename and copy provenance, arbitrary-third-path refusal, a host-owned serial drain under a parallel executor, typed cancellation, stale-terminal withholding, and shutdown quiescence. The shipped fixture holds an ambiguous A terminal, clicks B and returns to A through the real right pane, then proves final workbench selection, deferred/adopted draft anchor, pending retirement, and retry identity all remain A after reconciliation. BPR-0264 reconciles the current inventory: fresh tests-disabled Release requires the async-host and coordinator constructors, the generic reconciliation decision used for live post-mutation projection, and the live provider/factory while forbidding the fixture component type and both testing-grant factories. Ordinary production supplies no grant; language-level syntax probes reject external endpoint/grant issuers, a full-source digest locks the composition TU, and exact source-tree cuts reject direct/generic provider or coordinator indirection, pointer-to-member calls, second-TU authority, and a second live factory or bootstrap call. No credential or live lane was used. |
| One attended approval can authorize only one exact shipped-app request | Signed-capsule prelaunch supervisor, production bootstrap, GitHub public-observation adapter, asynchronous host, and explicit live CTest manifest | `ctest --test-dir build/dev --output-on-failure -R '^(application_designated_review_authority_bootstrap|application_review_mutation_application_host|application_review_round_trip_composition_contract|designated_live_macos_capsule_contract|test_network_isolation_(structure_contract|manifest)|architecture_(boundaries|boundary_contract))$'` plus a separate `ctest --show-only=json-v1` opt-in manifest inspection | The capsule inventory and signatures verify first; public observation is unauthenticated, redirect-free, and before GUI creation; the approval is owner-private, exact-schema/digest/build/target bound, retained, durably spent, and atomically consumed; invalid requests do not consume; the first exact host admission seals the fresh actor and consumes; registration names only the bound supervisor/inventory around a fresh tests-disabled Release app | **Reopened by BPR-0256 capability-critical review; recovery repaired by BPR-0263.** The durable baseline, native transmission witness, direct-receipt exclusion, and zero-second-POST recovery counterexample are independently accepted. Two authority blockers remain: replay state still lives below a replaceable approval parent, and the shipped app still accepts direct attended argv without authenticated supervisor/inventory provenance. BPR-0261 and BPR-0262 await exact attended architecture decisions. The checked-in publication packet is invalid and the live lane remains unexecuted. Target-native Linux/Windows and attended GitHub/web evidence remain open. |
| An indeterminate publication is durably recovered without a second POST | Production provider, native HTTPS transport witness, exact-scope SQLite recovery store, asynchronous host terminal, and two-process reopen course | `ctest --test-dir build/dev --output-on-failure -R '^(sqlite_review_mutation_recovery_storage|native_https_transport_(contract|fixture)|live_review_mutation_provider|live_review_mutation_restart_recovery|application_review_round_trip_composition_contract)$'` | Before POST the store atomically admits one prepared request, bounded exact-comment baseline, and SHA-256 binding; only the transport-owned callback after native POST-reply creation may mark it possibly transmitted; restart performs GET-only reconciliation only for that phase, resolving exactly one new identity without another POST | Verified hermetically by BPR-0263. Provider tests prove admission-to-POST death, definite no-send, and failed durable witness remain prepared and adopt no later external C1; an indeterminate C1 resolves, while multiple new IDs remain ambiguous. A direct 201 C0 is refused and a new direct C1 remains authoritative. Native preflight classifies no-send completions, while loopback TLS fixtures prove the real POST invokes one witness after reply creation, aborts on witness failure, and does not advance to a fallback proxy after a retryable first-proxy failure; the existing GET proxy-fallback course remains green. The two-process course proves prepared restart uses zero transport and possibly-transmitted restart resolves with zero POST. Storage tests enforce the baseline on resolve, round-trip the prepared/possibly-transmitted phase, verify the digest before decoding including valid-framing substitution, migrate v1 unresolved rows as baseline-incomplete, retain v1 successes, and enforce schema/capacity/integrity bounds. The corruption binding assumes the owner-private database and is not same-user tamper proof. No credential or live lane was used. |

| No automated test mutates an undesignated target | Whole suite | Structural `live_mutation_isolation` plus the default-deny network/process lane | Every CTest executable, script, helper subprocess, and production-app launch is network-denied unless it belongs to one explicit designated-target lane | **Verified on the configured macOS default graph; target-native Linux/Windows remain open.** Every ordinary configured command carries the exact sandbox-runner prefix, label, isolated Python startup, and pre-boundary environment scrubs; the manifest rejects injection and false-green CTest verdict overrides across every BPR-0235 safety gate, and binds the two architecture-gate names to their exact configured commands. Direct HTTPS-port access, a Python descendant, and the shipped native-HTTPS application course receive process-boundary denial. The runner grants only selected free ports plus a fresh private Unix-socket directory; exact IPv4, IPv6, and Unix fixtures pass, while separately held unrelated loopback and Unix listeners are denied and receive no connections. The structural guard still inspects provider construction and all non-production binaries, with a non-skipping Windows inspector fallback. The default manifest has zero live exceptions. A separate configured manifest proves exactly one default-off exception named `live_review_mutation_designated_target`, with the sole `bpr_live_mutation` label and exact signed-capsule supervisor/inventory/approval command. Configure-time producer checks reject the regression runner, direct GUI, fixture/alternate supervisors or inventories, missing approval, and tests-enabled, non-Release, wrong-source, outside-tree, or stale producer builds before direct `ctest -R` can execute. The original hermetic gate, fork-protocol, and native-race tests remain regression evidence for missing/malformed approval, tuple mismatch, remote failure, substitution, concurrent/repeated use, child mismatch, malformed capability, nonzero exit, timeout, inherited descriptors, and exact order. The prelaunch supervisor matches the immutable `MikeMarcin/BPR-Test#1` tuple before GUI creation; the private in-app bootstrap later consumes the build-bound approval before any grant exists. The live lane was not executed; the production provider exists but was not invoked and no write occurred. The Linux namespace launcher has no target-native record; Windows native approval and whole-suite isolation remain fail-closed. BPR-0235 remains open. |

The BPR-0255 round-3 source-tree cut binds every production app source/header by
sorted relative path and LF-normalized content digest. Hostile spaced-member,
comment-separated member, and fragmented-include variants fail even when their
text evades the former secondary literal scan; the composition TU's full digest
and semantic inventories remain independently enforced.

## Standing composed-host linkage audit

BPR-0248 records the fixture/production boundary in the binary verifier,
BPR-0254 makes its mutation inventory profile-specific, and BPR-0255 records
the shared asynchronous boundary. BPR-0264 records the generic reconciliation
decision used after mutation as shared rather than fixture-only. Both profiles
require the exact review-mutation application-host and coordinator constructors
and `bpr::application::reconcile`. Development fixture additionally requires
the test-only provider boundary and testing-grant factory while forbidding the
live provider and factory. Tests-disabled production requires the exact
live-provider type and factory while forbidding the fixture component type and
testing-grant factory.

Those results prove only what each binary links. The real fixture editor, core
proposal, asynchronous host, coordinator, server refresh, and Qt projection
trace remains the construction and runtime evidence. BPR-0255's independent
construction, grant, selection, and dispatch cuts prove the host is the only
route. BPR-0256 adds a sealed sole production issuer whose digest-bound grant
can reach that route only after attended bootstrap and exact one-use host
admission; ordinary launches still have no authority. Missing binaries,
unsupported tool chains, nonzero tools, failed
demanglers, and malformed or zero-defined symbol tables fail closed. Only
parsed defined records count; `U`/`UND` references and anchors embedded in
surrounding template text do not. Windows declaration prefixes are accepted
only through the first allowlisted calling convention outside balanced
template, parameter, and array delimiters; nested conventions do not select the
anchor boundary and unbalanced declarations fail closed. Absolute, common,
debug, and unknown symbol kinds or sections do not count. CTest treats the
verifier's exit status as authoritative.

## Deferred gates

Three acceptance criteria cannot be satisfied on this host, and each names
exactly what is missing.

| Card | Gate | What is missing |
|---|---|---|
| BPR-0142 | A comment is readable and replyable in the GitHub web UI with no tool-specific marker | Operator approval/materialization of the BPR-0256 packet, a session-scoped write token for the designated repository, attended live-lane execution, and a human to inspect the web UI |
| BPR-0143 | Mixed-client acceptance: external edit, delete, resolve, reopen | The designated repository, session authority, and a second client acting on it |
| BPR-0144 | Reviews appear under the authenticated user's identity with ordinary semantics | A real account and token; the fixture proves the identity is carried, not that GitHub honours it |

The product owner designated `MikeMarcin/BPR-Test` before any run. Session
authority and the required human/mixed-client actions remain unexercised. The
repository is bound to GitHub database ID `1328277631` and node ID
`R_kgDOTyvofw`. On 2026-08-10 the product owner also authorized and designated
disposable PR `#1`, database ID `4246446933`, node ID
`PR_kwDOTyvof879G6NV`, base-repository ID `1328277631`, open state, and head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`, before any BetterPullReview live
child or review mutation ran. BPR-0235's macOS default-deny boundary does not
authorize provider composition or a write. Its default-off exact-target canary
exception does not substitute for deferred Linux/Windows evidence or attended
live execution.

## Card allocation

| Card | Slice |
|---|---|
| BPR-0134 | This planning slice |
| BPR-0135 | Core draft, edit, discard, and publish state with typed legality |
| BPR-0136 | Core target and head revalidation, and the stale-head refusal |
| BPR-0137 | Host mutation coordinator: safe-target enforcement, idempotency, retries |
| BPR-0138 | Host synchronization, refresh cursors, and external-change reconciliation |
| BPR-0139 | Qt draft editors, publish confirmation, and the no-bypass proof |
| BPR-0140 | Documentation: mapping, drafts, publish, identity, stale head, retries, capability fallback, safe-test policy, mixed-client contract |
| BPR-0141 | Simplification and closeout preparation |
| BPR-0229 | Shipped editor-to-core-to-fixture-coordinator-to-server-refresh composition and non-bypass executable trace |
| BPR-0252 | Uncomposed production review-comment provider, exact mutation wire contract, native HTTPS mutation operation, immediate target/viewer revalidation, and guarded idempotency |
| BPR-0254 | Dormant exact-target production composition, renewable opaque accepted-snapshot lease, fixture marker, and profile-specific binary verification |
| BPR-0255 | Authorized asynchronous publication host, exact right/current-head selection and path binding, propagated stop authority, fixture-only execution grant, and production no-grant/direct-bypass cuts |
| BPR-0256 | Signed-capsule prelaunch supervisor, private digest-bound attended bootstrap, unauthenticated public target observation before GUI creation, durable indeterminate-write recovery, sealed one-use async-host grant, tests-disabled shipped-app live registration, and pending operator packet |
| BPR-0263 | Durable pre-attempt exact-comment baseline and SHA-256 binding, transport-owned possibly-transmitted witness, all-match provider observation, baseline-delta indeterminate recovery, schema-v1 guarded migration, and two-process GET-only/zero-transport phase proof |

## Documentation ledger

| Scope | Required documents | Disposition |
|---|---|---|
| Review, comment, and thread mapping; drafts; publish semantics; identity; stale-head behaviour; retries; capability fallback; safe-test policy; mixed-client contract | [Review Round Trip](../architecture/review-round-trip.md) | Written. Records ownership, the command/decision/proposal/request/completion/observation/delta vocabulary, the draft lifecycle, semantic anchoring, the two unequal target mismatches, safe-target enforcement, identity, idempotency, synchronization and the mixed-client contract, capability fallback, and what is not proven without a live target |
| Production composition, exact target/actor/selection identity, authority revocation, durable recovery, and asynchronous lifecycle | [System Architecture](../architecture/system-architecture.md), [Review Round Trip](../architecture/review-round-trip.md), and root README | Updated through BPR-0256. Compiled constants bind only the designated repository/PR/base/head tuple; acting database/node/login identity is dynamic, sealed from the first fresh authenticated binding, cross-bound to the accepted lease/snapshot, and re-observed. The application host validates the exact right/current-head semantic selection and resolved path, owns per-request stop/supersession/shutdown, and withholds stale terminals. Ordinary production has no grant. The signed-capsule supervisor binds the prelaunch app/dependency identity and public target observation; the private issuer binds an owner-private approval to exact build/source/target evidence and authorizes one exact host admission. Unresolved writes persist in an exact-scope SQLite store and reconcile from server truth without a second POST. Credentials remain separate. |
| Private review ledger boundary | [Review Ledger](../architecture/review-ledger.md) | No impact. The ledger holds private progress and this RFC holds shared state; the two share no table and no authority. Recorded rather than assumed |

The document names the safe mutation target and states that no other target is
reachable. It also records that weakening the coordinator's target check to let
a test through would remove the property the test exists to exercise. BPR-0235
adds the process-boundary isolation the current structural inventory lacks.

## Closeout blockers

RFC-BPR-0008 cannot become `Auto-Verified` until RFC-BPR-0007, RFC-BPR-0005,
RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002 are, and therefore not before real
Windows x64 evidence in BPR-0036 and BPR-0053. It additionally cannot close
before BPR-0142, BPR-0143, BPR-0144, and BPR-0235. The repository is
designated; session authority, human/mixed-client execution, and durable
default-deny isolation remain open. BPR-0141 will make no closure claim.
