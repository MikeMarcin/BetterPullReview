---
schema: "implement-rfc.card.v1"
id: "BPR-0626"
title: "Present unavailable diff reasons instead of zero changes"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0625"]
---

# Present unavailable diff reasons instead of zero changes

BPR-0624's first composed large-file run returned a typed `compute_limit`
result, but the application displayed “Diff ready — 0 semantic changes”. The
bounded matching fix in BPR-0625 makes that particular input available; other
resource-limited inputs still need truthful presentation.

## Bounded acceptance

- Project unavailable/nontext result state and its typed reason through the
  existing application/Qt presentation path. An unavailable comparison must
  not imply a successful comparison with no changes.
- Keep genuinely equal text, binary content and unavailable content distinct;
  use useful, copyable user-facing wording without exposing source content.
- Exercise a real composed application selection that exceeds a retained
  compute limit; render and verify its unavailable message, then select a
  supported file and verify the normal state returns. Reuse focused core reason
  evidence and add only missing presentation coverage.
- Inspect existing ownership before implementation. Expected scope is the
  application projection, Qt status presentation and focused public-path test.
  No new algorithm, settings system or diagnostic subsystem is required.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
Use synthetic fixtures and local Mac checks. No GitHub Actions, push, paid CI,
credential access or production application launch.
