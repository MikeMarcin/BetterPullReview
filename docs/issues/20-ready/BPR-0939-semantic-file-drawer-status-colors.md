---
schema: "implement-rfc.card.v1"
id: "BPR-0939"
title: "Match file drawer added and removed colors to Overview"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r20-usable-review-amendments"}
---

# Match file drawer added and removed colors to Overview

Owner amendment after observing a rendered file diff in the repaired signed r21
UnrealEngine#1773 CLI course. Preserve that success without asking for another
owner test. This card belongs to the existing selected-diff-exact-epoch-pair
milestone; no live GitHub mutations or private source in committed evidence.
Overview folder collapse remains separate in [[BPR-0934]].

## Acceptance

- AC1: Use theme-derived green for added and red for removed file operations consistently between the file drawer and Overview.
- AC2: Preserve native selection/focus, light/dark/high-contrast readability, literal filenames and accessible operation wording; color is not the only signal.
- AC3: Focused model/delegate tests and rendered composed file-drawer evidence cover added, removed, modified and selected rows.

## Evidence

Implementation and direct verification pending.
