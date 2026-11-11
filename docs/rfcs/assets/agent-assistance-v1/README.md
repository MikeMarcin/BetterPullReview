# Agent assistance: proposed pack

Six places where an agent can do reading the reviewer or author now does by
hand. They are the rest of the list the owner asked for when they asked where
else the agent could add value; self-review, from the same list, is already
RFC-BPR-0115. Nothing here has been implemented.

The owner's words and the list they refer to are in
[SOURCE_NOTES.md](SOURCE_NOTES.md). [DECISIONS.md](DECISIONS.md) is the
single record of what the owner said, what is only proposed, the deferrals,
the checks run on this draft and the unevaluated execution eligibility.
[AMENDMENTS.md](AMENDMENTS.md) lists what this changes in adopted RFCs.

## The RFCs

| RFC | Outcome |
|---|---|
| [RFC-DRAFT-0001](RFC-DRAFT-0001-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md) | A pull request that returns with new commits offers What changed since my review, and a thread offers Was this addressed? |
| [RFC-DRAFT-0002](RFC-DRAFT-0002-explain-this-file-where-i-am-reading.md) | A file in Review offers Explain this file, answered where the reviewer is reading |
| [RFC-DRAFT-0003](RFC-DRAFT-0003-turn-a-draft-comment-into-an-exact-suggested-change.md) | A draft comment becomes an exact suggested change that core has checked against the lines |
| [RFC-DRAFT-0004](RFC-DRAFT-0004-ask-the-agent-why-a-check-failed.md) | A failed check offers Ask why it failed |
| [RFC-DRAFT-0005](RFC-DRAFT-0005-the-agent-runs-a-planned-check-on-a-local-pr-draft.md) | On a local draft the agent runs a planned check and its result is recorded as reported evidence |
| [RFC-DRAFT-0006](RFC-DRAFT-0006-insights-shows-what-the-agents-did-and-what-was-kept.md) | Insights shows runs, reported spend and kept suggestions per agent feature |

Three rules hold across all six. Nothing starts a run but a press: no run on
open or refresh. An agent's answer is shown as its claim and changes no
state by itself: no thread is resolved, nothing is verified and nothing
reaches GitHub. And what the agent produces lands as something the
application already has: a note, a draft comment, a suggested change or an
evidence result.

## Cards and plan

Thirteen proposed cards are in [cards/](cards/). The canonical grouping is
[milestone-plan.json](milestone-plan.json), with its
[schema](milestone-plan.schema.json); [MILESTONES.md](MILESTONES.md) is
generated from it. The six milestones are independent of one another.
`agent-run-checks` waits on local-draft work that is not built yet; the
other five build on shipped code.

Start from the [implementation entrypoint](IMPLEMENTATION_PROMPT.md).
