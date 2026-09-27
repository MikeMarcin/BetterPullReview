# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **rfc-bpr-0027-advanced** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## structured-events

The shipped logger writes bounded versioned JSONL with stable event/category, severity, UTC and monotonic time, opaque session and sequence, build identity and existing operation correlation. Existing metadata export reads the new records and legacy records without copying free text.

**Work:** `BPR-1215`

**Prerequisites:** None

**Completion:**

- AC1: Persist parseable bounded structured records with explicit event identity, times, session, sequence, build and existing typed facts.
- AC2: Preserve BPR-0971 sanitizer and fail-closed malformed input, loss and file boundaries.
- AC3: Exercise provider-to-file public composition and existing diagnostic export; update owning docs.
- Commit verified scope, update durable evidence and land this accepted milestone on main.

**Boundary:** The current milestone is accepted and landed; a fresh implementer owns the next milestone. Required native or human evidence stays pending until supplied.

## session-retention

Replace the fixed-file ceiling with safely owned per-session files and coordinated rotation, aggregate and age retention. Preserve lifecycle and explicit failures.

**Work:** `BPR-1216`

**Prerequisites:** structured-events

**Completion:**

- AC1: Enforce file, aggregate, age and session bounds with safe ownership and concurrent-session coordination.
- AC2: Test disk/rotation/partial-tail failures and startup/cancel/shutdown availability through public composition.
- Commit verified scope, update durable evidence and land this accepted milestone on main.

**Boundary:** The current milestone is accepted and landed; a fresh implementer owns the next milestone. Required native or human evidence stays pending until supplied.

## diagnostic-controls

Signed-out profile Settings own Standard/Verbose, independent identity privacy, Off, recent context, Clear and Open logs; transitions and sign-out clear transient state under acknowledged barriers.

**Work:** `BPR-1217`

**Prerequisites:** session-retention

**Completion:**

- AC1: Reach and persist controls safely across restart and invalid preferences through the shipped UI.
- AC2: Enforce detail/privacy combinations, typed identity labels, no forbidden material, bounded priority/loss and ring behavior.
- AC3: Verify mode/Off/Clear/privacy/sign-out barriers and profile/window scope.
- Commit verified scope, update durable evidence and land this accepted milestone on main.

**Boundary:** The current milestone is accepted and landed; a fresh implementer owns the next milestone. Required native or human evidence stays pending until supplied.

## diagnostic-export

Export exact previewed scopes with fresh anonymized joins or explicit identifying detail, preserving useful sanitized diagnostics and originals, refusing or counting malformed records without network effects.

**Work:** `BPR-1218`

**Prerequisites:** diagnostic-controls

**Completion:**

- AC1: Freeze and reprocess every selected field, preserve joins, disclose capture-time omissions and keep originals intact.
- AC2: Verify malformed/mixed-mode/session/ring selection, local-only writes and negative content cases through public UI.
- Commit verified scope, update durable evidence and land this accepted milestone on main.

**Boundary:** The current milestone is accepted and landed; a fresh implementer owns the next milestone. Required native or human evidence stays pending until supplied.

## native-closeout

Reconcile all RFC-BPR-0027 criteria and platform, dependency, packaging, native and attended obligations against exact candidates; preserve gaps until evidence exists.

**Work:** `BPR-1219`, `BPR-0603`, `BPR-0601`, `RFC-BPR-0027`

**Prerequisites:** diagnostic-export

**Completion:**

- AC1: Run focused integration and native logging/lifecycle/control courses on each claimed platform; bind dependency and package provenance.
- AC2: Prepare required attended review with exact candidates and pending decisions; no live sign-in without separate authority.
- AC3: Reconcile BPR-0603 and BPR-0601, RFC/index and all full criteria, land accepted scope on main.
- Commit verified scope, update durable evidence and land this accepted milestone on main.

**Boundary:** The current milestone is accepted and landed; a fresh implementer owns the next milestone. Required native or human evidence stays pending until supplied.
