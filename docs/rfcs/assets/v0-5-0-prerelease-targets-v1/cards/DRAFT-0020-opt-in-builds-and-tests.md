---
schema: "implement-rfc.card.v1"
id: "DRAFT-0020"
title: "Let an agent build and run tests in its disposable checkout when asked to"
status: "Backlog"
rfcs: ["RFC-DRAFT-0007"]
depends_on: ["DRAFT-0019"]
---

# Let an agent build and run tests in its disposable checkout when asked to

## Scope and acceptance

Implements
[RFC-DRAFT-0007](../RFC-DRAFT-0007-agent-scratch-folder-and-opt-in-builds.md).

Add the per-agent Build and run tests setting, available only for the
disposable checkout, with the once-per-pull-request question, and pass the
build allowance only after both.

Covers AC04 and the review in AC07. The owner settled on 2026-11-10 that
builds belong in this release, with these limits. Building runs the pull
request's own code: give the allowance critical review.

This is part of `agent-scratch` in the [plan](../milestone-plan.json).
