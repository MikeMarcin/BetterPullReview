# Insights window pack

An unadopted portable proposal for a BetterPullReview Insights window. It's
modelled on AllTheThings' Insights window. It shows what BetterPullReview
costs (the GitHub budget, storage, work and idle energy) and what you've
reviewed (comments, notes, files accepted and seen, reviews opened and
submitted).

It was drafted on 2026-10-17 against `e9b1cee35`, from an owner chat session.

- [RFC-DRAFT-0024](RFC-DRAFT-0024-insights-window.md): the authoritative
  requirements and acceptance (AC01 to AC12).
- [Design record](DECISIONS.md): the owner's answers (including Q1 to Q4),
  readiness, checks and confirmation.
- [Proposed cards](issues/): DRAFT-0001 to DRAFT-0009, and closeout
  DRAFT-0010.
- [Canonical plan](milestone-plan.json) and its [generated view](MILESTONES.md).
  `milestone-plan.schema.json` is copied from the implement-rfc kit.
- [Implementation entrypoint](IMPLEMENTATION_PROMPT.md).

Six milestones, in order: window and storage; GitHub budget, which
introduces the shared charts; then review stats, energy and work, which
don't depend on one another; then closeout.

No live IDs, baseline or implementation evidence exist yet, and execution
eligibility hasn't been evaluated. Every design question is answered.
