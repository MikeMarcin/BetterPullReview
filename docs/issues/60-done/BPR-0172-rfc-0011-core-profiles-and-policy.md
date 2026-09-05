---
schema: "implement-rfc.card.v1"
id: "BPR-0172"
title: "Core: agent profiles, host policy, and explicit-invocation legality"
status: "Done"
rfcs: ["RFC-BPR-0011"]
depends_on: ["BPR-0171"]
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

## What this slice delivered

`core::agent_profile`, `core::agent_host_policy`, and
`core::decide_agent_invocation`. Legality is a pure function of the profile
and the policy — no process, no clock, no provider — so the suite runs under
`core-only` and every decision replays.

### The credential rule, made checkable

`agent_profile` carries an identity, a display name, an executable, a
protocol, a workspace policy, and a list of environment variable **names**.
There is no field a secret could occupy, and the allowlist is a `vector` of
names rather than a map: a map would give every name a value, and a value is
where a secret goes.

The test reads the type rather than a construction site. It asserts each
field's type by name and then asserts the struct's total size against the sum
of those fields. Adding a `Token` member fails to **compile**:

```
static assertion failed: agent_profile gained or lost a field. If it gained
one, say here why it cannot hold a credential.
```

That is the difference between a rule and a promise. A test that built a
profile and searched it for a secret would pass for every profile nobody
thought to put one in.

### An unmapped host refuses before anything else

The absent-policy check sits above the allowed-list check on purpose, so a
reviewer cannot reach a permissive answer on an unmapped host by naming a
well-formed profile. Making it permit instead fails `an unmapped host permits
nothing, rather than defaulting to permitting everything`.

### Required is not preferred

Being on the allowed list is not enough where the host requires one; the
verdict names the profile required instead. Removing that check fails `a host
that requires a harness refuses one it merely allows`, and the same test
asserts the required profile *is* permitted — so the refusal is about which
profile, not about a policy that refuses everything.

## Non-vacuity

| Break | Failure |
|---|---|
| An unmapped host permits everything | `an unmapped host permits nothing, rather than defaulting to permitting everything` |
| A required profile becomes merely preferred | `a host that requires a harness refuses one it merely allows` |
| Add a `Token` field to `agent_profile` | compile error on the field-sum assertion |

## What this card does NOT deliver

- **No session, revision binding, or staleness** — BPR-0173.
- **No context supplement** — BPR-0174.
- **No launch, environment materialisation, or process** — BPR-0175. This card
  decides legality; nothing here starts anything.
- **No profile registry.** `policy_for_host` searches a span the caller owns;
  where profiles and policies are stored is the host's question.

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
