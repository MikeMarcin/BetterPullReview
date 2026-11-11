---
schema: "implement-rfc.card.v1"
id: "BPR-2096"
title: "Fit the agent picker to effective native control widths"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"bpr-2096-agent-picker-effective-width","path":"docs/rfcs/plans/bpr-2096-agent-picker-effective-width/milestone-plan.json"},"release_source":"48bd9b3f2b82128e6b43527812f165eacc4f8b47","owner_scope":"release56-picker-fit-r1"}
---

# BPR-2096: Fit the agent picker to effective native control widths

## Cause and scope

The original Linux picker course and its one numeric diagnostic fail the unchanged twelve-pixel gap oracle. At drawer width 520 the scope has raw hint 112 but minimum hint and actual width 116; the first-party fit sum underestimates the row. The observed picker/scope inclusive-coordinate gap is 11, leaving ten blank logical pixels. This is first-party layout sizing, with no Qt-library waiver or timing cause claimed.

## Acceptance

- AC1: Use public QWidgetItem preferred sizing in the existing width-only wrap calculation; preserve the twelve-pixel gap, stable reflow, visibility and all gestures. No host effects, styling or API changes.
- AC2: The fixture predicts effective widths independently and retains its original >=12 mapped-gap oracle and full light/dark, narrow/wide, hover, Tour and whole-word checks. A controlled minimum-greater-than-hint case distinguishes the old undercounting decision without a hanging or unbounded course.
- AC3: Bind exact native affected Release/Debug positives, meaningful negative and final successor cross-native obligations; focused format22/tidy/source/docs/board checks pass, only owned changes land main, frozen56 stays unchanged and physical leases return.

Frontend-design and its desktop reference were loaded before Qt edits. Original failed evidence and the approved proposal remain external under implementer56-linux-history-triage-r1. Native controls, implementation and successor acceptance remain pending.
