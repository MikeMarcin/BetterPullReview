---
id: "RFC-DRAFT-0004"
title: "Ask the agent why a check failed"
status: "Proposed"
metadata: {"inspected_revision":"90785ae34aeccf63724b9e5e7a70446f89341ec5","milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/assets/agent-assistance-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
depends_on: ["RFC-BPR-0011", "RFC-BPR-0068", "RFC-BPR-0101", "RFC-BPR-0102"]
---

# Ask the agent why a check failed

## Problem and grounded current behavior

[RFC-BPR-0068](../../RFC-BPR-0068-inspect-commit-checks.md) made a failed
check findable: Overview lists each check with its name, producer,
conclusion and "a readable bounded provider description or output summary
where supplied", and an **Open check details** action. It stops there on
purpose: "Downloading logs, embedding external websites, rerunning jobs and
executing repository code are outside this pack." So the question a reviewer
has next, whether this change caused the failure, is answered in a browser.

An agent can answer it without BetterPullReview fetching anything. Agents
are launched with "their own credentials and GitHub access", and BPR
"passes no provider credential or its own GitHub credential"
([RFC-BPR-0011](../../RFC-BPR-0011-ai-agent-profiles-context-supplements.md)).
The owner asked for it ([notes 2 and 3](SOURCE_NOTES.md)).

A question has four possible subjects today: the pull request, a file,
lines or a conversation
(`src/core/include/bpr/core/agent_question.hpp`, on the v0.5.0 prerelease lane).
A check is none of them.

## Proposed behavior and boundaries

A check whose outcome is a failure, a timeout, a cancellation or action
required offers **Ask why it failed**, an agent control, on its row.

- A press opens the Agent tab with a chip naming the check and the commit
  it ran on, as asking about a conversation puts a chip there
  ([RFC-BPR-0094](../../RFC-BPR-0094-ask-the-agent-about-a-conversation.md)).
  Nothing is sent. The suggested prompts are about the check: "Why did this
  fail?" and "Did this change cause it?".
- A question asked with the chip has the check as its subject, a fifth
  subject kind. The brief carries what BPR already holds: the check's name,
  producer, conclusion, the provider's summary text, its details address
  and the commit. Provider text is untrusted content and is passed as quoted
  data, never as instructions.
- BPR downloads no log and passes no credential. Whether the agent reads
  the log is a matter of its own GitHub access. An agent without that access
  answers from the summary and says what it could not read; that is an
  answer, not an error.
- The answer is asked to say what failed, whether the pull request's change
  is the likely cause or the failure looks unrelated, flaky or due to a
  stale base, and what it could not determine. It is shown as the agent's
  claim.
- Nothing reruns a job and nothing is written to GitHub.
- When the pull request's head has moved past the commit the check ran on,
  the chip says so; a check that is no longer in the current result set
  removes the chip.
- It is briefed by a built-in prompt of its own, `review.check-failure`
  (a proposal; P02 in the [decision record](DECISIONS.md#decisions)).

RFC-BPR-0068's boundary is unchanged: BPR itself still downloads no log,
embeds no website, reruns nothing and executes no repository code.

## Acceptance

- **AC01:** Ask why it failed is on failed, timed-out, cancelled and action-required check rows and on no other row; a press opens the Agent tab with a chip naming the check and its commit and sends nothing.
- **AC02:** A question asked with the chip has the check as its subject, and its brief carries the check's name, producer, conclusion, summary and details address, with the provider's text marked as quoted data.
- **AC03:** BPR requests no log and passes no credential for the question, and no rerun or GitHub write exists on this path.
- **AC04:** An agent that cannot read the log produces an answer saying so, shown as a claim, and not an error state.
- **AC05:** The chip says when the head has moved past the check's commit, and a check gone from the current result set removes it.
- **AC06:** The operation is briefed by its own built-in prompt, whose edit reaches the next check question and no other prompt.
- **AC07:** On a frozen build serving a pull request with a failed check, the owner accepts the control, the chip and an answer.

## Proposed delivery

[Ask why it failed](cards/DRAFT-0008-ask-why-a-check-failed.md) and
[closeout](cards/DRAFT-0009-close-ask-why-a-check-failed.md) form the fourth
[milestone](milestone-plan.json).
