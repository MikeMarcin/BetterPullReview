---
schema: "implement-rfc.card.v1"
id: "BPR-0909"
title: "Make GitHub CLI a discovered sign-in path"
status: "Ready"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"owner private v0.1.0 GHES test","milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
---

# BPR-0909: Make GitHub CLI a discovered sign-in path

## Defect

The private v0.1.0 macOS build precomposes a GitHub CLI profile for GitHub.com,
but the same entry path cannot see a GHES session that `gh auth status` reports.
This makes the CLI look like another transport on an explicitly configured
account card even though the CLI already owns its host and account inventory.

The owner clarified the intended product model: `Use GitHub CLI` is a separate
sign-in path from explicit Native HTTPS/device-or-token accounts. Choosing that
path asks the installed official `gh` for its authenticated host/account
inventory, then offers those exact active sessions. It does not require the
reviewer to add the same GHES hostname separately first.

## Scope

- Offer `Use GitHub CLI` as its own signed-out entry path beside explicit
  GitHub and enterprise-account setup.
- Only after that explicit gesture, run the installed official `gh` through the
  existing constrained process boundary and request its active authentication
  status as bounded JSON. Never pass `--show-token`; admit only host, login,
  active and success state, and retain no credential material.
- Register one CLI profile for each admitted active host. GitHub.com may join
  its existing server card; a discovered GHES hostname may create a CLI-only
  card without first creating a Native HTTPS account. Selecting it remains
  bound to that exact hostname and passes through the existing CLI trust and
  read-consent warning.
- Do not launch `gh` merely because the screen opened, silently select a
  discovered account, change the active CLI account, or persist a token.

## Acceptance

- The signed-out surface presents distinct `Sign in`, `Use GitHub CLI`, and
  `Add enterprise server` paths; opening it launches no CLI process.
- Choosing `Use GitHub CLI` issues exactly one bounded active-status request,
  parses a hostile/truncated/malformed corpus fail-closed, and exposes every
  valid active successful GitHub.com or GHES host without exposing a token.
- A discovered GHES session can be selected and reaches the existing exact-host
  CLI connection flow without an explicit GHES account being added first.
- Focused adapter, Qt and composition/public-path checks, formatting and
  documentation verification pass.
