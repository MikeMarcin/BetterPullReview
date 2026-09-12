---
schema: "implement-rfc.card.v1"
id: "BPR-0750"
title: "Verify and close RFC-BPR-0029"
status: "Ready"
rfcs: ["RFC-BPR-0029"]
depends_on: ["BPR-0743"]
priority: "P1"
closeout_for: ["RFC-BPR-0029"]
metadata: {"accepted_base":"5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d","owner_acceptance":"2026-09-12 mandate r17"}
---

# Verify and close RFC-BPR-0029

## Objective and immutable base

Verify the accepted [RFC-BPR-0029](../../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md)
against its nine acceptance criteria and the direct surfaces in its verification
matrix. The accepted revision is the immutable baseline for this closeout.

RFC base: `5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d`.

The owner accepted the revised proposal under mandate r17.
[[BPR-0743]] implements the product work. RFC-BPR-0028's epoch-line handle
criterion stays on [[BPR-0646]] and must receive its own owner verdict.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| Two independent catalogues reading the same force-push history through the host assign equal identities and deterministic order to the **same evidenced occurrences**. A→B→A yields two distinct identities for A. A missing event never yields a fabricated second A, and the history declares the gap. | Epoch host over two independent catalogues and a recorded force-push timeline; compare exact identities and order. | Not run | None |
| An ordinary live transition yields one occurrence per observed transition, not one per surviving commit. Its portable identity agrees across signed-in, signed-out and recreated catalogues when the same occurrence is unambiguous; ambiguous snapshots remain marked unresolved. | Host and provider fixture with one multi-commit push, observed head transitions, signed-in/signed-out and recreated catalogues. | Not run | None |
| A live read that finds a new head for the same pull request still appends an author-upload epoch, and an imported epoch advances neither the latest live ordinal nor the previous distinct head ordinal. | Second live handoff with a changed head and imported historical occurrence; inspect causes and live ordinals. | Not run | None |
| An update observed live and later overwritten by a force push stays on the install that observed it. The epoch line says when the history it holds may be partial: when the timeline read failed, was refused or was cut off by the page cap, or when a force push overwrote updates this install did not see. | Composed failure, refusal, cap and unseen-force-push cases. | Not run | None |
| Live epochs retain their observed base. An imported occurrence with no evidenced historical base shows `unknown`; a computed comparison base is separately labeled and comparison can be unavailable. No imported row claims the current base ref or merge base was its original historical base. | Core, cache and composed presentation cases for absent and rewritten bases. | Not run | None |
| Event dates are labeled GitHub event times, other dates commit dates, and first-seen remains local. Equal timestamps have a deterministic tie-break without a false push-order claim. | Host and presentation cases for mixed dates, equal timestamps and source labels. | Not run | None |
| A current-version catalogue migrates without changing any old epoch row's identity key, incarnation or ordinal. Unresolvable host or duplicate-head provenance remains explicitly unresolved; review-ledger, window-restoration, retained-ref, MCP and agent-launch references to old epochs still resolve. | Copies of current-version duplicate-head and GHES files; inspect local keys, incarnation, ordinals, ledger, restoration, retained refs, MCP and agent bindings. | Not run | None |
| The history read is anonymous REST and stays within a stated page cap, and an open that cannot make the read still produces today's live epoch. | Bounded transport and composed open path, with request counts and failure injection. | Not run | None |
| BPR-0646's attended handle criterion is asked again, unchanged, on a signed bundle carrying this work. | Digest-bound attended candidate and actual owner verdict, recorded on BPR-0646. | Human pending | None |

## Completion rules

Record executed checks, results and producing commits against each row. Run the
documentation gate and `git diff --check` at closeout, reconcile RFC and index
status, and respect canonical RFC order; an Accepted RFC is not Auto-Verified
merely because implementation or an attended packet exists. No credential or
real pull-request mutation is permitted by this card.
