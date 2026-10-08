# Proposed amendment: saved-repository onboarding on Home

This accompanies [the onboarding draft](RFC-DRAFT-0001-saved-repository-onboarding.md).
It is a proposal, not an amendment applied to the live contracts. The owner's
2026-10-07 request establishes the discoverability problem; it does not record
acceptance of the drafted placement or the unanswered selection method.

## Exact adopted sources

The following SHA-256 identities describe the files read during authoring.
Paths are relative to the repository root. Reconcile drift before adoption.

| Source | SHA-256 |
|---|---|
| [RFC-BPR-0028](../../../RFC-BPR-0028-review-workbench-direction.md) | `9969cb78ee8ffc80bfff852e235acecf73419b8fc4a39866481b911963cfb010` |
| [RFC-BPR-0032](../../../RFC-BPR-0032-calm-home-and-accounts-surfaces.md) | `23a464a95de80c21ceb08eed05f735f376b9aec1f3e75c8df6f6fa86f02be3b2` |
| [Shell-surfaces plan](../../../plans/bpr-shell-surfaces-v1/milestone-plan.json) | `fa4c70cd9ed375e3f63299b2a2c8911853832b26073cf1cc9c895f41db04f1d3` |
| [BPR-0775](../../../../issues/60-done/BPR-0775-lead-home-with-the-bar-and-the-first-launch-card.md) | `2b884a5340a55abe075947c7a16d670785cce4c76f6201784e1e7d51159f29bf` |
| [BPR-0776](../../../../issues/60-done/BPR-0776-render-the-list-as-state-panels-with-one-row-anatomy.md) | `5d0860b559e730203614d2d71633ee7fe1a9fe2c7a4263584b305d11405a003f` |
| [BPR-0783](../../../../issues/60-done/BPR-0783-add-group-state-headers-account-chips-and-filter-to-home.md) | `b28c31bdbd12ef8deb43e4eb2842bad0fc3a68bea7b5e84c1e780a001916703f` |
| [BPR-0779](../../../../issues/60-done/BPR-0779-run-the-rendered-and-attended-course-for-the-three-surfaces.md) | `0ab8e83ad1443e3d42866be34cbc69c24caa0d64a66ce76817d4e12b679ee7fd` |
| [BPR-0781](../../../../issues/60-done/BPR-0781-close-rfc-0032.md) | `1b1ea4f24058aae9d6f2ad976eb9e299ecb7a5cd9a437d0577edd58f50b1a6ae` |

## Proposed contract changes

| Adopted requirement | Proposed reconciliation |
|---|---|
| RFC-BPR-0028, Home: “Rules and saved repositories live in Settings.” | Management remains in Settings; Home gains a visible action and a first-repository prompt that open the shared addition flow. |
| RFC-BPR-0032, Panels and A6: the all-empty sentence links to Reviews & queue. | Distinguish no saved repository from no matching/attention PRs. The former offers direct addition in the discovery row; the latter keeps the ordinary queue explanation. Settings remains reachable for management and observer rules. |
| RFC-BPR-0032, Filter and A7: footer points to Settings for repositories and rules. | Describe Settings as management of saved repositories and rules; addition is already directly available on Home. Keep the four filter sections and their window ownership. |
| RFC-BPR-0032, A5 and A8: first-launch/signed-out layout and rendered acceptance. | Keep the signed-out surface unchanged; after sign-in show the repository prompt without opening a modal automatically. Extend rendered/keyboard/attended evidence to the new signed-in states and addition dialog. |

The Open bar's grammar and arrival focus, list ordering, grouping, account
chips, public-reading history, settings authority and sign-in contracts remain
as defined by their owning RFCs. The draft changes how a reviewer reaches a
private save, not which work personal discovery finds or what saving means.

## Work and acceptance consequences

The adopted `home-page` and `verification-closeout` milestones contain the
affected Home presentation and evidence. Their completed cards and historical
accepted candidates remain terminal evidence for their exact designs; they
do not establish acceptance of this new Home row or dialog. The existing
plan's scope and exits are not rewritten during authoring. A later
implementation session should register the new scope and its own cards,
reconcile the named live prose, and supply new evidence for the changed surfaces.

RFC-BPR-0032 is still Proposed. This draft does not close it or waive its
RFC-BPR-0028 dependency or other outstanding obligations. Existing unrelated
work and attended candidates are not invalidated by a proposed amendment.
If adoption overlaps an active writer or a prepared candidate, reconcile that
overlap before changing its inputs and rebind only affected evidence.

The repository-selection choice is recorded in the draft and must be resolved
before implementing its dependent scope. All other internal decomposition is
left to implementation. No new live ID, execution mandate, owner acceptance,
or passing runtime observation is created by this note.
