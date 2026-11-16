# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **release-preparation-20261115** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## installer-choice-source

Release preparation has a durable ownership/dependency inventory and a source-verified Windows installer offering current-user and everyone scope, with native acceptance explicitly reserved.

**Work:** `BPR-1788` [AC1]

**Prerequisites:** None

**Completion:**

- Record all eleven current release/platform/feature cards, prior lane ownership, post-0.4.2 process improvements and independent Intel Ubuntu provisioning as a durable dependency plan without duplicating active work.
- Implement conventional fresh per-user default, explicit everyone choice with UAC, context-aware directory/registry/shortcuts, existing scope preservation and no claimed cross-scope MSI upgrade.
- Run focused package configuration tests and documentation checks against the exact committed source; retain native MSI compilation, ICE validation, ordinary-user UAC, upgrade, repair and uninstall as pending acceptance.

**Boundary:** Owner release preparation mandate 2026-11-15 r2: no publication/signing/version allocation, no Claude Ask/queue/report or active loading/fixture edits, no owner desktop capture. Source authority confined to this branch; other worktrees and native candidates remain protected.

## installer-native-acceptance

The exact final dual-scope MSI is validated and exercised on native Windows as an ordinary user in both contexts.

**Work:** `BPR-1788` [AC2]

**Prerequisites:** installer-choice-source

**Completion:**

- Compile with CMake 4.4.2 and WiX 5.0.2, retain actual MSI tables and ICE validation including ICE105.
- Install fresh as current user without UAC and Everyone with UAC; same-scope old per-user and per-machine upgrades retain directory and scope; explicit scope changes require uninstall and reinstall.
- Verify context-aware Start menu and registry, custom-directory repair/removal, standard-user Everyone credential prompt, both-marker conflict refusal, and no deletion of user data.

**Boundary:** Requires reconciled native Windows host/command lease and disposable account or owned install roots. A Mac source check is not native MSI acceptance.
