---
id: BPR-0171
title: Plan RFC-BPR-0011 verification and allocate its cards
status: Done
rfc: RFC-BPR-0011
depends_on: []
---

# Plan RFC-BPR-0011 verification and allocate its cards

RFC-BPR-0011 launches separately installed agent tools — Codex at home, a
corporate Claude or Bedrock harness at work — using **their own** credentials
and **their own** GitHub access, and contributes only the local review context
GitHub cannot provide.

## Outcome

`docs/verification/RFC-BPR-0011.md` records the allocation, the claim matrix,
and cards BPR-0172 through BPR-0179.

## The rule the whole plan serves

**BetterPullReview passes no credential.** Not a model provider's, not AWS's,
not its own GitHub token. The agent authenticates as itself, using whatever the
user already configured for it, and BetterPullReview never learns what that is.

This is the criterion a convenience would erode first, and the convenience is
obvious: the agent needs GitHub access, BetterPullReview has a GitHub token, so
pass it along and save the user a setup step. That would make every agent
invocation carry the reviewer's own authority into a process the tool does not
audit — and the RFC's non-goals say plainly that BetterPullReview does not
claim to audit every file or network request a full agent makes.

Structural expression: the launch request type has no field a credential could
occupy, and the environment is an allowlist rather than an inheritance.

## The second rule

**Prompts and context go over stdin or a structured protocol, never argv.**
Command lines are world-readable on every platform this ships to: `ps` on
POSIX, the process list on Windows. A context supplement naming an unreleased
feature, a customer, or a private path would be visible to every other user on
a shared machine.

Structural expression: the process request carries arguments and a separate
input channel, and the context has no path into the former.

## Why this RFC can be implemented now, and needs nothing

`depends_on` names RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0007, all
implemented on available targets. Done BPR-0088 records the product owner's
direction to advance the implementation frontier.

More usefully: the RFC's non-goals exclude calling model APIs and storing
provider credentials, and its acceptance criteria name Codex and Claude
**fixture processes**. Nothing here needs an API key, a real agent
installation, or a network. That was checked against the RFC text rather than
assumed.

## What is genuinely deferred

Only one thing, and it is optional rather than required for closeout: a smoke
test against a really installed Codex or Claude CLI. The RFC does not ask for
it — the fixture processes are the named surface — so it is recorded as
optional extra validation rather than carded as a gate.

## The document already exists

`docs/architecture/ai-agent-boundary.md` is 50 lines written under an earlier
RFC and already states the allocation, the four protocols, host policy,
workspace modes, the context supplement's contents, and session semantics —
including that BetterPullReview "cannot claim to audit every item the full
agent independently accessed".

BPR-0178 extends it rather than writing a second document beside it. A new file
would leave a reader to work out which of the two was current, and the answer
would be "both, partly".

What it must add is what implementation settles and prose could not: the shape
that makes the credential rule true rather than stated, the argv rule and why,
and what each claim was checked against.

## Cards

| Card | Slice |
|---|---|
| BPR-0172 | Core: agent profiles, host policy, and explicit-invocation legality |
| BPR-0173 | Core: session identity, revision binding, and staleness |
| BPR-0174 | Core: the context supplement and what it may carry |
| BPR-0175 | Host: process launch, environment allowlist, and the credential boundary |
| BPR-0176 | Host: workspace preparation, pinned and dirty |
| BPR-0177 | Qt: explicit launch, disclosure, and cancellation |
| BPR-0178 | Documentation: extend `docs/architecture/ai-agent-boundary.md` |
| BPR-0179 | Simplification and closeout preparation |
