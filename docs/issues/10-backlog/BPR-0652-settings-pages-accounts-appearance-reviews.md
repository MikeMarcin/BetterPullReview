---
schema: "implement-rfc.card.v1"
id: "BPR-0652"
title: "Settings pages: Accounts & hosts, Appearance, Reviews & queue"
status: "Backlog"
rfcs: ["RFC-BPR-0018","RFC-BPR-0028"]
depends_on: ["BPR-0646"]
priority: "P1"
---

# Settings pages: Accounts & hosts, Appearance, Reviews & queue

RFC-BPR-0028 Settings, from the Settings-and-drawers asset sections 1 to 3,
built on the Settings window BPR-0646 mounts. Accounts & hosts presents the
accepted session and server records through typed host projections and
intents (sign out, reconnect, add or remember an enterprise server); no
credential is shown. Appearance migrates the existing panel into the page
layout with theme previews rendered from the real palette (half-and-half
System), typography rows, per-role customisation with preview, and theme
import and export. Reviews & queue holds the modality rules, reading
behaviour, observer rules and saved repositories over the queue host's
preferences and enrollment ports. Every other page shows its notice.

## Acceptance

- RFC-BPR-0028 AC-10 and RFC-BPR-0018 AC-03 (migrated, not duplicated).
- Every control is host-validated and reverts on refusal; resets act on their
  own group only.
- Focused page tests, the appearance courses, and the boundary gate pass.
