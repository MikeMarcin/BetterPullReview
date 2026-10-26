---
schema: "implement-rfc.card.v1"
id: "BPR-1769"
title: "Check a corporate Claude wrapper without requiring a Claude login and expose setup diagnostics"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner report 2026-10-26: process wrapper fails first-run setup and Show details is missing; owner confirmed Amazon Bedrock with corporate SSO, possibly Okta/VPN (unconfirmed)","reservation":"docs/issues/_reservations/BPR-1769--BPR-1769--8e61db489223.json","related_cards":["BPR-1301","BPR-1749","BPR-1753","BPR-1738"],"frontend_design":"frontend-design:frontend-design applied; preserve native font, palette and existing layout","milestone_plan":{"id":"bpr-1769-wrapper-setup","path":"docs/rfcs/plans/bpr-1769-wrapper-setup/milestone-plan.json"}}
---

# BPR-1769: Wrapper setup and check details

## Mandate

Repair the first-run regression reported on 2026-10-26. The owner's Claude
wrapper uses Amazon Bedrock and corporate SSO. Okta and VPN requirements are
uncertain. Do not require normal Claude account sign-in for this wrapper or
claim its external credentials were validated. Preserve the prior acceptance
of RFC-BPR-0092 and BPR-1301; this is a new follow-up.

## Acceptance criteria

- AC1: A saved preset Claude process wrapper receives a bounded supervised
  availability check without a normal Claude-account auth command. Missing,
  failed, timed-out or unreadable checks fail. Discovered plain Claude keeps
  strict status decoding; custom agents retain their own check arguments.
- AC2: Each failed setup row exposes a keyboard-accessible, literal, bounded
  diagnostic disclosure, with separate agent and workspace output. Rerunning
  a check clears old details. No output is automatically logged or copied.
- AC3: Focused host, real-process wrapper, Qt and composed public-path checks
  demonstrate the repair, including a failing-before regression and offscreen
  rendered diagnostics. Formatting and documentation gates pass. Record the
  absence of live Bedrock/SSO and other-platform validation.

## Design

Keep the existing native palette and font. The setup retains its sequence and
primary Check again action; a quiet Show details / Show the log disclosure
sits immediately under the corresponding failure. The disclosure opens a
read-only plain-text area. A wrapper row says its availability was checked,
with sign-in and provider access delegated to the wrapper. No new sign-in
switch, model request or environment widening.
