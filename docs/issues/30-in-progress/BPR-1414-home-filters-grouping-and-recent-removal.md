---
schema: "implement-rfc.card.v1"
id: "BPR-1414"
title: "Repair Home role filters and grouping, lead with titles, and remove recent entries"
status: "In Progress"
rfcs: ["RFC-BPR-0026", "RFC-BPR-0032"]
depends_on: []
priority: "P1"
---

# BPR-1414: Home filters, grouping and Recently viewed

The owner reported that a CLI account's Reviewer and Requested filters hid its
pull requests, Repository grouping appeared ineffective, and repository/number
identity dominated the PR title. They also requested removal from Recently
viewed. The 2026-10-12 takeover mandate is to finish Claude's work directly and
land it on main. The reserved ID and existing WIP are retained.

## Acceptance

- **AC1:** My role uses accepted membership reasons, including requested and
  reviewed discovery and overlapping roles, for native and CLI account rows.
  No role excludes rows with any such role. Exercise the composed filters.
- **AC2:** Repository grouping changes the real Home list, survives the host
  round trip and restart, and remains window-local. Verify state headers on
  and off without losing selection or hiding rows.
- **AC3:** A known PR title leads the row; repository and number are secondary
  and omit the part already named by a group. Unknown titles retain identity.
  Authors, literal wrapping, accessibility and native selection remain intact.
  Inspect offscreen application renders using the frontend-design skill.
- **AC4:** A recent row can be removed through its menu or Delete/Backspace.
  Removal updates the profile's windows, persists across restart, preserves
  other destinations and open reviews, and makes no GitHub request. Reopening
  the PR may add it again. The existing ten-entry, no-expiry bound remains.

## Execution

One direct milestone delivers AC1–AC4 together. Base before Claude's work:
`241c21087`. Takeover preserved all uncommitted edits at `b229fdf1b` and merged
current main without conflicts. No running command from this worktree remained.
The registered main project's direct preflight passes; this legacy worktree
is not separately registered with MASK. No agents or global settings are used.

## Evidence

Pending completion.
