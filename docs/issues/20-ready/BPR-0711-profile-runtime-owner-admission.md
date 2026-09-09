---
schema: "implement-rfc.card.v1"
id: "BPR-0711"
title: "Admit one native runtime owner per profile"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0710"]
metadata: {"dispatch":"UX-2026-09-08-D3","mandate_revision":1}
---

# Admit one native runtime owner per profile

## Scope and acceptance

Bounded AC-06/09 safety foundation: retain an exclusive native lease before
profile domain stores or services start. Same-root competing runtime startup
must fail explicitly; distinct profile roots remain independently admissible.
Keep the lease until profile consumers and effects are drained. Process death
releases native ownership without PID-age or timeout-based lock stealing.

Desktop Open Profile must acquire admission before changing that profile's
durable window generations. Existing owned-profile New Window continues through
the management host. Rejected admission preserves the existing owner's durable
windows and services. Root/symlink/permission failure remains fail-closed.

Cross-process activation and explicit new-window transport remain a required
subsequent slice, not an accepted claim of this foundation. This card does not
close AC-06 or infer native package evidence from fixtures.

## Ownership and verification

Owned paths: application admission contracts, native storage-root lease adapter,
profile startup/desktop integration, focused native and composed process tests,
owning architecture/startup documentation and this card/closeout.

Verify actual competing processes, same-process duplicate admission, independent
roots, release/crash recovery, unsafe paths, cancellation and desktop contention
before window mutation. Run focused startup/management/lifecycle integration,
changed-file format/tidy and documentation gates. Independent critical review
targets ownership lifetime and admission ordering.

## Evidence

Planned; no implementation or acceptance yet.
