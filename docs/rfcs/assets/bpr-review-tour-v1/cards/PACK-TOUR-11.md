---
schema: "implement-rfc.card.v1"
id: "PACK-TOUR-11"
title: "Support the admitted gh api transport for tour operations"
status: "Backlog"
rfcs: ["RFC-BPR-0041"]
depends_on: ["PACK-TOUR-09", "PACK-TOUR-10"]
---

# Support the admitted gh api transport for tour operations

Pack alias: **PACK-TOUR-11**. Allocate/remap a live BPR ID at intake; this is not an already reserved board card.

## Outcome and work

Extend finite operation compilation to exact GET/POST/PUT/PATCH/DELETE and any approved fixed GraphQL route, with explicit hostname and body on bounded stdin through --input -. Preserve active-account checks, CLI write confinement/version/platform qualification, no token extraction and no shell interpolation. Shared normalization must match native HTTPS; avoid the old GET-versus-POST shortcut for new verbs.

## Existing ownership seams

`src/adapters/github/gh_cli_transport.cpp; github_cli_process_runner; RFC-BPR-0015 admission and platform boundary`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Argv/stdin exact-byte tests, shell metacharacters in prose, error status, account switch before/after, bounded IO/cancellation, unexpected redirects, JSON integer fidelity and unavailable-platform refusal. No claim of real CLI qualification from a fake process test.

Requirement ownership: `AC41-04`, `AC41-08`.

## Integration boundary

Milestone: **staged-publication**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
