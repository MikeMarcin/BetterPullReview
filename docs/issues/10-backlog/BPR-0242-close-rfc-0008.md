---
id: BPR-0242
title: Verify and close RFC-BPR-0008
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0142, BPR-0143, BPR-0144, BPR-0229, BPR-0234, BPR-0235, BPR-0241]
---

# Verify and close RFC-BPR-0008

RFC base: `7ea6c9ab7b0878706ca3df83390ad3a2e5b7e91a`.

## Closeout checklist

Historical evidence is provenance only; every row requires final replay at the final closeout head.

| State | Criterion | Direct verification surface | Command or attended interaction | Expected result | Durable evidence location | Gate class | Live blocker |
|---|---|---|---|---|---|---|---|
| [ ] | A BetterPullReview comment is readable and replyable in the GitHub web UI with no tool-specific marker. | Designated safe-target GitHub web UI thread created by the composed BPR-0229 mutation path | Perform [BPR-0142](BPR-0142-live-comment-web-ui-verification.md#required-direct-evidence)'s safe-target publish, then read and reply to the ordinary comment in GitHub's web UI with no BetterPullReview-only marker. | The comment and reply are ordinary GitHub prose/threads usable from the web UI and match the composed application's refreshed projection. | BPR-0142 live-comment verdict summarized in the deferred-gates ledger section in [RFC-BPR-0008 verification ledger](../../verification/RFC-BPR-0008.md#claim-matrix); final replay record keyed by “A BetterPullReview comment is readable and replyable in the GitHub web UI with no tool-specific marker.” | external-authority | [BPR-0142](BPR-0142-live-comment-web-ui-verification.md) remains open and owns the unreplayed surface. |
| [ ] | A comment created or edited externally appears correctly after refresh. | Mixed-client refresh against the designated safe GitHub target | Perform [BPR-0143](BPR-0143-mixed-client-acceptance.md#required-direct-evidence)'s external create/edit interaction, refresh BetterPullReview, and compare exact server comment identity/version/text. | External creation appears once and an external edit updates the existing projection without duplication or local overwrite. | BPR-0143 mixed-client evidence summarized in the deferred-gates ledger section in [RFC-BPR-0008 verification ledger](../../verification/RFC-BPR-0008.md#claim-matrix); final replay record keyed by “A comment created or edited externally appears correctly after refresh.” | external-authority | [BPR-0143](BPR-0143-mixed-client-acceptance.md) remains open and owns the unreplayed surface. |
| [ ] | Resolve/unresolve and viewed/unviewed state use native GitHub capabilities when available and degrade explicitly when not. | Native resolve/unresolve and viewed/unviewed capability interaction on BPR-0143's designated target | Run [BPR-0143](BPR-0143-mixed-client-acceptance.md#required-direct-evidence)'s supported and unsupported capability cases from both GitHub and BetterPullReview, then refresh. | Supported operations round-trip through native GitHub state; unsupported operations show a typed degradation and never pretend success. | Capability-fallback row plus BPR-0143 server-truth evidence in [RFC-BPR-0008 verification ledger](../../verification/RFC-BPR-0008.md#claim-matrix); final replay record keyed by “Resolve/unresolve and viewed/unviewed state use native GitHub capabilities when available and degrade explicitly when not.” | external-authority | [BPR-0143](BPR-0143-mixed-client-acceptance.md) remains open and owns the unreplayed surface. |
| [ ] | Reviews appear under the authenticated user's identity with ordinary GitHub semantics. | Authenticated designated-account review submission and GitHub web attribution | Run [BPR-0144](BPR-0144-acting-identity-on-a-real-account.md#required-direct-evidence)'s safe-target review submission and inspect the acting account in GitHub's authoritative review record. | The server attributes the review to the selected authenticated account and ordinary GitHub review semantics remain intact. | Acting-identity row and BPR-0144 authoritative account record in [RFC-BPR-0008 verification ledger](../../verification/RFC-BPR-0008.md#claim-matrix); final replay record keyed by “Reviews appear under the authenticated user's identity with ordinary GitHub semantics.” | external-authority | [BPR-0144](BPR-0144-acting-identity-on-a-real-account.md) remains open and owns the unreplayed surface. |
| [ ] | Mutation retries are idempotent or guarded against duplication. | Idempotency-key mutation coordinator and lost-result retry fixture | Run `ctest --preset dev -R '^(application_review_mutation)$' --output-on-failure`, then execute BPR-0229's public fixture trace from editor gesture through core proposal, coordinator, provider result, refresh, and Qt projection. | Replaying the same lost-result request returns the original comment ID and leaves exactly one provider comment; changing body, anchor, head, or disposition changes the key. | Retries-do-not-duplicate row in the claim matrix in [RFC-BPR-0008 verification ledger](../../verification/RFC-BPR-0008.md#claim-matrix); final replay record keyed by “Mutation retries are idempotent or guarded against duplication.” | automated | No live blocker; final-head focused replay remains required. |
| [ ] | No automated test can mutate a non-designated live repository/PR. | Process-boundary default-deny isolation with structural positive controls | Extend and run `ctest --preset dev -R '^(live_mutation_isolation\|live_mutation_isolation_contract)$' --output-on-failure` with BPR-0235's direct-HTTPS, Python/subprocess, helper, and production-app positive counterexamples plus the single identity-validated opt-in lane. | Direct HTTPS, script subprocess, helper, and production-app network attempts fail outside the single named designated-target lane; the allowed lane validates repository/PR before launch. | No-automated-test-mutates-an-undesignated-target row plus BPR-0235 required contract in [RFC-BPR-0008 verification ledger](../../verification/RFC-BPR-0008.md#claim-matrix); final replay record keyed by “No automated test can mutate a non-designated live repository/PR.” | automated | [BPR-0235](../40-parked/BPR-0235-enforce-default-deny-live-mutation-tests.md) remains open and owns the unreplayed surface. |
| [ ] | Uninstall, mixed-client, database-loss, and server-truth acceptance tests pass. | BPR-0143 uninstall, external-client, database-loss, and server-authority sequence | Run [BPR-0143](BPR-0143-mixed-client-acceptance.md#required-direct-evidence)'s designated-target sequence: publish, edit/reply externally, remove local state, reinstall/reopen, and refresh from GitHub. | GitHub retains authoritative comments/thread state across uninstall and database loss, and BetterPullReview reconstructs the exact current server truth. | External-changes-round-trip row plus BPR-0143 acceptance record in [RFC-BPR-0008 verification ledger](../../verification/RFC-BPR-0008.md#claim-matrix); final replay record keyed by “Uninstall, mixed-client, database-loss, and server-truth acceptance tests pass.” | external-authority | [BPR-0143](BPR-0143-mixed-client-acceptance.md) remains open and owns the unreplayed surface. |
| [ ] | Head movement between draft and publish cannot silently submit against the wrong revision. | Core moved-head legality and host prepare/execute target revalidation | Run `ctest --preset dev -R '^(core_review_round_trip\|application_review_mutation)$' --output-on-failure`, then execute BPR-0229's public fixture trace from editor gesture through core proposal, coordinator, provider result, refresh, and Qt projection. | Changing head after draft or after prepare refuses submission with a distinct stale-head reason until explicit revalidation; zero provider submissions occur. | Head-movement and wrong-target rows in the claim matrix in [RFC-BPR-0008 verification ledger](../../verification/RFC-BPR-0008.md#claim-matrix); final replay record keyed by “Head movement between draft and publish cannot silently submit against the wrong revision.” | automated | No live blocker; final-head focused replay remains required. |
| [ ] | Mutation legality and stale-head policy are core-tested without HTTP/Qt; host safe-target/idempotency tests and Qt draft/action tests prove no provider bypass. | Core legality, host coordinator/synchronizer, Qt editor, and BPR-0229 public trace | Run `ctest --preset dev -R '^(core_review_round_trip\|application_review_mutation\|application_review_synchronization\|qt_review_draft_editor)$' --output-on-failure`, then execute BPR-0229's public fixture trace from editor gesture through core proposal, coordinator, provider result, refresh, and Qt projection. | Every gesture produces a core proposal, host coordination, typed result/refresh, and Qt projection; no widget/provider bypass exists and stale targets remain refused. | Mutation-path-cannot-bypass-host row plus BPR-0229 composition contract in [RFC-BPR-0008 verification ledger](../../verification/RFC-BPR-0008.md#claim-matrix); final replay record keyed by “Mutation legality and stale-head policy are core-tested without HTTP/Qt; host safe-target/idempotency tests and Qt draft/action tests prove no provider bypass.” | automated | No live blocker; final-head focused replay remains required. |

## Current continuation — 2026-09-05

RFC base remains `7ea6c9ab7b0878706ca3df83390ad3a2e5b7e91a`. BPR-0606 repaired
the missing epoch-preparation diagnostics after v009 sign-in and PR open
succeeded but history preparation refused. Next: BPR-0607 owns the prepared read-only diagnostic checkpoint, which
records the first causal history failure or exact comparison success before
BPR-0256 prepares a new publication candidate. Its v009 approval is consumed. The live
rows above and the whole requested RFC goal remain incomplete.

## v008 preparation complete; attended decision pending — 2026-09-05

[[BPR-0256]] is parked as `awaiting-owner-verdict` with its
[v008 packet](../../verification/reviews/designated-live-review-publication-v008/operator-packet.md)
active after clean exact critical review. Packet SHA-256:
`2bc3e8699cf3674b25e1c5b0e933e3e59605d0abecc130dfafde99cfec77cdfd`.

Closeout review runtime: native collaboration (gpt-6-astra, xhigh).

Preparation at production source `a63e92e6`, unchanged at `eff28443`: offline
Release/tests-disabled capsule build and mounted preflight passed; six selected
fixture/authority/isolation checks passed in 99.19 seconds; configured isolation
passed for 281 tests; default/static enumeration proved zero/one live lanes.
Documentation structure and 62 self-tests passed. Independent review found no
blocking or non-blocking findings and matched all eight artifacts, 37 selected
source entries, 95 inventory entries, command properties and unspent replay ID.
No full matrix or native Windows/Linux check was added. These are preparation
results, not replacement evidence for the unchecked live acceptance rows above.

The temporary static graph, private invalid placeholder/parent and eleven
inspection/helper files are removed. Retain only the exact final producer/image
and empty mount parent for the pending owner decision. No app process or image
attachment remains; only the original main worktree exists. The task's restored
persistent goal stays active across this checkpoint; the issue board remains
authoritative. No portable/Mac ready card remains. Next: receive the exact v008
response, then revalidate and run its single bounded attended course; BPR-0142's
reply-return, BPR-0143 and BPR-0144 require separate authority and evidence.

## v008 execution disposition — 2026-09-05

The [v008 result](../../verification/reviews/designated-live-review-publication-v008/attended-result.md) records an agent UI-targeting failure: name-based selection launched/controlled an ordinary build alongside the approved mounted app. Wrong-build device-flow interaction and a device-panel capture violated the procedure; no PR or publication occurred. Both instances were explicitly signed out and closed; CTest returned 0 in 892.24 seconds and the image detached. No live acceptance row is discharged. The exact approval is consumed. The static graph, empty approval/mount parents and three temporary run files are removed; the unchanged final producer/image is retained for immediate v009 subject/packet preparation with full-path and PID binding. BPR-0256 remains the next slice; no new product-source repair or repeated fixture matrix is warranted by this execution mistake.

## v009 preparation and direct authorization — 2026-09-05

BPR-0256 prepares the [v009 packet](../../verification/reviews/designated-live-review-publication-v009/operator-packet.md) from the unchanged retained capsule. The owner directly authorized the same bounded successor and asked not to repeat the verbatim approval ceremony. Record that actual instruction and bounded interpretation; do not invent a digest quotation or reuse spent v008 authority. Exact review, static inspection and full mounted-path/PID UI binding precede the fresh one-use run. All earlier live-acceptance rows remain incomplete.

## v009 consumed; selected-file refusal — 2026-09-05

The [v009 result](../../verification/reviews/designated-live-review-publication-v009/attended-result.md) records the single approved mounted run: sign-in as MikeMarcin and all nine capability checks succeeded, and the correct designated PR opened. Selecting its sole file failed before source loading with a changed-file refusal and unavailable exact comparison history. No body was entered or Publish action taken. Explicit Sign out, quit and detach completed; CTest exit 0 in 321.64 seconds proves cleanup only. Device-field emissions are disclosed in the result; clean capture compliance is not claimed. The new approval is spent. Next: diagnose and repair the file/history path with fixtures before preparing another candidate. All live publication acceptance rows remain incomplete.

Current next slice: [[BPR-0606]] adds the missing epoch preparation diagnostics and investigates the upstream refusal. BPR-0256 returns to backlog behind it. Existing v009 authority is consumed; no successor or changed binary is launched in this repair slice.

## Epoch diagnostics available — 2026-09-05

[[BPR-0606]] is Done: five focused host/logging/composition checks passed in
49.33 seconds; integrator reproduction passed three checks in 5.94 seconds.
Dev/Release, six changed-TU tidy and scoped format pass. The live root cause is
still unknown. BPR-0256 is the next candidate/attended slice; no RFC closeout
or successful publication is claimed. All consumed v009 producer/image,
static/mount/approval and temporary inspection resources were removed.

Current frontier: [[BPR-0607]] is parked with its concrete
[epoch diagnostic packet](../../verification/reviews/epoch-preparation-diagnostic-v001/operator-packet.md).
It accepts a plain instruction to run, as requested by the owner, and does not
require another pasted digest block. No returned instruction, live result or
publication is fabricated. The persistent goal remains active for the complete
RFC set; all currently available unattended work in this slice is committed.

## History protocol repair — 2026-09-05

[[BPR-0607]] observed healthy MikeMarcin sign-in followed by protocol discovery
`unsupported_version`. The owner requested preserving remembered sign-in and
autonomous diagnostic iterations; the app quit without Sign out. No publication
occurred. Current next slice: [[BPR-0608]] repairs the observed smart HTTP
service preamble mismatch, verifies fixtures and reopens the designated PR.
No live acceptance row or RFC closeout is claimed by this diagnostic result.

BPR-0608 completed the HTTP framing repair at `092dddd8`: four focused checks
passed in 49.56 seconds and live remembered-sign-in, exact history and rendered
source diff succeeded. Current next slice is BPR-0256's fresh publication capsule
under standing owner authority, preserving sign-in on quit. Publication itself
remains unobserved; the live acceptance rows remain incomplete.
