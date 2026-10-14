---
schema: "implement-rfc.card.v1"
id: "BPR-1486"
title: "The appearance theme course's fixture theme lacks the C++ identifier roles"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_rfcs":["RFC-BPR-0072"]}
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
