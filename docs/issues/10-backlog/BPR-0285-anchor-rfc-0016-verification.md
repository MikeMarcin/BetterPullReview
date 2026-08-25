---
id: BPR-0285
title: Anchor RFC-BPR-0016 capability-critical verification
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0035, BPR-0275, BPR-0276, BPR-0277, BPR-0278, BPR-0279, BPR-0280, BPR-0281, BPR-0282, BPR-0283, BPR-0284, BPR-0310, BPR-0311, BPR-0318]
---

# Anchor RFC-BPR-0016 capability-critical verification

RFC base: `b969554fb45d838b2a4ea11811363bd7e158e47e`.

## Run objective and membership

This is the terminal anchor for the first RFC in the requested sequence:
RFC-BPR-0016, then RFC-BPR-0017, then remaining unattended macOS-ready work.
Its planned membership is exactly BPR-0274 through BPR-0285, target-native
platform cards BPR-0310/BPR-0311, distributable-identity card BPR-0318, plus
pre-existing RFC-BPR-0002 closeout BPR-0035/BPR-0036. Any newly discovered
required work is added explicitly to this dependency closure before closeout.

Historical runs are provenance only. Every row requires final replay at the
immutable closeout head and an independent capability-critical review.

| State | Acceptance criterion | Direct verification surface | Expected result | Gate |
|---|---|---|---|---|
| [ ] | GitHub.com device enrollment reaches the existing vault with no settings/SQLite copy. | Tests-disabled public composition plus vault/settings/SQLite inspection against recorded contracts | Exactly one existing-vault enrollment; no alternate copy or partial profile | external-contract + automated |
| [ ] | Core, Qt, and projection contain no client identifier, service endpoint, secret device code, or token material; the host display projection contains only user code, validated verification URI, and expiry. | Compile-time boundary tests plus source/symbol/type inventory | Forbidden facts cannot cross; exact allowed display fields reach Qt | automated |
| [ ] | Pending, denied, expired, rate-limited, and disabled outcomes are typed and clean. | Host replay of every sanitized real-service fixture | Distinct terminal/continuation behavior; zero retained transient material | automated |
| [ ] | Polling honors interval, backoff, total bound, cancellation, and shutdown. | Controlled-clock/manual-executor host tests | No early/late/extra poll; deterministic erasure and quiescence | automated |
| [ ] | Certificates and redirects fail closed on both endpoints. | Native HTTPS hostile loopback contracts | Zero forwarded credential/body on certificate, unknown-authority, or query redirect | automated |
| [ ] | Off-authority verification addresses never open. | Address-validation and browser-capability cut test | Typed refusal and zero browser calls | automated |
| [ ] | Browser-open failure leaves authorization live and completable elsewhere. | Qt/host failing-opener public course | Code/address remain; later authorization succeeds | automated |
| [ ] | Manual token enrollment remains functional for every profile. | Public composition manual-before/device/manual-after courses | Existing vault path remains first-class and unchanged | automated |
| [ ] | A selected GitHub App refreshes and rotates vault material safely. | Controlled-clock host, native HTTPS refresh, and vault rotation/restart contracts | Refresh precedes expiry; stale/canceled results cannot rotate; failure becomes typed re-enrollment with no leaked prior material | automated |
| [ ] | Unregistered and GHES profiles refuse with zero requests. | Tests-disabled public counters and hostile capability selection | Exact `device_flow_unavailable`; zero constructed/sent requests | automated |
| [ ] | Evidence is local/target-native and secret-free. | Clean matrices, Release/runtime/dependency scans, evidence audit | No GitHub Actions evidence and no secret/transient material | automated + target-native |

Every completed row records its exact command or attended interaction, result,
commit, and durable evidence in `docs/verification/RFC-BPR-0016.md`. Registered
selectors must be enumerated there before final replay; invented future command
names do not count.

The seeded exact commands, proof conditions, pending results/evidence, and
commit fields live in
`docs/verification/RFC-BPR-0016.md#final-command-ledger`; BPR-0282 replaces
planned selectors only by updating that ledger and this anchor atomically.

BPR-0282 exercised that clause once, on 2026-08-25. The planned selector set
had no row on which the first acceptance criterion below — one existing-vault
enrollment with no settings or SQLite copy — could be observed: all three
planned application selectors run against a build with no registered identity,
where nothing enrolls and the claim is vacuously true.
`application_device_authorization_registered_course` was added to the focused
matrix and to the ledger in the same commit. It composes the recorded BPR-0276
grant against the real host, the real enrollment boundary, and the real vault,
then reads every file under the application data root looking for the enrolled
token. The tests-disabled equivalent still requires a distributable identity
and remains BPR-0318's to enable and this card's to replay.

## Phase checklist

- [ ] The immutable planning snapshot, complete live issue graph, verification
      ledger, and attended packets are committed and revalidated at closeout.
- [ ] Eligibility decisions and external contracts are committed and every RFC
      gate is satisfied or narrowed by an exact accepted exception.
- [ ] Core, GitHub adapter, host, Qt, and production composition slices are
      terminal in dependency order.
- [ ] Security/hostile/public/Release and every claimed supported-platform
      target-native matrix is terminal; unavailable platforms remain explicit.
- [ ] BPR-0318 records a distributable production identity before any platform
      becomes available; the private owner-only verification identity is never
      treated as distribution authority.
- [ ] Simplification is terminal before the final documentation impact review.
- [ ] Every documentation-ledger row in
      `docs/verification/RFC-BPR-0016.md` is updated or has an exact no-impact
      disposition.
- [ ] Final clean replay and independent capability-critical review accept the
      immutable closeout head.
- [ ] The idempotent `verify_rfc0016_platform_course.py` final-head course passes
      with `--expected-device-selection enabled --fresh` on macOS, Linux, and
      Windows after BPR-0318 and every implementation, simplification,
      documentation, and platform card is terminal; one-time flip records are
      provenance only.
- [ ] Only after every row is satisfied, the RFC status, index, verification
      ledger, and issue board transition atomically to `Auto-Verified`/terminal.

## Closeout checklist

- [ ] BPR-0035 records RFC-BPR-0002 Auto-Verified after BPR-0036's real Windows
      x64 evidence; no available-platform substitute is counted.
- [ ] BPR-0275 through BPR-0284 and BPR-0318 are terminal and the documentation
      impact ledger is resolved against the immutable base.
- [ ] Clean `dev`, `core-only`, sanitizer, tests-disabled Release, focused native
      HTTPS/Qt/application, format, strict tidy, documentation, dependency,
      runtime, secret-scan, and diff gates pass at the closeout head.
- [ ] The verification ledger records exact commands, interactions, results,
      artifact paths, and commits for every acceptance row and claimed
      supported platform; macOS alone does not prove Windows or Linux.
- [ ] An independent `rfc-critical-reviewer` falsifies the exact claim matrix,
      application-selection path, packet provenance, and negative controls with
      no unresolved blocker.
- [ ] RFC status, verification ledger, issue board, and all claimed commits and
      artifacts agree before the RFC is marked `Auto-Verified`.
