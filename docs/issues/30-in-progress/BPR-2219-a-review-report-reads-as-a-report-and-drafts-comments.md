---
schema: "implement-rfc.card.v1"
id: "BPR-2219"
title: "A review ask's report reads as a report and drafts comments from its findings"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2219--BPR-2219--f5f7c1180ea8.json","origin":"owner, 2026-11-16, about BPR-2166"}
---

# A review ask's report reads as a report and drafts comments from its findings

## Origin

The owner, 2026-11-16: "Doing a 'Review this pull request' ask creates a
report but that report is not acceptable. First it reads much more like an
agent transcript than an actual report. Second we lost the most useful
feature of the agent integration: it no longer gives the ability to draft a
comment from the agent response. That UX needs to return into the report.
You can reproduce this and maybe have a deep worker try to do the
/frontend-design:frontend-design for how it integrates into the report."

The owner's review had 12 files, +94/-236. The report is RFC-BPR-0128's
([[BPR-2166]]): a card where it was asked and a tab in the files' strip. The
earlier feature is the one keep action on an answer and on a finding card
(Draft comment, RFC-BPR-0127, `core::decide_keep`; RFC-BPR-0117 AC02 and
AC03 recorded as superseded by RFC-BPR-0128).

## Scope and acceptance

- **AC-a.** A Review this pull request (and Review {file}) answer from a real
  agent reads as a report: a verdict and summary first, then findings
  grouped by what they need, each at its place in the code. What the agent
  said while it worked (its progress, its tool use) is not in the report.
- **AC-b.** Each finding in the report has Draft comment, which writes a
  private draft comment at that finding's file and lines in the review; it
  then reads Go to draft. A finding with no place a draft can be anchored to
  offers Keep local. Nothing is published until the reviewer publishes.
- **AC-c.** The report's summary can be kept as a general comment draft.
- **AC-d.** The core reads the report's findings and decides each one's keep
  action; the host performs it; Qt presents it.
- **AC-e.** The Fixture agent answers a review with a realistic structured
  review, and a composed course asserts the report's structure and that
  Draft comment creates the anchored draft.

Load `frontend-design:frontend-design` before the design and Qt work and say
so in the report. The owner's look on a frozen build (Fixture agent only, a
fresh short session, one Accept/Reject a criterion) is filed, not waited on.

## Intended verification

A real-agent reproduction (Claude Code, Sonnet, medium effort, a mid-sized
pull request on MikeMarcin/BPR-Test) before and after; core tests of the
report model; the composed fixture-agent course above.
