---
schema: "implement-rfc.card.v1"
id: "BPR-0947"
title: "Compose updater download and signed staging"
status: "Done"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0946"]
milestone: "RFC-0035 milestone 2"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
acceptance: [{"criterion":"AC-01","outcome":"pass","revision":"54535c1e3638af99c5e3560ce3033f4cf1ff5a06","evidence":{"kind":"observation","detail":"Actual profile_desktop application menu and General Settings composition plus the same updater service with fixture capabilities pass manual/daily discovery, Later, Skip, SQLite preference/account restart, literal notes, incremental projection and orderly shutdown.","reference":"docs/verification/RFC-BPR-0035.md"}},{"criterion":"AC-02","outcome":"pass","revision":"54535c1e3638af99c5e3560ce3033f4cf1ff5a06","evidence":{"kind":"observation","detail":"Release-only native GitHub.com authority/factory and revoked/GHES refusal fixtures pass; 24 native loopback TLS scenarios prove bounded metadata/assets, only allowed CDN redirects, first-hop-only credential canaries, cancellation/timeouts, TLS verification and inaccessible-not-current typed failures. No actual credential or live private service is claimed.","reference":"docs/verification/RFC-BPR-0035.md"}},{"criterion":"AC-03","outcome":"pass","revision":"54535c1e3638af99c5e3560ce3033f4cf1ff5a06","evidence":{"kind":"observation","detail":"Exact streamed bounds, 17 native archive refusals and private lock/cleanup pass. The genuine 0.1.3 universal ZIP (SHA256 72b71ebd642f83f86ff7ec889bd4e201a0586af40d32d9be9a15201e8d91a4a9) passes installed eligibility, bundle/version/Mach-O deployment checks, deep strict all-architecture designated-requirement/team and system trust validation, exact revalidation and changed-tree refusal on arm64 with network denied.","reference":"docs/verification/RFC-BPR-0035.md"}},{"criterion":"AC-04","outcome":"pass","revision":"54535c1e3638af99c5e3560ce3033f4cf1ff5a06","evidence":{"kind":"observation","detail":"Actual desktop composition, managed/unsupported/untrusted/read-only/translocated/mounted-image manual fallbacks and no-credential controls pass; Install remains disabled after staging. Six focused CTest rows, clean exact-candidate architecture/core scans, 138 architecture and four core contract tests, task-owned lint and documentation gates pass with existing environmental/full-header lint limitations retained. Helper, replacement, recovery, Intel and packaged upgrade remain later milestones.","reference":"docs/verification/RFC-BPR-0035.md"}}]
---

# Compose updater download and signed staging

Second authored milestone of [[BPR-0911]]. Compose one application-owned updater
with real menu and Settings controls, persisted preferences and separately selected
native GitHub.com release-access authority. Implement fixed private release and
asset transport, pinned redirect origins/API version, streamed bounds and native
signed macOS ZIP staging. Preserve existing PR redirect prohibitions.

## Acceptance

- AC-01: Real menu/Settings gestures traverse host/core and incremental Qt status;
  automatic/manual checks, Later, Skip and restart preferences behave as specified.
- AC-02: Private fixture access, revocation and cross-origin secret-canaries prove
  the release-only authority and redirect contract; inaccessible is never current.
- AC-03: Stream/archive/native validation rejects wrong bounds, bundle, version,
  architecture, digest, signer, unsafe entries and changed staged candidates.
- AC-04: Demonstrate fixture menu-to-host-to-projection composition and truthful
  unsupported/managed/manual fallback, without enabling replacement prematurely.

Record RFC AC-01 through AC-04 and AC-08 evidence and native limits on [[BPR-0912]].

## Verification and execution record

Mandate `r1`, run `rfc0035-20260920`, segment `m2-implementation`, child
`/root/rfc0035_m2`. Accepted source candidate:
`54535c1e3638af99c5e3560ce3033f4cf1ff5a06`. Claim `a8578fcc`; source commits
`5ed24a8c`, `1f229029`, `54535c1e`. The authored plan and acceptance are unchanged.

- AC-01: Actual desktop application menu/General Settings composition and the
  same service with fixture ports pass manual/daily checks, Later, Skip, SQLite
  preferences/account restart and incremental projection.
- AC-02: Release-only native authority/factory and revocation fixtures plus 24
  loopback TLS cases pass. Credential canaries appear only on the initial API
  hop; prohibited redirects and inaccessible releases refuse truthful typed status.
- AC-03: Actual streaming bounds, 17 archive refusals, genuine signed/notarized
  universal 0.1.3 source staging, exact revalidation and changed-tree rejection
  pass on macOS arm64 under the network-denied harness.
- AC-04: Real root/public-path fixture composition, six manual-fallback
  capabilities, bounded native mount inventory and no-credential negative
  controls pass. Install and Restart remains disabled even for verified staging.

Focused CTest 6/6, exact-candidate architecture/core scans, 138 architecture and
4 core-boundary contract tests, canonical formatting, task-owned tidy, the
documentation gate and 64 documentation tests pass. The preexisting full-header
tidy finding and unrelated nested-worktree scanner interference are retained,
not relabelled as passing evidence. [Verification ledger](../../verification/RFC-BPR-0035.md)
contains commands, artifact digest, corrected failures, self-review and limits.

No live private-service/native-vault credential operation, publication, running-app
replacement, mounted-image installation, Intel execution or packaged upgrade was
performed. No human or independent acceptance is required by this milestone.
All task commands are joined; temporary exports/copies are recoverably in Trash.
No review server or protected candidate exists. Next is [[BPR-0948]] with a fresh
implementer; no installation or full-RFC acceptance is claimed here.
