---
schema: "implement-rfc.card.v1"
id: "BPR-2213"
title: "The next release's notes carry the agent checkout settings security notice"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"release-obligation","reservation":"docs/issues/_reservations/BPR-2213--BPR-2214--32c543fd79d5.json","origin":"BPR-2119's owed release-note advisory, carded under the owner's decision of 2026-11-16 on the hygiene audit","related_cards":["BPR-2119"],"related_rfcs":["RFC-BPR-0104"],"first_release_with_fix":"the first release whose source contains 059e191fb; 0.4.2 (tag at 6efba00e1) does not"}
---

# BPR-2213: The next release's notes carry the agent checkout settings security notice

## Why

[[BPR-2119]] (Done) put the guard on `main`: an agent launched in a
checkout BetterPullReview made from a pull request no longer loads that pull
request's own Claude Code or Codex settings. Every full package before it
did, so a hostile pull request could run code on the reviewer's computer the
first time the reviewer asked the agent about it, and Codex with a writable
sandbox also persisted `trust_level = "trusted"` for the checkout in
`~/.codex/config.toml`. Details:
[RFC-BPR-0104 ledger, release exposure](../../verification/RFC-BPR-0104.md#release-exposure-check-2026-11-11).

Release bodies are written once, at publication, from
`.agents/skills/publish-release/references/release-notes.md`; there is no
standing notes file to write the notice into now. The latest release, 0.4.2,
does not contain the fix (`059e191fb` is not an ancestor of its tag), so the
next release, prerelease or stable, is the first to ship it.

## The notice

Paste as the body's opening notice, before the download table (the template's
`Security` label), adjusting only the link to that release's exact records:

```markdown
**Security:** Earlier versions loaded a pull request's own Claude Code or Codex settings when you asked the agent about that pull request, so a hostile pull request could run code on your computer; this version ignores them. If you used Codex with `sandbox_mode = "workspace-write"`, remove the `trust_level = "trusted"` entries for BetterPullReview workspace paths from `~/.codex/config.toml`.
```

and, under `### Fixed`, one entry such as: "The agent no longer loads a pull
request's own agent settings (hooks, MCP servers, project trust) when it works
in a checkout BetterPullReview made."

## Acceptance

- AC1: The published body of the first release whose source contains
  `059e191fb` opens with the Security notice above (or the owner's revision
  of it) and lists the fix under Fixed.
- AC2: If a prerelease carries it first, the stable release that follows
  carries it too, since its change list is the one a stable reader sees.
