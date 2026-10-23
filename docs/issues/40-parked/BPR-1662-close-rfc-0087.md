---
schema: "implement-rfc.card.v1"
id: "BPR-1662"
title: "Verify and close RFC-BPR-0087"
status: "Parked"
rfcs: ["RFC-BPR-0087"]
depends_on: ["BPR-1659", "BPR-1660", "BPR-1661"]
metadata: {"source": "Owner request 2026-10-19; taken over from the Codex branch on owner direction 2026-10-22", "milestone_plan": {"id": "shared-action-buttons-v1", "path": "docs/rfcs/plans/shared-action-buttons-v1/milestone-plan.json"}}
closeout_for: ["RFC-BPR-0087"]
parked_on: "human-review"
acceptance: [{"criterion":"AC01","outcome":"pending","revision":"9c23301ebbba3cc000afe6fb8a0c6f744ad0fbe7","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Technical part passes on the rebased candidate (focused suites and the four-theme Fusion/Windows matrix at 1x, 1.25x and 2x). \"Visibly ... without the heavy native bezel\" is a native macOS look offscreen renders cannot show, so it waits on the owner's verdict on frozen build 9c23301eb (attended session 053ec09c3451efe1c5ae725858a177fd, candidate RFC0087-AC01-shared-chip)."}},{"criterion":"AC02","outcome":"pass","revision":"9c23301ebbba3cc000afe6fb8a0c6f744ad0fbe7","evidence":{"kind":"observation","path":"docs/ux/shared-action-buttons.md","detail":"The 46 baseline rows plus the surfaces main added after the baseline are each migrated or retained with a reason; the 2026-10-22 scan of push/tool constructions, the shared classes and QPushButton/QToolButton/QAbstractButton subclasses finds 55 files, all listed. No migration is deferred."}},{"criterion":"AC03","outcome":"pass","revision":"9c23301ebbba3cc000afe6fb8a0c6f744ad0fbe7","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev --output-on-failure -j 4 -R <the focused selector in docs/verification/RFC-BPR-0087.md>","detail":"72/72 focused Qt and composed suites pass at every registered scale, including qt_quiet_buttons (keyboard, default, mnemonic, disabled, accessibility, icon/menu fallbacks, sizing, and the new default-to-chip size check), qt_feedback_label_settings and the commit filter's new split-diff choice check."}},{"criterion":"AC04","outcome":"pass","revision":"9c23301ebbba3cc000afe6fb8a0c6f744ad0fbe7","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"30/30 offscreen Fusion/Windows courses at 1x, 1.25x and 2x in four themes with hover, press, focus and disabled captures of Overview, Settings Review/Comments, Appearance, Diagnostics and the commit filter, inspected. Native macOS, Windows and Linux appearance are recorded as unobserved."}},{"criterion":"AC05","outcome":"pass","revision":"9c23301ebbba3cc000afe6fb8a0c6f744ad0fbe7","evidence":{"kind":"command","command":"python3 scripts/verify_documentation.py --root .","detail":"DOCUMENTATION_OK; clang-format 22.1.8 on all 50 changed C++ files; clang-tidy 22 on the 48 changed translation units; core-only build and tests; the ledger names commands, revisions, results, retained variants and platform work."}},{"criterion":"BPR-1662:integration-authorization","outcome":"pass","revision":"9c23301ebbba3cc000afe6fb8a0c6f744ad0fbe7","evidence":{"kind":"observation","detail":"Owner direction 2026-10-22, relayed by the coordinator: \"pick up the work you think we should complete and take over it\". The branch is rebased onto main for the coordinator to land; this card does not land it."}}]
---

# BPR-1662: Verify and close RFC-BPR-0087

## Acceptance

- Judge every RFC criterion against direct evidence, with the full migration inventory accounted for.
- Documentation gate, focused integration and relevant formatting checks pass; retain native-platform limitations and obtain separate integration authorization.

RFC base: `74ef7f4fb643b2fbbcd4753efcd1fe29c38bbddc`.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** Overview's selected/unselected Open/All and the migrated Settings choices/actions visibly use one shared treatment in all four themes, without the heavy native bezel or changes to labels and meaning. | Pending the owner's look. The technical part passes on the rebased candidate; whether the native bezel is gone on the owner's Mac is a look offscreen renders cannot give, so it is asked once, as Accept or Reject, on frozen build `9c23301eb`. |
| **AC02:** The app-wide inventory accounts for comparable text buttons on every Qt surface. Each is migrated or has an intentional variant reason; deferred migrations are visible and prevent app-wide closeout. | Pass. The [inventory](../../ux/shared-action-buttons.md) covers the baseline's 46 rows and the [surfaces main added](../../ux/shared-action-buttons.md#surfaces-added-on-main-after-the-baseline); its scan finds 55 button files, all listed, with none deferred. |
| **AC03:** Existing gestures and command outcomes still hold. Keyboard focus, exclusivity, checked state, disabled state, accessibility, icon/menu content and translated sizing survive adoption, including theme/style changes. | Pass. The focused selector's suites pass at every registered scale on the rebased candidate; see the [ledger](../../verification/RFC-BPR-0087.md#verification-on-the-rebased-candidate). |
| **AC04:** Representative composed Overview, Settings Review/Comments and another migrated page are rendered and inspected with hover, press, focus and disabled evidence at 1x, 1.25x and 2x. Available style variants are exercised; offscreen and native-platform claims are recorded separately. | Pass for offscreen drawing and layout. Native macOS, Windows and Linux are recorded as unobserved. |
| **AC05:** Focused composition and presentation checks, formatting and the documentation gate pass. The verification record names actual commands, revision, observed results, retained variants and remaining platform work. | Pass. |

## Execution record

The Codex branch proposed this closeout on 2026-10-19 and stopped at the
integration-authorization boundary. On 2026-10-22 the owner directed the work
taken over; its seventeen commits were cherry-picked onto main `4e318b625`
(the [ledger](../../verification/RFC-BPR-0087.md#takeover-onto-main--2026-10-22)
maps every SHA), main's newer button surfaces were brought in or retained with
reasons, and every criterion was rechecked on candidate `9c23301eb` (its
source is that of `da892a514` after the final rebase onto `c7d82b4ea`).

## Remaining

**`human-review` — AC01.** One Accept/Reject question in fresh attended
session `053ec09c3451efe1c5ae725858a177fd` (state directory
`/Users/jaeger/.mask/attended-review/owner-review-20261022-shared-buttons-v001`),
candidate `RFC0087-AC01-shared-chip`, frozen root
`/Users/jaeger/bpr-rfc0087-shared-buttons-v001.vwv381ny` (executable SHA-256
`22e37973…`, its own `--check` reached `ATTENDED_PULL_REQUEST_OPENED`). On
Accept this card and RFC-BPR-0087 close; on Reject the note says which theme
and control to change.
