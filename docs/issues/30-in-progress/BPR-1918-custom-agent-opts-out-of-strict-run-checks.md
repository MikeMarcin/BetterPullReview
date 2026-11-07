---
schema: "implement-rfc.card.v1"
id: "BPR-1918"
title: "A custom agent profile can opt out of the strict writing and read-only run checks"
status: "Backlog"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat","related_cards":["BPR-1375","BPR-1891","BPR-1896","BPR-1899","BPR-1900","BPR-1912","BPR-1916"]}
acceptance: []
---

# BPR-1918: A custom agent profile can opt out of the strict writing and read-only run checks

## Owner (2026-11-06)

"right now I'm only going to run against trusted repos with the wrapper so im
not too worried so long as it works. If we're going to build something strict
which should have an opt out for custom agents at least."

Context: the owner's custom agent runs Claude Code through a managed wrapper
that replaces this app's `--settings` (BPR-1896), and it may widen
permission rules (BPR-1912: `awk` ran). Each strict check that a wrapper
trips has cost a card. The run checks are:

- the writing init checks (tools, MCP);
- the read-only init and tool-use checks (`tool_refused`);
- the text-only answer check;
- BPR-1916's unreferenced-skill check.

## Change

- A per-profile setting on custom agent profiles (Agent settings): **Strict
  run checks**, on by default. With it off, writing and read-only runs for
  that profile don't end on a check that a wrapper's or policy's settings
  can trip:
  - extra tools or MCP servers in init;
  - a tool_use outside the list;
  - non-text blocks in a writing answer;
  - unreferenced skills.
  Everything such a check would have refused is reported instead, in the
  transcript and in diagnostics (`relaxed=on`, with counts, never content).
- The app still passes its restrictive arguments, so a wrapper that honours
  them stays tool-free or read-only. Only this app's own enforcement relaxes.
- Things that stay, whatever the setting:
  - the private, disposable workspace or empty writing folder;
  - never using the reviewer's own checkout;
  - text answers are the only content that becomes tour text;
  - proposals still need acceptance.
- Built-in profiles keep strict checks. Record whether to offer the switch
  there later.
- The setting says plainly what it means in one or two lines (for example,
  "Use only with repositories you trust. The app reports what the agent did
  instead of stopping it."). Load `frontend-design:frontend-design` for the
  control and the wording.
- Threat model: record the opt-out and its residual.

## Acceptance

- AC1. With strict checks off, a writing or read-only run whose init reports
  an extra tool, or that uses a tool outside the list, finishes. The
  transcript and diagnostics name what happened.
- AC2. With strict checks on (the default), behaviour is unchanged. The
  existing refusal tests pass.
- AC3. The setting is per custom profile, stored through the host, and shown
  only for custom profiles.
- AC4. Owner look: an Accept/Reject on the setting's control and wording, in a
  frozen build.
- AC5. The documentation gate passes.
