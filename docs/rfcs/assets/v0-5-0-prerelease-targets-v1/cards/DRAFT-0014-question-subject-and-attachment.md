---
schema: "implement-rfc.card.v1"
id: "DRAFT-0014"
title: "Give each agent question a subject and replace the scope list with the attachment"
status: "Backlog"
rfcs: ["RFC-DRAFT-0005"]
depends_on: []
---

# Give each agent question a subject and replace the scope list with the attachment

## Scope and acceptance

Implements
[RFC-DRAFT-0005](../RFC-DRAFT-0005-agent-questions-say-what-they-point-at.md).

Have the core decide a request's subject; send where the reviewer is looking
separately from it; remove the scope list and show selected lines as a
detachable chip; make suggestions carry their subject and entry point so each
one sends from any mode; send a typed question with no selection; clear the
diff's selection on Escape under the RFC's precedence.

Covers AC01 to AC07. AC03 needs an installed agent on a fixture pull request
of at least five changed files. Load frontend-design before the Qt work.

This is part of `agent-tab-asks` in the [plan](../milestone-plan.json).
