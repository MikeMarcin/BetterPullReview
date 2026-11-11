---
id: "RFC-BPR-0121"
title: "Insights shows what the agents did and which suggestions were kept"
status: "Proposed"
metadata: {"origin":"imported 2026-11-10 from the agent-assistance-v1 pack, where it was RFC-DRAFT-0006","inspected_revision":"90785ae34aeccf63724b9e5e7a70446f89341ec5","milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"baseline":"a9663a706fd72061f60b615bd8e749ee0ddb66b5"}
depends_on: ["RFC-BPR-0086", "RFC-BPR-0101"]
---

# Insights shows what the agents did and which suggestions were kept

## Problem and grounded current behavior

Agent features cost money, and nothing in the application says which of
them earn it. The Agent tab shows what a turn and a session cost
([RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md)), and that
figure is gone when the session is.

[RFC-BPR-0086](RFC-BPR-0086-insights-window.md) is where the
application counts what it does and what the reviewer has done: requests,
energy, reviews submitted, comments posted, files accepted. It counts
nothing about agents: the RFC does not mention them. The cost in its title
is what BetterPullReview itself costs the computer and the GitHub rate
limit.

The application already knows what happened to an agent's suggestion. Core
decides when a finding is promoted into a draft and records one the reviewer
dismissed as `agent_finding_standing::dismissed`
([agent_finding_promotion.hpp](../../src/core/include/bpr/core/agent_finding_promotion.hpp)).
The owner asked for accepted against discarded suggestions per feature
([notes 2 and 3](assets/agent-assistance-v1/SOURCE_NOTES.md)).

## Proposed behavior and boundaries

Insights gains an **Agents** section, with the ranges Reviews already has:
This week, 30 days and All time.

For each agent feature it shows:

- **Runs:** started, and how each ended (answered, failed, stopped).
- **Spend:** the sum the agent reported for those runs. A run whose agent
  reported no cost counts as "Not reported", as RFC-BPR-0086 already writes
  for a counter it cannot read, and never as zero.
- **Suggestions,** for a feature that offers something to keep: offered,
  kept, dismissed, and neither. Kept means the reviewer took it into their
  own work: a finding drafted as a comment or kept as a note, an enhanced
  text accepted, a tour proposal accepted, a suggested change kept, a run's
  changed file kept, a check result accepted.

The rules are RFC-BPR-0086's:

- Each event is counted once, when it happens, and a count never goes down.
- Counting starts the first time a build with this section runs. Nothing is
  backfilled.
- The counts live in the per-profile Insights store with the review stats
  and follow their purge and reset rules.
- The anonymized export carries counts and sums only: no repository, pull
  request, path, prompt, question or agent output.

The features are a closed list that core owns. An agent feature added later
must name its row, and one that does not fails a test, as a new agent entry
point fails the build until it is counted.

The section states what it is: a record of what was kept, not of what was
right. It ranks nothing and recommends nothing.

Agent spend per pull request or per repository, budgets and alerts are not
in this RFC.

## Acceptance criteria

- **AC01:** For a fixture session, the Agents section's runs per feature and their endings equal the runs that session started and how each ended.
- **AC02:** Spend per feature equals the sum of the costs the agent reported for its runs, and a run with no reported cost shows as Not reported and adds nothing to the sum.
- **AC03:** Offered, kept, dismissed and neither are counted for finding cards, Enhance and tour proposals, each once and never decreasing, and keeping then removing a kept item does not subtract.
- **AC04:** The counts start with the first run of a build that has the section, follow the review stats' purge and reset rules, and the anonymized export holds counts and sums only.
- **AC05:** An agent feature with no Insights row fails a test.
- **AC06:** On a frozen build with seeded counts, the owner accepts the Agents section.

## Proposed delivery

[The Agents section](../issues/10-backlog/BPR-2084-insights-agents-section.md) and
[closeout](../issues/10-backlog/BPR-2085-close-rfc-0121.md) form the sixth
[milestone](plans/agent-assistance-v1/milestone-plan.json).
