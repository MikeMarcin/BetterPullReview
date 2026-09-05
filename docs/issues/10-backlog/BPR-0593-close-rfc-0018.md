---
schema: "implement-rfc.card.v1"
id: "BPR-0593"
title: "Verify and close RFC-BPR-0018"
status: "Backlog"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0018"]
---

# Verify and close RFC-BPR-0018

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0018](../../rfcs/RFC-BPR-0018-robust-settings-center.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 1 of 4.
Planned cards: BPR-0592, BPR-0593. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

**Design accepted; registration only; implementation requires a later request.**
This unstarted backlog card records the 2026-09-04 registration, not an execution
grant or completed product work. The immutable base is the pre-registration
commit for this approved workstream; registration commits after it are planning
history, not evidence of interrupted product implementation.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: The shipped application exposes a native Settings action and one singleton, modeless window per available BPR profile through platform-appropriate menus and shortcuts, with the edited profile visibly identified. | Menu/shortcut composition and profile identity course | Not run; implementation deferred | None |
| AC-02: All seven initial categories are present when their capabilities are composed, with stable sidebar/page/row identities and truthful unavailable states. | Category inventory and unavailable-capability projection course | Not run; implementation deferred | None |
| AC-03: Appearance is migrated from the embedded workbench surface to the settings center with no duplicate authority; existing mode, typography, palette, import/preview/commit/cancel/reset, and persistence behavior remains intact. | Existing appearance courses through Settings and composed diff | Not run; implementation deferred | None |
| AC-04: Every setting gesture becomes one typed domain intent, reaches only its owning host, and updates the control only from an accepted authoritative projection. | Typed domain intent/acceptance and stale-completion host tests | Not run; implementation deferred | None |
| AC-05: Existing domain stores migrate and reopen from prior schemas; future, malformed, interrupted, and unavailable cases fail at their exact scope without corrupting another domain. | Prior-schema migration, restart, interruption and unavailable-store tests | Not run; implementation deferred | None |
| AC-06: Credential, privacy, cache, notification, and agent actions retain their existing capability and secrecy boundaries. | Recording vault/provider/agent/notification capability and secrecy checks | Not run; implementation deferred | None |
| AC-07: Keyboard-only and screen-reader courses reach every sidebar item, row control, reset, confirmation, status, and close action in a natural order. | Keyboard, screen-reader and focus-return application course | Not run; implementation deferred | None |
| AC-08: Layout and rendered evidence pass at 100%, 125%, 150%, and 200% scale in Light, Dark, High-contrast Light, and High-contrast Dark, including long text and minimum window size. | Rendered application at specified appearances/scales and long-text/minimum-size cases | Not run; implementation deferred | None |
| AC-09: One-setting updates preserve sidebar selection, page scroll position, focus, unaffected persistent indexes, and all unrelated projections without routine resets. | Incremental Qt model/focus/scroll tests for one-setting changes | Not run; implementation deferred | None |
| AC-10: Hostile catalog, profile, rule, and row floods stop at declared limits with typed refusal; one window materializes at most one page per category; rapid previews coalesce; cancelled or superseded loads publish nothing late; and instrumentation observes zero database, vault, filesystem, network, process, or blocking-wait work on the Qt thread. | Bounded input, cancellation, coalescing and Qt-effect instrumentation checks | Not run; implementation deferred | None |
| AC-11: Tests-disabled packaged applications on macOS, Windows, and Linux expose the same setting inventory except for explicitly documented platform capability rows. | Target-native tests-disabled setting inventory on macOS, Windows and Linux | Not run; implementation deferred | None |
| AC-12: Sign in starts supported device authorization directly and shows its code and browser action without Details, another Get code gesture, or pane scrolling; cancellation, expiry, and supported manual-token alternatives remain reachable. | Public Sign in gesture through recording device/browser ports; cancel/expiry/manual alternatives | Not run; implementation deferred | None |
| AC-13: User-configured servers and account selections survive restart through typed host persistence, while remembered grants reuse the existing vault path and session-only manual credentials are never presented as durable. | Add/edit fixture server through typed host/store and restart; credential lifetime checks | Not run; implementation deferred | None |
| AC-14: Configuration leaves the primary review surface compact, preserves tuned diff/appearance behavior, and exposes direct reconnect plus an exact pending destination handoff without changing credential or provider authority. | Composed compact review, appearance regression and exact pending-destination handoff | Not run; implementation deferred | None |

## Completion rules

- Replace planned checks with exact executed commands/courses, results, and
  producing commits as implementation completes; retain not-run rows honestly.
- Inspect all cards linked to this RFC, not just the initial planned pair.
  Every required card must be done and every canonical criterion satisfied.
- Exercise the selected public path and check critical changed-surface findings.
  Use focused evidence; do not infer a full matrix requirement from this card.
- Record target-native or attended evidence in the project's existing workflow.
  Park pending attended work and continue independent work; no approval or live
  GitHub authority comes from registration.
- Run the documentation structure gate and `git diff --check`, reconcile RFC
  and index status, and close only when canonical order and all dependencies
  permit Auto-Verified. No status changes are authorized by this registration.

## Resume

Next task: [BPR-0592](BPR-0592-inventory-settings-authority-and-plan.md) after a later explicit implementation
request and the applicable RFC eligibility/dependency checks. No product check
has run, no product slice is complete, and no temporary worktree is retained.
