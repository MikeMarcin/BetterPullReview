---
schema: "implement-rfc.card.v1"
id: "BPR-0777"
title: "Stop presenting the chooser when the last window closes; stay resident on macOS and quit elsewhere"
status: "Backlog"
rfcs: ["RFC-BPR-0033"]
depends_on: ["BPR-0770"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"profile-chooser","acceptance":["B1","B2","B3"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0777: Stop presenting the chooser when the last window closes; stay resident on macOS and quit elsewhere

## Outcome

Remove the last-runtime presentation in `profile_desktop.cpp`; add an injected, testable empty-desktop policy that keeps the application resident on macOS and quits through the existing drain and Quit path on Windows and Linux; open the most recently used profile at Home from application activation, ⌘N and the Profiles menu through the existing intents; route failures with a live window to that window and present the chooser for a failure only when no window exists.

## Acceptance

- Closing the last window presents nothing; the resident or quit outcome follows the injected policy and the restore record distinguishes close from Quit.
- Activation, ⌘N and the Profiles menu reopen the most recent profile exactly once with one runtime owner.
- A failure with a live window reports there and presents nothing; with no window the chooser is presented; the existing management course still passes.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
