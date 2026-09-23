---
schema: "implement-rfc.card.v1"
id: "BPR-0933"
title: "Shorten the GitHub CLI trust confirmation"
status: "Ready"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P2"
---

# Shorten the GitHub CLI trust confirmation

The owner requested this follow-up in attended decision 432 on 2026-09-20:
the CLI warning dialog is too long and visually unwieldy. The decision applies
to candidate `rfc0013-v004-9c670d88-71117a5c`, source `9c670d88`.

## Acceptance

- Present a concise, readable trust decision at ordinary and narrow macOS
  window sizes; put supporting detail behind an accessible disclosure.
- Preserve the explicit configured executable/account trust, residual account
  switch and configuration risks, read versus publish authority, and an obvious
  Cancel path. This is presentation work, not expanded CLI authority.
- Verify the real sign-in/CLI opt-in composition, keyboard/accessibility flow,
  and rendered dialog without changing credentials or global CLI state.

## Scope

This records the requested follow-up only. It is separate from the RFC-0013
selected-diff repair and does not gate that milestone's acceptance.
