---
provisional_id: PACK-ISSUE-BPR-0005
rfc: RFC-BPR-0005
status: Backlog
live_id: allocate-on-import
milestone: M1 Native Read-Only Workbench
depends_on_rfc: [RFC-BPR-0004]
---

# Issue Plan for RFC-BPR-0005

## Goal

Implement and Auto-Verify **Native Workbench Shell and Virtualized Review Surface** from the live repository after pack import.

## RFC prerequisites

- RFC-BPR-0004

## Minimum live issue-card slices

1. **Pack import and planning** — reconcile paths/IDs, import the normative architecture, create the live task set, and commit the planning slice.
2. **Architecture allocation** — identify authoritative state, compact core state, bounded observations, intents/events/requests/deltas/diagnostics, host ports/effects, Qt projections, async identity, and evidence-backed no-impact regions.
3. **Deterministic functional core** — implement semantic state, policy, transitions, planners, request identity, deltas, diagnostics, and direct tests without `QApplication`, Qt, I/O, clocks, threads, or provider libraries.
4. **Imperative application host** — implement capability ports/adapters, authority sessions, caches, jobs, persistence, request execution, cancellation, lifecycle, and fake-port/manual-executor tests without duplicating core policy.
5. **Qt presentation adapter** — implement or update event translation, `QAction` binding, Qt models/delegates/widgets, focus/layout/accessibility, and incremental projection application; record direct no-impact evidence when the RFC has no presentation effect.
6. **Composition and public-path integration** — wire the concrete pieces in the composition root and prove the feature is selected and reachable through the real BetterPullReview application.
7. **Focused tests and fixtures** — add core replay/property tests, host failure/reordering/cancellation tests, Qt adapter tests, contract tests, and hostile/edge fixtures.
8. **UX/UI verification** — mandatory when rendering or interaction changes; use deterministic automation and actual rendered evidence rather than a static mockup.
9. **Security and boundary verification** — verify authority, credentials, target identity, process/network boundaries, limits, secret-free evidence, and absence of provider/widget bypasses.
10. **Agent and product documentation impact** — update architecture, state authority, ports, threading, UI, security, build, and operator documentation or record an approved no-impact result.
11. **Simplification** — run the available code simplifier or a conservative manual pass; remove duplicated policy, broad service access, unnecessary serialization, and whole-state refreshes, then re-verify.
12. **Verification and RFC closeout** — complete the live claim and architecture-conformance matrices, move all cards to done, update indexes, and mark Auto-Verified only when direct evidence agrees.

## Suggested RFC-specific implementation slices

1. Implement core navigation/selection transitions and command projections for file, change, epoch, drawer-semantic, and review actions.
2. Implement host observation/projection assembly, settings persistence requests, changed-domain publication, and cancellation/generation handling.
3. Implement the native main window and top primary toolbar as Qt event/action adapters rather than controllers.
4. Implement collapsible/resizable file and conversation drawers with stable-identity incremental model/view adapters.
5. Implement virtualized old/new diff panes, synchronized scrolling/selection, and the clean center grouping band from RFC-0004 without presentation-side regrouping.
6. Implement the full-width lower line-over-line detail pane.
7. Implement independent epoch selectors, keyboard/focus/accessibility, loading/error states, and UI automation proving intent translation and incremental deltas.

## Required planning artifacts

- Live issue dependencies and board lanes
- Per-acceptance-criterion verification rows
- Affected documentation ledger entries
- Safe GitHub fixture or designated test-target plan where mutations apply
- Platform/build matrix for affected code
- Architecture allocation ledger covering core, host, Qt, composition, authority, and no-impact regions
- Typed intent/event/observation/request/delta path and asynchronous identity/generation plan
- Incremental-projection and active-set/changed-set performance plan
- Explicit deferred work; no hidden scope reduction

## Closeout blocker examples

- Qt, provider, filesystem, process, clock, thread, or persistence dependencies enter the functional core.
- A widget/model performs GitHub, Git, SQLite, AI, or other host effects directly.
- The application host duplicates semantic policy owned by the core or a giant service locator bypasses capability ports.
- Asynchronous work lacks request/session/revision/generation identity, or a worker touches Qt/core state directly.
- Routine bounded changes require a full model reset, whole-PR scan, or complete application projection rebuild.
- A feature exists only in a helper or mock and is not selected by the application.
- A screenshot or static mockup substitutes for deterministic UI behavior.
- GHES/GitHub.com capability selection is assumed from a target name.
- A test can pass while displayed line identity, review target, acting account, or credential boundary is wrong.
- Required documentation, simplification, package, security, or live issue-board work remains open.
