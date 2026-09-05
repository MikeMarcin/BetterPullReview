---
schema: "implement-rfc.card.v1"
id: "BPR-0230"
title: "Compose defect correlation and defect cards into the shipped workbench"
status: "Done"
rfcs: ["RFC-BPR-0009"]
depends_on: ["BPR-0229","BPR-0238"]
---

# Compose defect correlation and defect cards into the shipped workbench

The defect core, correlator, and Qt card are built and tested independently;
the application constructs none of them.

## Required final state

- Accepted ordinary GitHub threads, local classification, and epoch evidence
  reach the composed correlator under exact identity.
- Classification, severity, category, lifecycle, and retained anchor history
  persist through BPR-0238's identity-scoped store and survive restart.
- A visible defect card consumes the accepted projection and emits only
  explicit reviewer intents.
- No host or Qt path can reach `verified` without the semantic reviewer
  intent; nearby changes and author replies remain evidence only.
- Counts and navigation are proven against a composed multi-defect queue rather
  than a single isolated card.
- A public fixture trace proves classification is local until a distinct
  shared action enters BPR-0229's coordinator.
- The standing composition inventory covers both the correlator and visible
  defect-card/queue surface; symbol linkage alone is not accepted as use.

## Implemented state

- The shipped conversations drawer exposes local classification, a visible
  defect card, exact counts, stable previous/next navigation, explicit
  lifecycle actions, local severity/category selectors, and unavailable-context
  text.
- Composition accepts only an exact ordinary review thread from the current
  snapshot whose path matches the real selected diff and whose subject carries
  a real line. File-level threads are refused rather than assigned a substitute.
  SHA-256 of the canonical thread identity supplies two independent durable
  integer halves; correlation requires both rather than aliasing through one
  lossy hash.
- Classification and every accepted lifecycle, availability, or
  reclassification change persist through the BPR-0238 review-ledger host
  before projection. Restart loads the exact histories without rewriting them.
- Author replies on identity-proven current source anchors enter
  `application::correlate` as evidence. Outdated anchors and retained-epoch
  browsing alone produce none. Evidence can produce `needs_verification`; only
  the card's explicit Verify command reaches `verified`.
- The shipped-executable seed/reopen contract selects the real file row,
  classifies two current right-side line threads, refuses a real file-level
  thread, drives a reverse epoch pair without changing either defect or
  permitting a third classification, explicitly
  verifies the evidence-bearing defect, reclassifies the other, navigates both
  stable identities, invokes the separate BPR-0229 Publish action, and proves
  exact restart state with zero reopen writes for GitHub.com and GHES 3.18.

## Verification

- `core_defect_workflow`
- `application_defect_correlation`
- `application_review_ledger_host`
- `sqlite_review_ledger_storage`
- `qt_defect_card`
- `qt_workbench_window_scale_100`
- `github_read_contract`
- `application_review_round_trip_composition_contract`
- `application_defect_composition_contract`

The public contract requires seed evidence `1111111001`, reopen evidence
`1111111111`, and `DEFECT_COMPOSITION_CONTRACT_OK`. Classification observes
zero fixture mutation submissions before the distinct shared Publish action.
