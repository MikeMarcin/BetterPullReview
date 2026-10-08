# Proposed amendment: qualify the environment's Git

Status: unadopted proposal accompanying [RFC-DRAFT-0004](RFC-DRAFT-0004-ghes-exact-file-comparison.md).
The owner's 2026-10-07 decision selects qualification of the environment's Git
and restoration of file viewing. It does not publish this amendment or certify
an installed tool. The follow-up adds actor-login snapshot rejection and its
second original log to the same draft.

## Live basis and affected requirements

The exact content identities below bind this proposal to inspected live
sources. The build identifier in the incident logs is not proof of a source
revision match. These sources remain unchanged during authoring.

| Live source | SHA-256 |
|---|---|
| [docs/rfcs/RFC-BPR-0015-github-cli-production-enablement.md](../../../RFC-BPR-0015-github-cli-production-enablement.md) | `0ea8025859b4e2a53b257f21cdcd0cfd84b0e6f154738ecc05748592fefff58a` |
| [docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json](../../../plans/rfc-bpr-0015-cli-profile/milestone-plan.json) | `eeb2c94607c72f91e10617dab0b01f8d03e5660854283b614a39c151593af831` |
| [docs/issues/60-done/BPR-0788-close-rfc-0015.md](../../../../issues/60-done/BPR-0788-close-rfc-0015.md) | `78410c4abb1d3d43e4be7a0c32f1b15e4da3fe42f339a78a73a48f53a9d23463` |
| [docs/architecture/product-contract.md](../../../../architecture/product-contract.md) | `a7c81fe21263af5d3e32a0dfb451f0fca4beaeca837f1b9339ed25476babd9ca` |
| [docs/architecture/system-architecture.md](../../../../architecture/system-architecture.md) | `860a431b23cf34e84ea89c8b9b239073ecbac745130771dcd2b0c358c2d46ea1` |

RFC-BPR-0015's macOS Git-object qualification clause admits only gh 2.100.0
or 2.101.0 with Apple Git 2.50.1 (Apple Git-155). The product and system
contracts repeat that qualification. The plan's `opt-in-cli-read` boundary
still names its initial gh 2.100.0/Apple Git baseline; its configured-dependency
refusal exit and `native-qualification-and-closeout` evidence exits remain
obligations. BPR-0788 C2, C3 and C6 record historical fixture/native evidence
and closeout; they do not qualify the newly reported installation.

## Proposed before and after

Before: the exact Apple Git banner is the sole qualified Git admission.
After: retain existing qualification and add the owner's actual selected Git
installation only after bounded version/probe facts and direct installed-tool
acquisition, process confinement and exact-object verification evidence pass
on its actual supported platform. Define the supported installation precisely
from that evidence; do not replace an exact gate with an arbitrary version
range. Classify probe failure separately from unsupported installation and
give an actionable recovery through the existing typed projection.

The new draft's AC01–AC04 require reproduction, exact comparisons, recovery
and unchanged acquisition guarantees. AC05 requires the read-only real GHE
rendered outcome. AC06 additionally owns the newly reported actor-login
snapshot rejection. No change to the identity relationship contract is proposed
from the anonymized logs alone: first establish whether the conflict comes
from legitimate representation, adapter aggregation or actually inconsistent
input. Any demonstrated need to change an adopted identity requirement needs
its own precise reconciliation within that implementation scope.

Preserve CLI opt-in, host/account observations, credential isolation, process
write denial, temporary-store bounds, cancellation, exact OID verification,
stale-result rejection and documented residual races. Keep the 32 MiB per-file
and supervised 96 MiB/128-entry store thresholds. No native credential fallback,
TLS bypass, approximate source, unbounded clone, broader gh support, live
publication is authorized by this proposal. It does not by itself enable an
additional platform: if the selected client platform lacks production support,
include its explicit native gates and affected platform-contract reconciliation
in the registered implementation scope before claiming restoration there.

## Reconciliation and next authorized action

In a later authorized implementation session, inspect current source and active
writers, register the draft and allocate its new issue scope, then reconcile
the RFC-BPR-0015 qualification clause, product/system guidance and affected
plan boundary explicitly against the installed-tool evidence. Keep old Done
cards and their producing-revision acceptance as historical evidence; do not
reuse their results to accept the added installation or change their status
merely because this new failure exists. Preserve unrelated exits/dependencies
and pending native or human decisions. Renew any affected active bindings before
publication. No active binding audit has been performed during authoring.

This changes an adopted qualification requirement. Replaying plan adoption or
using `check-amendment` as if it permitted changed requirements is not an
amendment route; that helper only checks compatible refinements preserving
scope, selectors and exit text. Import and plan reconciliation belong to the
implementation mandate, not this draft-writing action.

No material support choice remains open. Selected executable/version facts,
actor-conflict provenance, installed-tool qualification and the real rendered
outcome remain evidence obligations. Neither attached log claims a repaired
comparison or acceptance of a new platform/tool combination.
