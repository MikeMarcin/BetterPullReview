---
schema: "implement-rfc.card.v1"
id: "BPR-0593"
title: "Verify and close RFC-BPR-0018"
status: "Backlog"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592","BPR-0678","BPR-0679","BPR-0680","BPR-0681","BPR-0682","BPR-0683","BPR-0684","BPR-0685","BPR-0686","BPR-0687","BPR-0688","BPR-0689"]
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

The 2026-09-04 registration was subsequently activated by the owner's
2026-09-07 implementation direction in the canonical index. This closeout stays
in Backlog until its required acceptance is complete. The immutable base remains
the pre-registration commit; registration commits are planning history.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: The shipped application exposes a native Settings action and one singleton, modeless window per available BPR profile through platform-appropriate menus and shortcuts, with the edited profile visibly identified. | `qt_settings_window` menu, singleton, Default label and close course | Automated Default environment passes; target-native inventory pending | 68e0a40f, a881d9f9 |
| AC-02: All seven initial categories are present when their capabilities are composed, with stable sidebar/page/row identities and truthful unavailable states. | Seven-page composed matrix and agent/storage offered-model checks | Automated pass; unsupported capabilities remain explained | 51c24f6b, 6c0ba02b |
| AC-03: Appearance is migrated from the embedded workbench surface to the settings center with no duplicate authority; existing mode, typography, palette, import/preview/commit/cancel/reset, and persistence behavior remains intact. | Appearance host/store/Qt and application theme customization course | Pass, including export, scoped reset and actual Settings close | 03a8a02e, a881d9f9 |
| AC-04: Every setting gesture becomes one typed domain intent, reaches only its owning host, and updates the control only from an accepted authoritative projection. | Domain host refusal/late-result checks; queue field merge and composed intent checks | Pass on changed settings paths; medium review found no additional state defect | dd64c098, 51c24f6b, a881d9f9 |
| AC-05: Existing domain stores migrate and reopen from prior schemas; future, malformed, interrupted, and unavailable cases fail at their exact scope without corrupting another domain. | SQLite appearance v1/v2 and queue v5 migrations/reopen; malformed/future checks | Canonical migrations and pre-write malformed/future refusal pass | 03a8a02e, dd64c098, 22e30024 |
| AC-06: Credential, privacy, cache, notification, and agent actions retain their existing capability and secrecy boundaries. | Device flow, metadata-only notification preview, offered private opt-ins and exact maintenance course | Pass on fixture public paths; no live credentials/provider mutations | 01a3ada4, dd64c098, 51c24f6b |
| AC-07: Keyboard-only and screen-reader courses reach every sidebar item, row control, reset, confirmation, status, and close action in a natural order. | Qt keyboard, focus-return, page scroll, reset and Cancel courses | Automated checks pass; native screen-reader course pending | 68e0a40f, a881d9f9 |
| AC-08: Layout and rendered evidence pass at 100%, 125%, 150%, and 200% scale in Light, Dark, High-contrast Light, and High-contrast Dark, including long text and minimum window size. | `qt_settings_window` plus scale 125/150/200, each four appearances and seven pages; 112 captures | Automated layout pass at 760x480 with long metadata; attended visual verdict pending | 6c0ba02b |
| AC-09: One-setting updates preserve sidebar selection, page scroll position, focus, unaffected persistent indexes, and all unrelated projections without routine resets. | Stable page/scroll/focus and incremental workbench projections | Pass for one-setting changes and Settings reopen | 68e0a40f, dd64c098, a881d9f9 |
| AC-10: Hostile catalog, profile, rule, and row floods stop at declared limits with typed refusal; one window materializes at most one page per category; rapid previews coalesce; cancelled or superseded loads publish nothing late; and instrumentation observes zero database, vault, filesystem, network, process, or blocking-wait work on the Qt thread. | Appearance host cancellation/cache/coalescing, one page per category, architecture boundary checks | Collection limits, stable models, coalesced previews and cancellation pass; focused Settings capability-port instrumentation passes; exhaustive OS/native instrumentation remains unproven | 143abbc9, e354b45e, 29bb2980, a881d9f9, 6c0ba02b |
| AC-11: Tests-disabled packaged applications on macOS, Windows, and Linux expose the same setting inventory except for explicitly documented platform capability rows. | Tests-disabled target-native packaged inventory | Pending macOS/Windows/Linux current-candidate inventory; old isolated build is not this criterion | None |
| AC-12: Sign in starts supported device authorization directly and shows its code and browser action without Details, another Get code gesture, or pane scrolling; cancellation, expiry, and supported manual-token alternatives remain reachable. | Application device panel course from Home, GHES/absent refusal and Qt duplicate/cancel/expiry | Pass; supported flow starts directly, manual/session-only alternatives retained | 01a3ada4 |
| AC-13: User-configured servers and account selections survive restart through typed host persistence, while remembered grants reuse the existing vault path and session-only manual credentials are never presented as durable. | Saved server typed store/restart and existing device vault binding cases | Existing BPR-0660 and BPR-0679 evidence retained; manual tokens stay session-only | 01a3ada4 |
| AC-14: Configuration leaves the primary review surface compact, preserves tuned diff/appearance behavior, and exposes direct reconnect plus an exact pending destination handoff without changing credential or provider authority. | Compact workbench, composed GitHub, theme and direct reconnect paths | Pass: visible unknown-server Add/sign-in preserves exact target; cancel/stale stages preserve prior review, unavailable locations are explicit | 554564f8, 01a3ada4, a881d9f9 |

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

The owner activated the broader UX work. BPR-0678 through BPR-0682, BPR-0684
through BPR-0688 are implemented. BPR-0689 retains exact setup destinations
and stages authentication without disturbing an existing review. BPR-0683 is
parked for required native/attended evidence, with its automated passes retained.
Continue RFC-0024 planning after the automated Settings integration boundary,
while preserving pending target-native/human criteria and canonical closeout order.

## Resumed implementation, 2026-09-07

The owner activated the broader UX frontier. [The authority inventory](../../development/settings-authority-inventory.md)
reconciles the integrated 0028 work and maps every criterion to BPR-0678 through
BPR-0683. These join BPR-0592 and BPR-0593 as the planned cards. Existing
partial implementation is not full criterion acceptance; not-run rows above
are superseded by the criterion ledger as each selected path is exercised.

BPR-0678: default-profile shell now uses stable per-page scroll containers,
retains panel identity and category scroll, identifies its default profile and
handles unconsumed Escape. Composed Qt checks pass at 100/125/200%; actual
760x480 offscreen Accounts/Reviews/Appearance captures inspected. This is
partial AC-01/02/07/08/09/10 evidence, not a full platform/accessibility verdict.
Two affected attended candidates (chrome and widths) were released by the
owner's resume-development authorization and need preparation after integration;
public-preview was subsequently released for BPR-0679 and also needs preparation.

BPR-0679: Home and the signed-out account action now start the existing device
flow directly. The executable panel course exercises Home, and the GHES
negative retains its no-request refusal. Running-flow duplicate, cancellation,
expiry and selected-host retry checks pass. Saved server authority/store and
session-only manual-token evidence from BPR-0660 remain unchanged. The
isolated tests-disabled build passed; one digest assertion was updated for
the reviewed course change and its bypass test passed separately. See the
card for commands and revision. Next: BPR-0680 appearance export/reset.

BPR-0684 repairs the Settings-close preview/import lifecycle discovered during integration preparation.

## Current integration evidence

At `3d36ce70` plus bookkeeping, both architecture checks and
`qt_composed_github_scale_100` passed (116.96s total). The actual application and
composed Qt target built successfully. Reused unchanged evidence: full core-only
125/125 at BPR-0681, selected domain host/store checks, device public course,
queue restart/pause course, theme customization 39.16s and maintenance 66.12s.
BPR-0685 adds all four scales/appearances with zero horizontal page scrolling;
112 deterministic offscreen rasters were generated and representative corrected
surfaces inspected. This does not stand in for screen-reader or target-native
packaged-app evidence.

One independent review was initially dispatched with the stale native role's
Astra high setting. It was interrupted on the owner's correction, and the same
bounded review continued with explicit Astra medium routing. The finding was
preserved: appearance legacy migration must validate before writing (BPR-0686).
The medium review reported no additional material finding in queue preference
merges, scoped resets, exports or close/import cancellation. No second review
loop is required; the repair receives its direct regression checks.

BPR-0686 fixed the migration finding (22e30024); malformed and future legacy
stores are refused before mutation. BPR-0687 bounds queue collections and keeps
accepted models and removal controls usable after refusal (e354b45e). Host/store
checks and Qt 100/125/200 passed, including maximum admitted collections.
BPR-0688 coalesces a burst to the latest preview and invalidates queued work on
close/reset (29bb2980); the panel and actual theme course passed. Documentation
verifier regression suite: 62 tests passed during this integration.
