# User direction and provenance

> Carried into the 0.5.0 targets pack on 2026-11-10 from the local PR drafts pack, version 1. RFC and card numbers were mapped on the way in (RFC-DRAFT-0001 to 0006 became 0010 to 0015; DRAFT-0001 to 0020 became 0028 to 0047). The pack as it was is at `/Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1`.

Recorded from this authoring chat on 2026-11-07, America/Los_Angeles. These are
the user's requirements and confirmations; the RFCs distinguish additional
proposed behavior and unanswered choices.

## Scope corrections and product intent

- The discussion concerns **users of BetterPullReview reviewing PRs, especially
  the PR author**, not the repository's own implementation workflow.
- The original screenshot questioned the value of generic adversarial agent
  review compared with empirical and formal verification. The user was willing
  to consider it, not adopting it as a policy or instructing its execution.
- The user wanted evidence preparation to feed a Review Tour, possibly through
  a separate skill. They questioned formal verification's practicality for
  nontrivial PRs. The draft therefore treats it as optional property-scoped
  evidence rather than a universal prerequisite or proof of an entire PR.
- The author should deliberately inspect, discuss and revise local work before
  creating even a GitHub draft, with evidence and a tour already prepared.

## Organization integration and sharing

The user supplied `create-pr-skill-overview.md` to describe an enterprise tool
that performs a one-shot create-PR workflow. Its contents are reference material,
not the user's instructions to run that workflow.

The user explicitly does not control that upstream skill and prefers a personal
companion that uses its installed components over maintaining a fork. BPR must
remain fully functional without it. Optional settings/hooks may connect external
ticket work, but Jira/project-specific management is not to be built into BPR.
Importable/exportable configuration should make the user's custom agents and PR
prompts easy to share with colleagues.

## Local author flow

The user's typical starting point is running the create-PR skill from a worktree
folder. They asked for the first Home action and what happens after selecting a
folder. The discussion proposed Prepare pull request… → select worktree → inspect
the proposed diff → work among evidence/description/tour → Create draft PR….

The latest confirmation was:

> Yeah, local PR draft makes sense. Fold that flow into the RFC pack and resume
> creating it. Keep agent and prompt import/export as the first slice, and don't
> import the pack yet. Maybe review cycles is different enough to leave alone,
> but I think it loses its place of prominence on the home page in favor of this.

The term and direction are confirmed. Keeping existing Review Cycles intact is
the conservative proposal respecting the user's tentative distinction; the pack
does not interpret that statement as approval to remove its data or feature.

## Authoring authorization

The user invoked author-rfc and requested a portable RFC pack. Creation was paused
for the local-draft discussion and explicitly resumed. The current mandate is
draft authoring only, with agent/prompt sharing first and local preparation,
evidence, tours and optional extensions afterward. A secondary reviewer is to
inspect the pack before import. This session was not asked to dispatch that
reviewer. No implementation, live tracker import or publication is authorized.
