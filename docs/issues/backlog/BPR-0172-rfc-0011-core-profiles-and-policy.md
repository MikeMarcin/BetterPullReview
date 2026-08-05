---
id: BPR-0172
title: "Core: agent profiles, host policy, and explicit-invocation legality"
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Core: agent profiles, host policy, and explicit-invocation legality

A profile declares a display name, an executable, a protocol, an environment
allowlist, a workspace policy, and a host policy. Git hosts map to allowed or
required profiles; a corporate GHES host can require the corporate harness and
forbid a personal provider, and an unknown host has AI disabled until mapped.

## The rule this card must not erode

**No profile type carries a credential.** Not a token field, not a key field,
not a generic string map the caller could put one in. The absence is what makes
"BetterPullReview passes no credential" a property rather than a promise.

## Required direct evidence

- A forbidden personal provider is refused on a corporate host even when
  selected by name, and the refusal says which host forbade it.
- An unmapped host permits nothing, rather than defaulting to permitting
  everything.
- No type in this slice has a field a credential could occupy, and the test
  that says so reads the type rather than a construction site.
- Legality is decided without a process, a clock, or a provider; the suite runs
  under `core-only`.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
