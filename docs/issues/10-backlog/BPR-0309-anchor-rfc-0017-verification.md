---
id: BPR-0309
title: Anchor RFC-BPR-0017 capability-critical verification
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0035, BPR-0285, BPR-0297, BPR-0298, BPR-0299, BPR-0300, BPR-0301, BPR-0302, BPR-0303, BPR-0304, BPR-0305, BPR-0306, BPR-0307, BPR-0308, BPR-0312, BPR-0313, BPR-0314, BPR-0315, BPR-0316, BPR-0317, BPR-0558]
---

# Anchor RFC-BPR-0017 capability-critical verification

RFC base: `a45750f6bd84716240aa27a20f0734f33808ab8e`.

## Run objective and membership

This is the terminal anchor for the second RFC in the requested sequence,
after RFC-BPR-0016. RFC-BPR-0017 membership is BPR-0296 through BPR-0309 plus
BPR-0312 through BPR-0317. RFC-BPR-0016 anchor BPR-0285 and RFC-BPR-0002 cards
BPR-0035/BPR-0036 are external dependencies only and retain their own RFC
ownership. Newly discovered required work enters the dependency closure before
closeout.

## Acceptance checklist

| State | Criterion | Final direct surface | Expected result | Gate |
|---|---|---|---|---|
| [ ] | Public GitHub.com PR opens through the existing provider/normalizer. | Tests-disabled recorded-contract public composition | One path, no parallel provider/normalizer/fixture system | automated |
| [ ] | Public Git/cache/epoch authority renders exact selected-file diff content without viewer identity. | Credential-free smart-HTTP, discriminated authority, versioned SQLite migration/restart, and real rendered diff course | Exact path/base/head/content, migrated authenticated rows, isolated namespaces, no credential, stable anonymous authority | automated + native |
| [ ] | GraphQL and viewer identity refuse before request construction. | Provider admission counters | Distinct typed reasons and zero requests | automated |
| [ ] | Anonymous read shape is exact and triage/action capabilities are absent. | Core/host/projection/Qt matrix | No defaulted viewer, resolution, viewed, or actionability state | automated |
| [ ] | No credential or authorization path is touched. | Vault/native-HTTPS positive canaries | Zero lookup/enrollment/header and deliberate anonymous authority | automated |
| [ ] | Rate exhaustion is typed and not retried. | Recorded limit fixture plus controlled host scheduler | Exact limit diagnostic and zero request past refusal | external-contract + automated |
| [ ] | 404 response discloses neither existence nor privacy. | Provider/Qt 404 course | Existence-neutral diagnostic only | automated |
| [ ] | GHES anonymous selection refuses with zero requests. | Tests-disabled profile selection counters | Exact unavailable reason and zero transport construction/send | automated |
| [ ] | Anonymous/authenticated switch invalidates all identity-sensitive state. | Host/application two-mode composition | No stale viewer, capability, queue, epoch, cache, or projection survives | automated |
| [ ] | Evidence is local/target-native and every enabled platform is proved. | Clean matrices and platform ledger | No GitHub Actions; unproved platforms remain unavailable | target-native |

## Phase checklist

- [ ] The immutable planning snapshot, complete live issue graph, verification
      ledger, and attended/external packet paths are committed and revalidated.
- [ ] Real-service evidence and the build/no-build product decision are valid,
      exact, digest-bound, and terminal.
- [ ] RFC-BPR-0002 is Auto-Verified or any implementation-only exception is
      recorded without weakening the final closeout dependency.
- [ ] Core, native HTTPS/provider, normalizer, host, Qt, and composition cards
      are terminal in dependency order.
- [ ] Security/target-native evidence, simplification, and final documentation
      impact are terminal.
- [ ] `docs/verification/RFC-BPR-0017.md` records exact final-head commands,
      interactions, results, commits, and durable evidence for every row.
- [ ] An independent `rfc-critical-reviewer` accepts the immutable head with no
      unresolved blocker before any `Auto-Verified` transition.
- [ ] Only after every row is satisfied, the RFC status, index, verification
      ledger, and issue board transition atomically to `Auto-Verified`/terminal.

## Final clean gates

Clean `dev`, `core-only`, sanitizers, tests-disabled Release, focused provider/
native-HTTPS/host/Qt/composition, format, strict tidy, documentation,
dependency, runtime, boundary, secret/credential absence, no-Actions, and diff
gates must all pass. Historical counts, the designated public observer, a
fixture/helper, or macOS evidence for another platform do not substitute.

The seeded exact commands, proof conditions, pending results/evidence, and
commit fields live in
`docs/verification/RFC-BPR-0017.md#final-command-ledger`. BPR-0306 replaces a
planned selector only by updating that ledger and this anchor atomically.
