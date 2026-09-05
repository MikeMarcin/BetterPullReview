---
schema: "implement-rfc.card.v1"
id: "BPR-0541"
title: "Persist the device-grant binding across a restart and defer the first refresh"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0540"]
---

# Persist the device-grant binding across a restart and defer the first refresh

## Why

The second independent review of RFC-BPR-0016 ([[BPR-0285]], 2026-09-04)
found that the "restart resume" [[BPR-0536]] composed is unreachable: the
credential binding catalog is process-local, `resume_profile` has no caller
in the composed application, and an activation without a binding fails with
`vault`, so no session survives a restart and nothing re-arms renewal after
one. It also found that `resume_refresh` begins a refresh at once, so a
transient transport failure on resume becomes `reenrollment_required` while
the access token still works.

## Required final state

- A device-grant binding persists across a restart in the catalog the vault
  already anchors, so a launch with a stored grant resumes the session and,
  through [[BPR-0540]]'s provenance rule, arms renewal without a new code.
- The first refresh after a resume is scheduled by the stored lifetime, not
  begun at once; a transient failure retries within the lead before it
  reports re-enrollment.
- A composed course restarts the application over the fixture with a
  stored grant and proves the resumed session, `REFRESH_RESUMES:1`, and one
  rotation; the documents and the ledger row describe the persisted binding.

## Non-goals

- Persisting a pasted-token session.

## Owner-policy checkpoint — 2026-09-04

Claimed for the remaining-issues run. The RFC-BPR-0016 closeout explicitly
classifies this as a product-rule change for the owner: composition currently
selects `delete_retained_on_shutdown`, and BPR-0540 requires both access and
refresh slots to share that erasure path. Persistent bindings alone cannot
make a session resume when its tokens were intentionally deleted. A concrete
attended policy decision is prepared before changing that credential lifecycle.

## Decision prepared — 2026-09-04

The [offline policy review](../../verification/reviews/device-session-retention-policy-v001/offline-package/index.html)
binds two alternatives: remember device grants with a visible Sign out action
and exact restart validation, or retain session-only sign-in. Its exact
[policy proposal](../../verification/reviews/device-session-retention-policy-v001/policy.md)
names the affected shutdown, manual-token replacement, expiry, vault, and
restart-course behavior. The batch is
`bpr:/attended-review/betterpullreview/2ae811784b1f4aba21e7e09e`.

At this preparation checkpoint no verdict existed. No implementation,
credential access, or network operation had been performed for this card. Once the owner exports a complete verdict, validate
it against `offline-package/attended-review-manifest.json` using the attended
review builder, record the response immutably, and apply only the chosen policy.
The card parks on this product decision; all its dependencies are terminal.

## Owner decision validated — 2026-09-04

The owner selected **Remember device sign-in** and left the session-only
alternative not chosen. The complete exported verdict validates against batch
`bpr:/attended-review/betterpullreview/2ae811784b1f4aba21e7e09e`; the
[immutable validation record](../../verification/reviews/device-session-retention-policy-v001/verdict-validation.json)
binds the manifest, received response, decoded JSON, and official validator.
Only HTML space entities and Markdown underscore escapes introduced in message
transport were decoded. No decision value changed.

The chosen policy is now authorized for implementation, including a visible
Sign out action and manual-token replacement erasure. Credential operations,
live mutations, implementation completion, and RFC closeout remain unclaimed.
The product-decision blocker is removed; this card returns to the dependency-
ready backlog while the integrator finishes the claimed BPR-0316 slice.

## Implementation claim and design — 2026-09-04

The validated owner decision is applied after BPR-0558 and BPR-0308 at
`0b522fb1`. Device grants gain durable exact binding/provenance/lifetime metadata
in a dedicated SQLite store; token bytes remain solely in the vault. Reuse
the lazy queue-store pattern and perform load/write/delete on the effect lane,
with typed serialized completions, cancellation and quiescence. Do not add
filesystem or SQLite work to core, Qt, or the host decision lane.

Startup restores only the compiled native GitHub.com device profile after
both GitHub and device hosts exist, then uses `resume_profile` and the existing
exact `/user` account validation. Missing/corrupt/mismatched/expired stored
material fails visibly. Stored issuance/expiry facts bridge process lifetimes;
active deadlines remain monotonic, and backward or expired wall-clock facts
cannot extend a token. First refresh is deferred from the remaining lifetime;
only transient failures retry, with a bounded delay inside access validity.

A distinct production shutdown selection retains only accepted device grants.
Manual enrollment remains session-only, replacing a device grant exact-deletes
the prior pair/binding, and a visible Sign out dispatches core deactivation
through the existing ordered access/refresh erasure path. Store/vault failures
leave a non-resumable diagnostic/tombstone rather than a successful false grant.

Verification will separate durable metadata reopening from fixture vault
continuity: the composed course must destroy and reconstruct application hosts
and reopen the store, with runtime-only fake-vault material kept outside disk.
A separate-process metadata test proves no process-local catalog supplies the
restart selector. Native-vault cross-process evidence is reported separately;
no ordinary operator credential or live service is used by this implementation
slice. A capability-critical review will assess persistence, expiry and erasure
boundaries after the focused tests pass.

## Implementation candidate and review — 2026-09-04

The initial candidate adds a lazy, private metadata store and serialized effect
wrapper; exact startup validation, deferred renewal, device-only retention and
the ordinary Sign out control. Manual replacement remains session-only. The
worker's six focused tests passed in 25.61 seconds; the integrator reproduced
`application_github_host`, `application_device_authorization_refresh`,
`sqlite_device_binding_storage`, and `application_device_binding_restart`:
4/4 in 15.83 seconds. Dev and tests-disabled Release builds, scoped strict tidy,
formatting, boundary/inventory checks and fixture exclusion passed. The
integrator inspected the actual rendered session header with visible Sign out.

These are candidate results, not completion. Independent review identified
three paths the initial tests did not cover: a retired renewal deactivating a
replacement public-preview identity; Sign out racing the initial metadata load;
and a renewal during a second device flow losing the previous-binding marker
before the new flow authenticates its account. Focused reproductions and repairs
are required before this card can close. No live service or operator credential
has been used.

Closeout review runtime: codex (gpt-6-astra, xhigh). The Codex driver has no
Claude-lane mechanism in `implement-rfc/references/codex-lanes.md`; native review
is used. The final narrow repair recheck is clean, as recorded below.

### Independent findings reproduced

The integrator ran
`ctest --test-dir build/dev --output-on-failure -R '^(application_github_host|application_device_authorization_refresh|application_device_binding_restart)$'`
against unchanged candidate production source and the three new regression
assertions. All three failed as predicted (exit 8, 26.98 seconds): pending-load
Sign out did not tombstone the binding; the concurrent second grant wrote under
the preceding authority/claimed settled refresh too early; and the retired
renewal deactivated ordinary Public preview after its deadline. The comparison
case for ordinary shutdown during startup loading still passes. These findings
are accepted as blockers within this slice; the corresponding repairs and
narrow recheck remain owed.

### Repairs and final focused verification

All three reproduced defects are repaired. Renewal failure carries the exact
credential authority and identity generation through serialized ingress, and
replacement modes release the retired renewal. A same-chain generation rebind
preserves whether its binding preceded a pending second device flow. Startup
load retains the core-issued invalidation request, validates the loaded record
against the compiled native profile, and tombstones before ordered access and
refresh deletion. Replacement session effects wait for the exact refresh-delete
terminal. Ordinary shutdown while loading retains the grant; explicit Sign out
followed immediately by shutdown completes its requested cleanup.

The integrator reran the same three-test selector against the final repair:
**3/3 passed, exit 0, 28.09 seconds**. The held refresh-delete regression, early
manual/preview replacement, compiled-profile mismatch, current-versus-stale
renewal failure, and reentrant invalidation are included. The worker's final
host/restart recheck passed 2/2 in 26.84 seconds; its preceding focused set
passed 9/9, including the two-rotation, orphan, and ordinary public-preview
courses. Core-only `core_github_policy`, `architecture_boundaries`,
`architecture_boundary_contract`, `core_dependency_boundary`, and
`core_boundary_contract` passed 5/5. Incremental dev and tests-disabled Release
builds, fixture exclusion, full application and twelve-source composition pins,
scoped strict tidy over the eight repaired translation units (with final host
and test reruns), formatting, and diff hygiene passed. The prior SQLite storage
and separate-process metadata results remain valid because their inputs did
not change. Code simplification found no further behavior-preserving change
needed in this bounded slice.

The README, product/system/host architecture, threat/foundation security,
workbench UX, operator build/test guide, and RFC-0016 ledger now describe the
approved policy. Documentation structure validation and all 54 validator tests
passed before this final result reconciliation. Native vault continuity across
OS processes and native Windows/Linux execution were not run; the composed
course uses runtime-only fake-vault continuity and reopens real SQLite storage.
No token bytes were persisted by tests, and no operator credential or live
service was accessed. The same critical reviewer is rechecking only the three
repairs and the startup replacement barrier before terminal status.

### Follow-up counterexamples resolved and closeout

The narrow review found two additional timing variants within the same
criteria: retrying manual enrollment after a failed startup load could overwrite
an unread old binding, and startup restoration completing during an already-live
device flow could give the new grant the old account's refresh slot. The
integrator reproduced both failures against unchanged production source:
2/2 failed, exit 8, 0.12 seconds. The repairs latch unresolved startup metadata
and visibly refuse further session establishment for that application lifetime
(retry requires a fresh launch), and mark a restored grant as preceding an
already-live device flow. Both focused regressions now pass.

Final integrator replay of the three-test host/refresh/composed-restart selector
passed **3/3, exit 0, 26.86 seconds**. The worker's replay passed 3/3 in
28.39 seconds. Incremental dev and tests-disabled Release builds, strict tidy
on the final four affected translation units, format and diff checks passed.
The same critical reviewer resolved both follow-ups and reported no remaining
blocker on the repair surfaces. Earlier unchanged passing lanes remain valid.

| Acceptance | Direct evidence | Result | Source commit |
|---|---|---|---|
| Exact durable device binding and session-only manual enrollment | `application_github_host`, `sqlite_device_binding_storage`, separate-process metadata reopen | Pass, including tombstone ordering, startup failures and manual replacement | This BPR-0541 completion commit |
| Deferred first renewal and bounded transient retry | `application_device_authorization_refresh` | Pass, including old/new grant identity timing | This BPR-0541 completion commit |
| Composed restart, resume, one rotation and visible Sign out | `application_device_binding_restart` and inspected real Qt header | Pass across multiple application lifetimes; Sign out prevents next resume | This BPR-0541 completion commit |
| Approved policy and current documentation | Validated owner verdict; documentation structure gate and 54 validator tests | Pass; native-vault OS-process and Windows/Linux execution remain unclaimed | This BPR-0541 completion commit |

No implementation acceptance criterion remains open. The earlier RFC closeout
keeps its original platform evidence; this separately approved policy change
is complete with the explicit evidence limits above.
