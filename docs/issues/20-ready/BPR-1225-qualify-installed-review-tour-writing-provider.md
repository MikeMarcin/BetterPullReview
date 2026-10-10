---
schema: "implement-rfc.card.v1"
id: "BPR-1225"
title: "Qualify the installed review-tour writing provider"
status: "Ready"
rfcs: []
depends_on: ["BPR-1013"]
metadata: {"standalone_follow_up":true,"owner_amendment":"r2-20260927-close-implemented-scope","originating_pack":"bpr-review-tour-v1","originating_rfcs":["RFC-BPR-0039","RFC-BPR-0040","RFC-BPR-0041","RFC-BPR-0042","RFC-BPR-0043"],"original_criteria":["AC43-10"],"source_card":"BPR-1013","excluded_from_terminal_scope":true,"owner_answers":{"2026-10-09":{"bpr1225-provider":"Claude code, sonnet-5-5, my account, yes\nCodex cli, gpt-6.1-sol , my account, yes","source":"wizard bpr-owner-inputs-20261009-v001, session 10567d12-3a30-44d5-ab84-ebbd3af1b500, revision 2"}}}
---

# BPR-1225: Qualify the installed review-tour writing provider

## Owner amendment and retained obligation

Standalone follow-up under the owner's 2026-09-27 instruction: “Just close and land it on main anyways, can file the GHE stuff as blocked follow-ups.” The [r2 mandate](../../verification/RFC-BPR-0039-0043.owner-closeout-r2.md) accepts only the verified implementation. This card remains Blocked; no external qualification is passed by that scope change.

## Required course and blocker

Use an approved installed restricted provider, exact model/account and explicit inference authority. Observe actual provider compliance with the existing no-tools/MCP/hooks/writes/credentials boundary and actual Problem, Solution, outline and stop-writing quality, uncertainty and faithful meaning on approved synthetic context. Exercise exact selection/whole/empty scope, native undo/redo, settings, cancellation/staleness and delivery. Retain fixture evidence for policy/protocol correctness separately. No approved installed provider/model/account or inference authority has been supplied; no model-quality or installed-enforcement pass is inferred from RFC closeout.

Use the [existing qualification course](../../verification/RFC-BPR-0039-0043.manual-course.md) and preserve the [candidate/evidence ledger](../../verification/RFC-BPR-0039-0043.md). No live mutation, inference, platform activation or unrelated predecessor work is authorized by this card alone.

## Original criteria retained verbatim

- **AC43-10:** End-to-end native editor tests exercise the real undo path, profile settings and agent delivery seam; canned response tests are not represented as quality evidence for an untested model.

These original criteria require the direct evidence described above for this card's host/platform/provider slice. Shared criterion IDs on the GitHub.com and GHES cards partition host scope; neither card closes the other.

## Completion evidence

Record exact candidate, environment/host/provider identities, command or observed action, result and relevant human verdict. Unsupported/not-run rows retain their limitations; do not generalize fixture or historical evidence. This card starts with no passing qualification record.

## Owner session prepared, 2026-10-09 (main, Mac)

The missing input (an approved installed provider, model and account, and inference authority) is the second step of wizard `bpr-owner-inputs-20261009-v001` (session `10567d12-3a30-44d5-ab84-ebbd3af1b500`, state `/Users/jaeger/.mask/wizard/bpr-owner-inputs-20261009-v001`). The answer is a named provider, model, account and a yes, or 'not now'. No inference ran.

## Owner answer, 2026-10-09: two providers

The owner answered the wizard's second step (`bpr1225-provider`, session
`10567d12-3a30-44d5-ab84-ebbd3af1b500`, event revision 2, actor human) with,
verbatim (two lines):

```text
Claude code, sonnet-5-5, my account, yes
Codex cli, gpt-6.1-sol , my account, yes
```

Read as two approved installed providers, each with explicit inference
authority on the owner's own account:

| Provider | Model as the owner wrote it | Model id used | Account | Inference authority |
|---|---|---|---|---|
| Claude Code (installed `claude`) | `sonnet-5-5` | `claude-sonnet-5-5` | owner's own | yes |
| Codex CLI (installed `codex`) | `gpt-6.1-sol ` (trailing space as written) | `gpt-6.1-sol` | owner's own | yes |

This supplies the missing input; the blocker `installed-provider-model-evidence`
is lifted and the card is unblocked for the qualification run. The wizard
host was stopped after recording.
