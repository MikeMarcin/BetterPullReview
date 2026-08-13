---
id: BPR-0285
title: Anchor RFC-BPR-0016 capability-critical verification
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0035, BPR-0275, BPR-0276, BPR-0277, BPR-0278, BPR-0279, BPR-0280, BPR-0281, BPR-0282, BPR-0283, BPR-0284]
---

# Anchor RFC-BPR-0016 capability-critical verification

RFC base: `b969554fb45d838b2a4ea11811363bd7e158e47e`.

Historical runs are provenance only. Every row requires final replay at the
immutable closeout head and an independent capability-critical review.

| State | Acceptance criterion | Direct verification surface | Expected result | Gate |
|---|---|---|---|---|
| [ ] | GitHub.com device enrollment reaches the existing vault with no settings/SQLite copy. | Tests-disabled public composition plus vault/settings/SQLite inspection against recorded contracts | Exactly one existing-vault enrollment; no alternate copy or partial profile | external-contract + automated |
| [ ] | Core, Qt, and projection contain no client/endpoint/code/token material. | Compile-time boundary tests plus source/symbol/type inventory | Forbidden facts cannot cross the host boundary | automated |
| [ ] | Pending, denied, expired, rate-limited, and disabled outcomes are typed and clean. | Host replay of every sanitized real-service fixture | Distinct terminal/continuation behavior; zero retained transient material | automated |
| [ ] | Polling honors interval, backoff, total bound, cancellation, and shutdown. | Controlled-clock/manual-executor host tests | No early/late/extra poll; deterministic erasure and quiescence | automated |
| [ ] | Certificates and redirects fail closed on both endpoints. | Native HTTPS hostile loopback contracts | Zero forwarded credential/body on certificate, unknown-authority, or query redirect | automated |
| [ ] | Off-authority verification addresses never open. | Address-validation and browser-capability cut test | Typed refusal and zero browser calls | automated |
| [ ] | Browser-open failure leaves authorization live and completable elsewhere. | Qt/host failing-opener public course | Code/address remain; later authorization succeeds | automated |
| [ ] | Manual token enrollment remains functional for every profile. | Public composition manual-before/device/manual-after courses | Existing vault path remains first-class and unchanged | automated |
| [ ] | Unregistered and GHES profiles refuse with zero requests. | Tests-disabled public counters and hostile capability selection | Exact `device_flow_unavailable`; zero constructed/sent requests | automated |
| [ ] | Evidence is local/target-native and secret-free. | Clean matrices, Release/runtime/dependency scans, evidence audit | No GitHub Actions evidence and no secret/transient material | automated + target-native |

## Closeout checklist

- [ ] BPR-0035 records RFC-BPR-0002 Auto-Verified after BPR-0036's real Windows
      x64 evidence; no available-platform substitute is counted.
- [ ] BPR-0275 through BPR-0284 are terminal and the documentation impact ledger
      is resolved against the immutable base.
- [ ] Clean `dev`, `core-only`, sanitizer, tests-disabled Release, focused native
      HTTPS/Qt/application, format, strict tidy, documentation, dependency,
      runtime, secret-scan, and diff gates pass at the closeout head.
- [ ] An independent `rfc-critical-reviewer` falsifies the exact claim matrix,
      application-selection path, packet provenance, and negative controls with
      no unresolved blocker.
- [ ] RFC status, verification ledger, issue board, and all claimed commits and
      artifacts agree before the RFC is marked `Auto-Verified`.
