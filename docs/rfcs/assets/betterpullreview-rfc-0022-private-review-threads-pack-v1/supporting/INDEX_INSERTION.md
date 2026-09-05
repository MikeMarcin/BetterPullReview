# RFC Index Insertion for RFC-BPR-0022

This is source material for the integrator. Reconcile against the live
`docs/rfcs/README.md`; do not replace newer index language.

## Deferred/post-pack table row

Add after RFC-BPR-0021, unless a newer live ordering requires another position:

```markdown
| [RFC-BPR-0022](future/RFC-BPR-0022-private-review-threads-provenance-comment-composition.md) | Private Review Threads, Provenance, and Comment Composition | Proposed | RFC-BPR-0007, RFC-BPR-0008, RFC-BPR-0011, and RFC-BPR-0012; available-target implementation explicitly directed on 2026-09-04, with dependency-ordered Auto-Verified closeout |
```

## Explanatory paragraph

Add near the other post-pack explanations:

```markdown
RFC-BPR-0022 adds the private deliberation layer between AI/user analysis and
ordinary GitHub publication. Private reviewer and agent turns retain honest
authorship and derivation provenance; selected sources may be non-destructively
Squashed to Draft, but publication remains a separate RFC-BPR-0008 action. The
2026-09-04 product-owner direction authorizes import and available-target
implementation after reconciliation. It does not permit out-of-order
Auto-Verified closeout, hidden GitHub metadata, inferred runtime provenance, or
a second agent/mutation path.
```

## Status/frontier handling

- Keep `status: Proposed` while required dependencies or direct evidence remain
  open.
- Available-target cards may complete and move to done; the RFC closeout card
  remains backlog/parked as appropriate until ordered closeout is legal.
- Record target-native, owner-attended, or live-service gaps on live cards with
  the exact missing evidence and supplier.
- Do not alter the canonical RFC-BPR-0001 through RFC-BPR-0014 sequence.
