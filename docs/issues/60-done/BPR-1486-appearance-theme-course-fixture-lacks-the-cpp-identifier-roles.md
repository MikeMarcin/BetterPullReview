---
schema: "implement-rfc.card.v1"
id: "BPR-1486"
title: "The appearance theme course's fixture theme lacks the C++ identifier roles"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_rfcs":["RFC-BPR-0072"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"993e07c91","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_review_round_trip_composition_contract|application_uiforetw_selected_diff_contract|application_workbench_composition_contract|application_start_review_contract|application_appearance_theme_customization_contract|live_review_submission_provider|application_review_navigation_entry|application_change_arrows_across_files)$' --output-on-failure","detail":"The fixture states both roles (BPR-1479 landed them first); the probe prints APPEARANCE_THEME_PROBE_COMMIT_FACTS and, on refusal, the panel status text."}},{"criterion":"AC2","outcome":"pass","revision":"993e07c91","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_review_round_trip_composition_contract|application_uiforetw_selected_diff_contract|application_workbench_composition_contract|application_start_review_contract|application_appearance_theme_customization_contract|live_review_submission_provider|application_review_navigation_entry|application_change_arrows_across_files)$' --output-on-failure","detail":"application_appearance_theme_customization_contract passes on macOS arm64 (8.97 s)."}}]
---

# The appearance theme course's fixture theme lacks the C++ identifier roles

`application_appearance_theme_customization_contract` fails at its commit
stage on main: "Keep changes" is refused. c58bdebda (RFC-BPR-0072) added the
`syntax_namespace` and `syntax_enum_member` roles but not to the course's
`tests/fixtures/appearance/contrast-safe-theme.bprtheme`, so import fills them
with Light's colours, which fail contrast on the theme's black code background.
Diagnosed in the owner's session "Diagnose four failing GUI public-path
contracts" (branch `claude/interesting-germain-a84def`, 85350f0f8).

## Acceptance

- AC1: The fixture theme states both roles, and the course reports the commit
  facts and the panel's refusal text when a commit is refused.
- AC2: `application_appearance_theme_customization_contract` passes on macOS.

## Boundaries

What an older imported theme's missing roles should become is a separate
owner design question; this card changes only the test fixture and probe output.

## Evidence

- Diagnosed and fixed in the owner's session "Diagnose four failing GUI public-path contracts" (branch `claude/interesting-germain-a84def`, 85350f0f8). By the time it landed, [[BPR-1479]] (392256fbc) had already added the same two roles to the fixture theme and filled an older theme's missing roles from its own base, so `application_appearance_theme_customization_contract` already passed in the 0.2.0 baseline gate on cd4860e30 (6.35 s). The merge keeps main's fixture and BPR-1479's `APPEARANCE_THEME_PROBE_CUSTOMISED_VARIANT` line, and adds the probe's commit facts and the panel's refusal text on a refused commit.
- AC2: on claude/bpr-1486-1489 at 993e07c91 (main a729920a0 merged), the row passes (8.97 s) in the focused run below.
- Focused run, macOS arm64, build/dev of the release-gate worktree: `ctest --preset dev -R '^(application_review_round_trip_composition_contract|application_uiforetw_selected_diff_contract|application_workbench_composition_contract|application_start_review_contract|application_appearance_theme_customization_contract|live_review_submission_provider|application_review_navigation_entry|application_change_arrows_across_files)$' --output-on-failure`: all eight pass.
