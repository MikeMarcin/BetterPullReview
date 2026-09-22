---
schema: "implement-rfc.card.v1"
id: "BPR-0976"
title: "Share server conversations across exact-authority profile windows"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0697", "BPR-0699", "BPR-0706"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0024-closeout-r2"}
---

# Share server conversations across exact-authority profile windows

## Scope

Complete RFC-0024 AC-03/08/09: a fresh accepted server conversation snapshot
in either window reaches exact-profile/account/repository/PR siblings.
Added, edited, deleted, resolved and reopened conversation state is shared;
each window retains its own comparison, file, navigation and scroll state.
Application hosts coordinate delivery through the serialized decision lane;
Qt is not an inter-window bus. Authentication replacement, pending navigation,
late completions and detached windows cannot receive stale authority.

Owned surfaces: core GitHub window read policy; application GitHub read/profile
hosts and narrow contracts; application composition and fixture; focused host,
core and composed multi-window tests. No live provider writes.

## Acceptance

- Exact-authority sibling windows receive accepted added/edited/deleted and
  resolve/reopen conversation state without moving either view.
- Different profiles/accounts/PRs, retired sessions and detached or newly
  navigating windows do not receive inapplicable updates; fanout is bounded
  and reentrant-safe, with no refresh loop or additional provider request.
- The shortest composed refresh path and discriminating stale/negative tests
  pass; the relevant core/host and architecture checks pass.

## Evidence

Pending. Baseline at `444d215c`: 18 focused profile/window selectors pass,
including the composed multi-window path; this does not prove server fanout.
