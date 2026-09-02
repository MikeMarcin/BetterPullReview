---
id: BPR-0301
title: Implement typed credential-free native HTTPS reads
status: In Progress
rfc: RFC-BPR-0017
depends_on: [BPR-0035, BPR-0312]
---

# Implement typed credential-free native HTTPS reads

## Required final state

- Native HTTPS accepts an explicit anonymous read authority distinct from a
  missing credential and structurally omits authorization and all vault access.
- The existing authenticated transport and RFC-BPR-0002 TLS, proxy, redirect,
  cancellation, timeout, and response-limit rules remain unchanged.
- Provider admission refuses GraphQL, viewer identity, all mutations, private
  intent, and GHES before constructing a request.
- Loopback positive and hostile tests prove header absence with canaries and
  prove that absent/invalid authenticated credentials do not become anonymous.

## Lane — 2026-09-02

[[BPR-0312]] is satisfied: the owner's verdict validated on 2026-09-02 and
the four public-preview definitions are bound in
`docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json`.
The fourth is this card's admission rule stated from the product side:
GitHub.com public repositories only, every GHES profile unavailable with
zero requests. [[BPR-0035]] is terminal, so the RFC-BPR-0002 transport this
card extends is closed and its TLS, proxy, redirect, cancellation, timeout,
and response-limit rules are the fixed baseline. Both dependencies hold, so
this card moves to `20-ready/` as generic work runnable on any station.
