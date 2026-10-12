# Proposed amendment: separate authored pull requests on Home

This accompanies [the draft](RFC-DRAFT-0005-home-authored-pull-requests.md).
The owner's 2026-10-11 request settles separation from Needs me. Q1 in the
draft records their answer: keep waiting PRs in Waiting on others. All other
authored rows move to My pull requests. No material choice remains open.
The live contracts, statuses, cards and plans remain unchanged during authoring.

## Sources inspected

These SHA-256 identities bind the live files read for this proposal. Reconcile
any drift before adopting the change; these are source identities, not an RFC
implementation baseline or acceptance evidence.

| Source | SHA-256 |
|---|---|
| [RFC-BPR-0026](../../RFC-BPR-0026-personal-review-home-pr-url-navigation.md) | `4531c9c1c647bf0b73f7c1aaaef3248d52bcaf20c238568b8a78920e542b7b35` |
| [RFC-BPR-0028](../../RFC-BPR-0028-review-workbench-direction.md) | `9e7161725875c5e7c73d47d386c567902d491993802f120d03201439409802ca` |
| [RFC-BPR-0032](../../RFC-BPR-0032-calm-home-and-accounts-surfaces.md) | `56b2f703c8fa04c2cd07aae9ac5c78368b4ac8b7300bbc1de0ec3bebf314599a` |
| [Workbench Home specification](../../../ux/workbench-specification.md#home) | `296c8278e758540f055d197a44daa7f29982a4aae10af22af30e03eb7e8bf2e9` |
| [Shell-surfaces plan](../../plans/bpr-shell-surfaces-v1/milestone-plan.json) | `fa4c70cd9ed375e3f63299b2a2c8911853832b26073cf1cc9c895f41db04f1d3` |
| [UX redesign plan](../../plans/ux-redesign-pack-0028/milestone-plan.json) | `22a2dbcefc33ebb2b968f6d63cf79dd462882960ca4be9e632fee47a051b1987` |
| [BPR-0776](../../../issues/60-done/BPR-0776-render-the-list-as-state-panels-with-one-row-anatomy.md) | `5d0860b559e730203614d2d71633ee7fe1a9fe2c7a4263584b305d11405a003f` |
| [BPR-0783](../../../issues/60-done/BPR-0783-add-group-state-headers-account-chips-and-filter-to-home.md) | `b28c31bdbd12ef8deb43e4eb2842bad0fc3a68bea7b5e84c1e780a001916703f` |

## Proposed reconciliation

| Existing contract | Change on adoption |
|---|---|
| RFC-BPR-0026, Home and attention; AC-02 | Separate authored membership from Home's Needs me placement and count while retaining its attention evidence. Keep its waiting-only placement in Waiting on others. Deduplication, unknown detail and personal discovery remain required. |
| RFC-BPR-0028, Home; AC-06 | Reconcile the named groups with the selected draft behavior. Home remains the window's other view with its existing navigation and account boundary. |
| RFC-BPR-0032, Home list/panels; A6 | Replace bucket-only grouping with the selected section membership and order. The mixed-state authored panel has a neutral header; its rows retain state words and colours. Preserve row anatomy, sticky headers and empty-panel rules. |
| RFC-BPR-0032, Group/account/Filter; A7 | Apply the new section order within the existing groups and when headers are hidden. Add the authored panel's per-window collapse behavior while preserving the owner's session-only decision for named group panels and bands. |
| RFC-BPR-0032, implementation allocation and A8 | Its original “Core. No change” allocation does not constrain the new deterministic section decision. Supply focused composed, rendered and keyboard evidence for the changed Home states; prior evidence cannot establish them. |
| Workbench specification, Home | Reconcile list order, unknown authored placement, panel colour/summary and collapse descriptions. Preserve the separate Waiting on others panel, its blue waiting state and its saved collapse. |

The current discovery and notification contracts do not change. Preserve
[RFC-BPR-0062's adopted onboarding correction](../../RFC-BPR-0062-saved-repository-onboarding.md#owner-correction-amendment--2026-10-08),
the later [Home layout correction](../../../issues/60-done/BPR-1381-home-keeps-its-layout-still.md),
and the current selection and opening behavior documented in the Home
specification. This proposal does not restore older layouts quoted in the
historical RFCs or add a redesign of the surrounding surfaces.

## Work, dependencies and evidence

The shell-surfaces plan's `home-page` and `verification-closeout` milestones
contain the old presentation and its acceptance. The UX redesign plan's
`plan-rfc-0026` and `close-rfc-0026` milestones reconcile personal Home against
that presentation. Their historical grouping and exit text remain intact;
this changed requirement needs new implementation scope rather than replaying
or silently rewriting a completed milestone. BPR-0776 and BPR-0783 remain Done.

The implementation consumes existing personal discovery, accepted queue
membership and attention, and Home's per-window layout storage. Register
dependencies on those capabilities using the live execution workflow; do not
invent a new dependency on unrelated pending RFC closeouts. This draft neither
closes RFC-BPR-0032 nor waives any of its outstanding obligations.

In an authorized implement-rfc session, carry forward the recorded Q1 answer.
Reconcile any overlapping active writer or prepared Home candidate before
changing its inputs. Adopt the new scope and
the named live prose changes with fresh bindings; preserve unaffected human,
native and external evidence and obtain new evidence only for the changed
surface. No plan amendment or implementation readiness is claimed here.
