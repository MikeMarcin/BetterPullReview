---
schema: "implement-rfc.card.v1"
id: "BPR-0099"
title: "Simplify the RFC-BPR-0005 change set"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0098"]
---

# Simplify the RFC-BPR-0005 change set

Run a conservative simplification pass over the RFC-BPR-0005 change set, preserving behaviour and public semantics. Remove duplicated policy, broad service access, unnecessary serialisation, and whole-state refreshes, then replay the affected focused and boundary verification.

Record an explicit no-change result if the pass finds nothing safe to improve.

## The open composition question, decided

BPR-0097 raised but deliberately did not settle whether composing
RFC-BPR-0003's epoch control should expose its `Local history maintenance`
menu — prune selected epochs and delete repository cache — in the workbench
shell. This card owns that decision.

**The menu stays.** Removing it would require one of two worse outcomes.
Forking the control would create exactly the second epoch authority the RFC
forbids and that composing it was chosen to avoid. Adding a hide flag would
widen RFC-BPR-0003's control API from inside RFC-BPR-0005, letting a later RFC
edit an earlier RFC's verified surface to suit its own layout.

The capability is also already bounded where it matters. It is one secondary,
keyboard-focusable menu rather than three prominent buttons, a deliberate
RFC-BPR-0003 choice. Each destructive command captures its exact target before
a window-modal confirmation whose default and Escape action are Cancel, states
its consequence in text, and is revalidated by the core and host afterwards.
BPR-0067 proved the end-to-end path through the selected executable and
BPR-0070 disabled prune for a selection that could only fail. Nothing in this
composition weakens any of that.

What remains genuinely open is not authorization but prominence: whether a
destructive menu sits comfortably in the assembled workbench at supported
sizes. That is a visual judgement, which is precisely the owner-attended
BPR-0100 gate, and it is recorded there rather than pre-empted here.

## Simplification result: no code change

The card's four named targets were each checked against the change set.

**Duplicated policy.** One instance exists and is already tracked. BPR-0092
could not express change-navigation enablement as a core projection because
`core::workbench_command_id` names only file and drawer commands, so it carries
a Qt-local projection type. BPR-0102 owns making that symmetric with file
navigation. Removing it here would mean editing `src/core` under a
simplification card, which is a design change wearing a tidy-up costume.
Otherwise the Qt layer decides nothing: enablement flows through a single
helper fed by core projections, and there is exactly one `setEnabled` call in
the window.

**Broad service access.** Absent. Neither Qt file references `QProcess`,
`QNetworkAccessManager`, `QSqlDatabase`, or `QFile`. No widget performs an
effect.

**Unnecessary serialisation.** Absent. Every boundary in the slice is a typed
in-process call; no JSON or RPC was introduced.

**Whole-state refreshes.** Absent. The drawer model contains no
`beginResetModel`, and the host publishes only the projection domains whose
published meaning changed.

One cosmetic opportunity was considered and declined. The
`setObjectName`/`setAccessibleName`/`setAccessibleDescription` triple recurs,
but only four sites share an identical shape, so extracting a helper would
remove roughly eight lines while requiring a mechanical transform across a
970-line file whose guards are largely structural. Repetitive widget
construction is not one of the four smells this pass exists to remove, and the
risk-to-benefit ratio is wrong. It is recorded here rather than done silently
or left unmentioned.

## Direct verification

The change set was replayed after the review to confirm the no-change result
rests on a green tree rather than an assumption: the full Qt suite passes
24/24, `core_workbench_interaction` passes in the Qt-free preset, and the
build is clean.

## Result

The RFC-BPR-0005 change set needs no behaviour-preserving simplification. Its
one policy duplication is tracked in BPR-0102 as a design correction rather
than absorbed here, the other three smells are absent by construction and
tested, and the composed maintenance menu is kept with its reasoning recorded
and its remaining question routed to the owner-attended gate. BPR-0099 is
complete.
