---
schema: "implement-rfc.card.v1"
id: "BPR-0712"
title: "Route profile activation and new-window launches to the runtime owner"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0711"]
metadata: {"dispatch":"UX-2026-09-08-D4","mandate_revision":1}
---

# Route profile activation and new-window launches to the runtime owner

## Scope and acceptance

Bounded RFC-0024 AC-06/09 continuation: a stable profile identity and only
activate/new-window intents cross an owner-private native activation boundary.
Ordinary repeated launch activates the existing MRU window; explicit New Window
requests another window through the owner's durable management admission.
The sender never changes window registry state or bypasses the runtime lease.
Bound framing, peer authority, pending work and timeout/owner-loss outcomes.
Refuse closing owners; no automatic retry of an indeterminate New Window.

## Owned paths and verification

Application activation contracts, native local transport adapter, desktop/startup
and launch parsing, focused native and real-process tests, startup documentation
and this card/closeout. Preserve RFC-0028 Home/layout and unscheduled file tabs.
Verify real independent processes, exact profile/intent routing, malformed and
unavailable requests, existing owner lifetime, and no competing registry writer.
Run focused composed lifecycle regression, format/tidy and documentation gates.
Independent critical review targets framing, ownership and shutdown/ack races.
Native Windows/package focus evidence remains open unless directly observed.
