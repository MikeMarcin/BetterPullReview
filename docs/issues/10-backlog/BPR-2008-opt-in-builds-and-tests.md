---
schema: "implement-rfc.card.v1"
id: "BPR-2008"
title: "Let an agent build and run tests in its disposable checkout when asked to"
status: "Backlog"
rfcs: ["RFC-BPR-0104"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0020-opt-in-builds-and-tests.md"}
depends_on: ["BPR-2007"]
---

# Let an agent build and run tests in its disposable checkout when asked to

## Scope and acceptance

Implements
[RFC-BPR-0104](../../rfcs/RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md).

Add the per-agent Build and run tests setting, available only for the
disposable checkout, with the once-per-pull-request question, and pass the
build allowance only after both.

Covers AC04 and the review in AC07. The owner settled on 2026-11-10 that
builds belong in this release, with these limits. Building runs the pull
request's own code: give the allowance critical review.

This is part of `agent-scratch` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
