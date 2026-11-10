# 0.5.0 targets: proposed pack

This is an unadopted authoring pack in two parts. The first, RFC-DRAFT-0001
to 0009, answers the owner's notes of 2026-11-10 from a day with the 0.4.2
prerelease. The second, RFC-DRAFT-0010 to 0015, is the local PR drafts pack,
which the owner chose the same day to absorb here so that there is one pack
to import and implement. [RECONCILIATION.md](RECONCILIATION.md) records what
was brought in, what was left out, and the rule that holds wherever the two
parts meet. Nothing here has been implemented, no live ID has been
allocated, and the board is untouched.

The owner's notes are preserved verbatim in
[SOURCE_NOTES.md](SOURCE_NOTES.md). The mockups for the surfaces that needed
design are in the
[design round](../../../design/v0-5-0-targets-v1/index.html).
[DECISIONS.md](DECISIONS.md) is the single record of what the owner actually
said, what is only proposed, the open choices, the checks run on this draft
and the unevaluated execution eligibility. The [glossary](GLOSSARY.md) fixes
the few terms the RFCs share.

## The RFCs

| RFC | Outcome | Notes answered |
|---|---|---|
| [RFC-DRAFT-0001](RFC-DRAFT-0001-home-says-whose-move-my-pull-request-is.md) | Home says whose move each of my pull requests is, and why | 1 |
| [RFC-DRAFT-0002](RFC-DRAFT-0002-tour-publishing-in-reach-and-reported.md) | Tour publishing stays in reach, reports each step, chooses its own form, and the tour on Overview is repaired | 2, 3, 6, 7 |
| [RFC-DRAFT-0003](RFC-DRAFT-0003-tour-conversation-that-changes-the-tour-in-one-turn.md) | The tour's chats are uncluttered, change the tour in one turn, run Enhance in the part's session, and write references and moved code well | 4, 8, 9, 10 |
| [RFC-DRAFT-0004](RFC-DRAFT-0004-agent-chat-conventions.md) | Every agent chat shares rules for code references, the send key, cost and the agent colour | 5, 11, 12, 14 |
| [RFC-DRAFT-0005](RFC-DRAFT-0005-agent-questions-say-what-they-point-at.md) | An agent question states its subject, suggestions always send, and the scope list goes | 17 |
| [RFC-DRAFT-0006](RFC-DRAFT-0006-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md) | A running turn shows its work and helpers and cannot end silently | 18 |
| [RFC-DRAFT-0007](RFC-DRAFT-0007-agent-scratch-folder-and-opt-in-builds.md) | An agent can be given a scratch folder, and builds and tests on request | 15 |
| [RFC-DRAFT-0008](RFC-DRAFT-0008-a-review-opens-fresh-and-a-submission-always-ends.md) | A review opens fresh, and Approve always publishes or says why not | 13, 16 |
| [RFC-DRAFT-0009](RFC-DRAFT-0009-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md) | Arriving at a stop shows its code, a stop is accepted the way a file is, and Suggest an outline leaves the application | 19 to 23 |

Every note is answered by an RFC above. Notes 19 to 23 were added by the
owner during authoring.

### Absorbed from the local PR drafts pack

| RFC | Outcome | Was |
|---|---|---|
| [RFC-DRAFT-0010](RFC-DRAFT-0010-agent-and-prompt-sharing.md) | Share selected agent settings and prompt customizations as a file | RFC-DRAFT-0001 there |
| [RFC-DRAFT-0011](RFC-DRAFT-0011-local-pr-drafts.md) | Prepare a local PR draft from a worktree and resume it from Home | RFC-DRAFT-0002 there |
| [RFC-DRAFT-0012](RFC-DRAFT-0012-author-evidence.md) | Keep claims, checks and results attached to the candidate examined | RFC-DRAFT-0003 there |
| [RFC-DRAFT-0013](RFC-DRAFT-0013-local-tours-and-description.md) | Write the description and Review Tour before a pull request exists | RFC-DRAFT-0004 there |
| [RFC-DRAFT-0014](RFC-DRAFT-0014-draft-publication-and-continuity.md) | Create the draft pull request and keep the evidence and tour | RFC-DRAFT-0005 there |
| [RFC-DRAFT-0015](RFC-DRAFT-0015-optional-workflow-extensions.md) | Optional shareable workflow actions through an external companion | RFC-DRAFT-0006 there |

Their decision record, UX flow, current-behaviour observations and the
owner's intent are in [local-drafts/](local-drafts/DECISIONS.md). Each of
these RFCs has a section "Fitted to this pack's other decisions" that states
how it takes the behaviour decided in RFC-DRAFT-0001 to 0009.

## The work

The [implementation entrypoint](IMPLEMENTATION_PROMPT.md) points to the
canonical [milestone-plan.json](milestone-plan.json) and its generated
[MILESTONES.md](MILESTONES.md). The [portable schema](milestone-plan.schema.json)
is copied unchanged from the discovered implement-rfc skill's offline kit.

| Milestone | Cards |
|---|---|
| `dependable-session` | [DRAFT-0022](cards/DRAFT-0022-submission-always-ends.md), [DRAFT-0023](cards/DRAFT-0023-review-opens-fresh.md) |
| `agent-tab-asks` | [DRAFT-0014](cards/DRAFT-0014-question-subject-and-attachment.md), [DRAFT-0010](cards/DRAFT-0010-code-references-everywhere.md), [DRAFT-0011](cards/DRAFT-0011-send-key-and-cost.md), [DRAFT-0012](cards/DRAFT-0012-agent-accent-rule.md) |
| `agent-work-visible` | [DRAFT-0016](cards/DRAFT-0016-background-work-and-turn-lifecycle.md), [DRAFT-0017](cards/DRAFT-0017-activity-block-and-quiet-notice.md) |
| `agent-scratch` | [DRAFT-0019](cards/DRAFT-0019-agent-scratch-folder.md), [DRAFT-0020](cards/DRAFT-0020-opt-in-builds-and-tests.md) |
| `tour-publishing` | [DRAFT-0003](cards/DRAFT-0003-tour-tab-bands.md), [DRAFT-0004](cards/DRAFT-0004-tour-publication-form-and-steps.md), [DRAFT-0005](cards/DRAFT-0005-overview-tour-summary.md) |
| `tour-stops` | [DRAFT-0025](cards/DRAFT-0025-stop-code-on-arrival-and-rest-versus-edit.md), [DRAFT-0026](cards/DRAFT-0026-stop-acceptance.md) |
| `tour-conversation` | [DRAFT-0007](cards/DRAFT-0007-tour-chat-foot-and-one-turn-changes.md), [DRAFT-0008](cards/DRAFT-0008-tour-references-and-writing-rules.md) |
| `home-next-move` | [DRAFT-0001](cards/DRAFT-0001-home-next-move.md) |
| `accepted-scope` | Closeouts [DRAFT-0002](cards/DRAFT-0002-close-home-next-move.md), [DRAFT-0006](cards/DRAFT-0006-close-tour-publishing.md), [DRAFT-0009](cards/DRAFT-0009-close-tour-conversation.md), [DRAFT-0013](cards/DRAFT-0013-close-agent-chat-conventions.md), [DRAFT-0015](cards/DRAFT-0015-close-agent-question-subject.md), [DRAFT-0018](cards/DRAFT-0018-close-agent-work-visible.md), [DRAFT-0021](cards/DRAFT-0021-close-agent-scratch.md), [DRAFT-0024](cards/DRAFT-0024-close-fresh-review-and-submission.md), [DRAFT-0027](cards/DRAFT-0027-close-tour-stops.md) |

| `share-agent-prompts` | [DRAFT-0028](cards/DRAFT-0028-portable-agent-and-prompt-configuration-exchange.md), [DRAFT-0029](cards/DRAFT-0029-selective-settings-import-and-export.md), closeout [DRAFT-0030](cards/DRAFT-0030-close-the-independent-configuration-sharing-slice.md) |
| `local-draft-entry` | [DRAFT-0031](cards/DRAFT-0031-discover-the-worktree-and-prospective-pr-comparison.md), [DRAFT-0032](cards/DRAFT-0032-persist-local-pr-draft-candidates-and-author.md), [DRAFT-0033](cards/DRAFT-0033-open-and-resume-local-pr-drafts-from.md), closeout [DRAFT-0034](cards/DRAFT-0034-close-local-pr-draft-entry-and-persistence.md) |
| `author-evidence` | [DRAFT-0035](cards/DRAFT-0035-retain-candidate-bound-evidence-and-qualified-provenance.md), [DRAFT-0036](cards/DRAFT-0036-curate-evidence-in-local-preparation.md), closeout [DRAFT-0037](cards/DRAFT-0037-close-author-evidence-preparation.md) |
| `local-tour-preparation` | [DRAFT-0038](cards/DRAFT-0038-add-local-description-and-tour-ownership.md), [DRAFT-0039](cards/DRAFT-0039-prepare-an-evidence-backed-tour-and-publication.md), closeout [DRAFT-0040](cards/DRAFT-0040-close-pre-pr-description-and-tour-preparation.md) |
| `publish-prepared-draft` | [DRAFT-0041](cards/DRAFT-0041-create-the-draft-pr-from-the-pushed.md), [DRAFT-0042](cards/DRAFT-0042-associate-publication-and-resume-remaining-follow-up.md), closeout [DRAFT-0043](cards/DRAFT-0043-close-built-in-draft-publication-and-continuity.md) |
| `workflow-companion-contract` | [DRAFT-0044](cards/DRAFT-0044-share-optional-workflow-configurations.md), [DRAFT-0045](cards/DRAFT-0045-provide-a-resumable-companion-action-contract.md) |
| `workflow-effectful-actions` | [DRAFT-0046](cards/DRAFT-0046-hand-off-effectful-actions-and-recover-their.md), closeout [DRAFT-0047](cards/DRAFT-0047-close-optional-workflow-integration.md) |

`dependable-session` comes first because one of its two defects stops a
review from being approved. The seven absorbed milestones keep their own
order and their closeouts, and follow the first nine in the plan's default
order. `local-tour-preparation` waits for this pack's tour and chat
milestones and `publish-prepared-draft` for `dependable-session`. All seven
absorbed milestones are part of the 0.5.0 release, by the owner's decision
of 2026-11-10.

## What is not ready

For RFC-DRAFT-0001 to 0009 nothing waits on the owner. Five choices were asked and answered on
2026-11-10, and [DECISIONS.md](DECISIONS.md) quotes each answer: Home
placement (a reason inside My pull requests), the Overview faults (the three
the RFC fixes), the scratch folder's default (on to begin with, a per-agent
starting state, remembered per pull request), the tour stop design (accepted
as drawn in the stop mockup) and builds and tests (included, with the limits
in RFC-DRAFT-0007).

The owner accepted drawings and directions, not running behaviour. Each
RFC's look on a frozen build remains a criterion.

Six rules that fit the absorbed RFCs to the first nine were the authoring
session's proposals. The owner confirmed all six on 2026-11-10; they are
listed in [RECONCILIATION.md](RECONCILIATION.md#rules-the-owner-confirmed)
with the cards they govern.

One thing about the absorbed part remains the owner's: whether the
enterprise overview may be committed.

The enterprise overview behind RFC-DRAFT-0015 was deliberately not copied
into the repository; RFC-DRAFT-0015 does not need it to be implemented.

One cause is unconfirmed: why Approve stalls. RFC-DRAFT-0008 makes finding
it the first work of DRAFT-0022 and requires the stated conditions and the
bounded wait whatever the cause turns out to be.
