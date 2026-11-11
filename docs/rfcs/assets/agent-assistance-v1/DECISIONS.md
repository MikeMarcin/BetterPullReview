# Design record

## Scope and provenance

Pack `agent-assistance-v1`, first version, authored 2026-11-10 in the same
session as `local-draft-authoring-v1` and imported after it. The owner's
words and the list they refer to are in [SOURCE_NOTES.md](SOURCE_NOTES.md).
Grounded against lane `claude/v0-5-0-prerelease-targets-52265d` at
`90785ae34aeccf63724b9e5e7a70446f89341ec5`. No review of this pack has been
performed; none was requested.

## Decisions

Settled means the owner said it. Proposed means the author of this pack
chose it and the owner has not separately confirmed it.

| Decision | Requirement/acceptance | Status and actual answer | Rationale/consequences | Depends on / affected artifacts |
|---|---|---|---|---|
| D01: the other agent ideas from the first answer are wanted | All six RFCs | Settled. "The other agent ideas from the initial ask are part of our final suggestions." ([note 3](SOURCE_NOTES.md)) | The answer adopts a list the author wrote ([note 2](SOURCE_NOTES.md)). It says the ideas are wanted, not how each works. | All |
| P01: which ideas "the other agent ideas" are | The six RFCs | Proposed. Read as the seven remaining items of this session's first answer. A forked side session gave the owner ten more ideas the same day; those not also in this list are not in this pack. | See deferrals for the ones left out. | All |
| P02: one built-in prompt per new operation | Each RFC's prompt paragraph and criterion | Proposed. Follows the owner's request that self-review have its own prompt ([note 4](SOURCE_NOTES.md)). Against it: the owner merged the three Enhance prompts into one on 2026-11-10 (`90785ae34`). The six operations here are different tasks, not one task on three surfaces, which is why the proposal is one each. | Six new prompt purposes. If the owner prefers fewer, the six prompt criteria change and nothing else does. | DRAFT-0001, 0002, 0004, 0006, 0008, 0010; RFC-BPR-0107 |
| P03: an answer is a claim and changes no state | Returning-pull-request RFC AC04, AC06; check RFC AC03, AC04 | Proposed. Follows RFC-BPR-0009's and RFC-BPR-0012's non-goals. | No automatic Seen, resolve, verify or post. | DRAFT-0001, 0002, 0008 |
| P04: Explain this file answers in a card at the file header | Explain-file RFC AC02, AC03 | Proposed. The first answer said only "without a trip to the Agent tab". | A new card in Review; the turn still lives in the Agent tab's history. | DRAFT-0004 |
| P05: the checks core makes before an agent's replacement is shown as a suggestion | Suggested-change RFC AC02, AC03 | Proposed. | A reply that does not match the lines byte for byte is never shown as a suggestion. | DRAFT-0006 |
| P06: BPR downloads no check log; the agent uses its own GitHub access | Check RFC AC02 to AC04 | Proposed. Keeps RFC-BPR-0068's boundary and RFC-BPR-0011's rule that BPR passes no credential. | An agent without access answers from the summary. | DRAFT-0008 |
| P07: an agent-run check is a reported result, never observed | Agent-run-checks RFC AC02, AC04 | Proposed. Follows RFC-BPR-0109's own rule. | The observed class stays reserved. | DRAFT-0010 |
| P08: what the Agents section counts, and that it ranks nothing | Insights RFC AC01 to AC05 | Proposed. | Corrects the first answer, which said Insights "already tracks cost": it tracks what BetterPullReview costs the computer and the rate limit, and nothing about agents. | DRAFT-0012 |

## Open frontier and explicit deferrals

No question is waiting on the owner for import. P01 to P08 are proposals.
P02 is the one to ask first, because every RFC carries it and there is a
recent owner decision pointing the other way.

Deferred, with the boundary each leaves:

- **The side session's other ideas.** Reply drafts in Threads beyond the
  existing Draft a reply prompt, review ordering on Overview, a drafted
  review summary, a tone pass over all drafts, triage of returned feedback,
  a drafted pull request description, and a one-line reason per Home row.
  None is in this pack.
- **Explaining a selection in place.** Only a file; attached lines in the
  Agent tab cover a selection today.
- **Suggested changes** for a finding card that is not yet a draft, for
  several comments at once, or across files.
- **Check logs fetched by BPR**, and rerunning a job.
- **Observed executions** supervised by BPR, and running every planned
  check at once.
- **Agent spend** per pull request or repository, budgets and alerts.

## Checks and confirmation

1. **Structural validity.** `python3 <implement-rfc>/examples/milestone-plans/validate.py milestone-plan.json`
   answered "Structurally valid; references and semantic coverage
   unchecked." [MILESTONES.md](MILESTONES.md) is that validator's
   `--render` output.
2. **Design readiness.** All six milestones have defined behavior and
   acceptance. D01 is the owner's; P01 to P08 are proposals with no owner
   answer.
3. **Confirmation.** The owner's one statement adopting the list. No
   confirmation of any RFC's text was asked for or given. The pack was not
   reviewed or grilled.
4. **Execution eligibility.** Not evaluated by authoring. Every card is
   Backlog. The owner asked for import, not implementation.

## Continuation

Next action: implementation needs its own mandate, and P02 needs the
owner's answer first. Artifacts: the [overview](README.md), the six RFCs,
[cards/](cards/), the [proposed amendments](AMENDMENTS.md), the
[plan](milestone-plan.json) and the
[implementation entrypoint](IMPLEMENTATION_PROMPT.md).
