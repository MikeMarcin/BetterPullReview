# Proposed amendment: identify draft pull requests on Home

This accompanies [the draft](RFC-DRAFT-0006-home-draft-pull-requests.md).
The owner's 2026-10-11 request establishes an obvious Home indication of
GitHub draft status. The proposed treatment is a neutral outlined Draft chip
before the PR identity, shared by queue and Recently viewed rows. Live
contracts, cards, statuses and plans remain unchanged during authoring.

## Sources inspected

These SHA-256 identities bind the live sources read for this proposal.
Reconcile drift before adoption. They are document source identities, not
an implementation baseline or acceptance evidence.

| Source | SHA-256 |
|---|---|
| [RFC-BPR-0026](../../RFC-BPR-0026-personal-review-home-pr-url-navigation.md) | `92381bda1642ec9e9abf85db4f3f37142743d756b95d20421b98fbaaf90c50f5` |
| [RFC-BPR-0032](../../RFC-BPR-0032-calm-home-and-accounts-surfaces.md) | `d51f592b94f87a08362b0d459cd05dce88cf30c9016647c957c7bc675f19f4b8` |
| [RFC-BPR-0066](../../RFC-BPR-0066-home-authored-pull-requests.md) | `ca973be45c11dcd0027f5bd4009bd9f4417f8d4856e7d0480d3aa279b61c83bc` |
| [Workbench Home specification](../../../ux/workbench-specification.md#home) | `cd0dc75b41b7be0fb416219921960d8c2e4f2650c3024e02377b20393ebc6757` |
| [Shell-surfaces plan](../../plans/bpr-shell-surfaces-v1/milestone-plan.json) | `fa4c70cd9ed375e3f63299b2a2c8911853832b26073cf1cc9c895f41db04f1d3` |
| [BPR-0776](../../../issues/60-done/BPR-0776-render-the-list-as-state-panels-with-one-row-anatomy.md) | `5d0860b559e730203614d2d71633ee7fe1a9fe2c7a4263584b305d11405a003f` |
| [BPR-1379](../../../issues/60-done/BPR-1379-populate-home-pull-request-titles-before-opening.md) | `4b8349952ef67de1435d9191709acd1b9e5b18f0dd8a867f530df56c2f9238f8` |

## Proposed reconciliation

| Adopted contract | Addition on adoption |
|---|---|
| RFC-BPR-0026, Home and attention; AC-02 | Include observed draft status in Home's PR information without making it an attention reason, excluding drafts or changing their counts. |
| RFC-BPR-0032, Home row anatomy; A6 | Add the leading chip to the shared queue/recent row. Preserve the attention-only colour rule, section order, panels and activity explanation. |
| RFC-BPR-0032, implementation allocation and A8 | Carry draft observations through the host/core presentation boundary and obtain fresh rendered, keyboard and accessible evidence for the changed row. Historical candidate evidence does not establish the new chip. |
| RFC-BPR-0066, Keep activity visible and Fit the existing Home controls | Extend its referenced row anatomy with the chip; retain its settled waiting placement, authorship decision, neutral section header and activity treatment. |
| Workbench specification, Home row anatomy | Describe the chip's position, known/unknown and cached-state behavior, and accessible wording through the linked draft. |

## Work, dependencies and evidence

The shell-surfaces plan's `home-page` and `verification-closeout` milestones
retain their original scope and exits. Done BPR-0776 supplies the shared row
foundation; Done BPR-1379 supplies bounded Home metadata refresh before
opening. Neither card nor its old evidence accepts the new draft indication.
Implement this addition as new follow-up scope rather than replaying completed
milestones. No existing plan exit or historical verdict is rewritten here.

Use the existing personal Home, metadata, recent-history and row capabilities
as implementation prerequisites. Preserve RFC-BPR-0066's settled decisions,
current Home selection/opening behavior, and all unaffected native/human
obligations, including RFC-BPR-0032 A8. This proposal neither closes that RFC
nor claims system appearance or human acceptance from offscreen renders.

In an authorized implement-rfc session, reconcile overlapping active Home
work and any prepared candidate whose inputs would change, register the new
scope and adopt the named prose additions with fresh bindings. Verify the
changed data and composed row paths. Concrete storage and type choices
remain implementation work; no material choice awaits an owner answer.
