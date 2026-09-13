---
schema: "implement-rfc.card.v1"
id: "PACK-SHELL-11"
title: "Update documentation and remove the retired panels"
status: "Backlog"
rfcs: ["RFC-BPR-0032", "RFC-BPR-0033"]
depends_on: ["PACK-SHELL-04", "PACK-SHELL-05", "PACK-SHELL-07", "PACK-SHELL-09"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["A1","A4","B4"],"alias_requires_live_allocation":true}
---

# PACK-SHELL-11: Update documentation and remove the retired panels

## Outcome

Rewrite the workbench specification's host, public-preview and device sign-in sections to the card, the settings guide's Accounts and General pages, the profile and window authority inventory, and the settings authority inventory's row-to-authority mapping; delete the retired session-panel groups, servers editor and management controls once no course references them; run the documentation gate.

## Acceptance

- Documentation describes the card page, the Home column, the empty desktop per platform and the sheet; historical cards and packets are untouched.
- No retired widget remains in the Qt adapter or in test selectors; format and tidy pass on touched files.
- `python3 scripts/verify_documentation.py --root .` reports DOCUMENTATION_OK.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
