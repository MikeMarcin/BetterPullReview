---
schema: "implement-rfc.card.v1"
id: "BPR-1481"
title: "Compute intraline ranges with dtl after the owner's render review"
status: "Backlog"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1467"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r2","source":"RFC-BPR-0082 intraline decision: eligible pending AC06 (owner Q2 Accept for dtl, per-line use only)"}
---

# BPR-1481: Compute intraline ranges with dtl after the owner's render review

The adoption follow-up of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md) for intraline ranges. The candidate
`c-onp-intraline` (dtl 1.21, O(NP) over a pair's Unicode scalars) produced
valid spans on the whole corpus with a p95 per-pair time of 0.0227 ms against
the engine's 0.0250 ms and passed every macOS gate. The owner approved dtl for
the core on 2026-10-13 for per-line intraline comparison only. Adoption waits
on the owner's Accept of the render set (RFC AC06, prepared under
[BPR-1467](../60-done/BPR-1467-measure-diff-engine-candidates-on-macos.md)); a Reject
closes this card without change.

## Acceptance

- **AC1:** The owner's Accept of the dtl render set is recorded before any
  production change.
- **AC2:** dtl 1.21 is acquired by a pinned verified-archive provider as
  `BetterPullReview::Dtl`, linked only by `better_pull_review_core`, with
  provenance, licence, `dependencies.json`, notices, SPDX and corresponding
  source, and the package audits passing. `architecture_boundaries` keeps dtl
  to the one core file that computes intraline ranges; dtl is never used as a
  whole-file line matcher.
- **AC3:** Intraline ranges come from dtl's common scalars, counted through the
  engine's step meter; every span validates on the evaluation corpus, the diff
  suites and `core-only` pass, and the report's change in lost intraline
  emphasis and matrix accounting is stated and tested.
