---
id: BPR-0318
title: Record a distributable device-authorization identity
status: Parked
rfc: RFC-BPR-0016
depends_on: [BPR-0276, BPR-0282]
parked_on: awaiting-product-decision
---

# Record a distributable device-authorization identity

## Required final state

- The private, owner-only `BPR-Test` application remains a bounded verification
  identity and is never silently widened, reused, or represented as the client
  identity for a generally distributed BetterPullReview build.
- Before any production platform becomes available, the product owner gives
  plain direction to proceed with a separate distributable GitHub.com
  application identity. If that direction is deferred, every production
  platform remains visibly `device_flow_unavailable` with zero requests and
  RFC-BPR-0016 remains open.
- The reversible go/no-go choice uses that plain direction only. If the owner
  proceeds, this card creates one bounded
  `rfc0016-production-app-registration-v001` operator packet because the next
  action registers an external production identity and authorizes exact real-
  service traffic. Its validated claims authorize only the named settings,
  permissions, designated read-only repository, and evidence boundary; they
  authorize no mutation, release, distribution, GHES change, or secret-bearing
  evidence.
- The distributable identity is recorded with its owner, public client
  identifier, public-installation policy, device-flow and expiring-token
  settings, exact repository permissions, selected test installation, and
  absence of a shipped client secret. Registration or real-service traffic is
  performed only under separately recorded bounded authority.
- A designated read-only course proves that the exact production client can
  authorize against `MikeMarcin/BPR-Test`, reach the existing operation
  catalog with the approved permissions, and retain no secret-bearing
  evidence. No repository mutation, release, or distribution occurs in this
  card.
- Only after the identity and every BPR-0282 macOS native/security row pass
  does this card own the macOS production capability-selection flip and the
  complete clean Release/public/native/hygiene rerun against the enabled
  artifact. Failure or deferral leaves macOS unavailable and this card
  nonterminal.
- BPR-0310 and BPR-0311 consume this same recorded production identity before
  their independent target-native Linux and Windows enablement courses.

## Verification contract

- This card owns the packet, response, claims contract, and committed
  validation record under
  `docs/verification/reviews/rfc0016-production-app-registration-v001/`.
  Replay uses the checked-in attended-review validator with
  `--validate-existing` and the committed record identity.
- This card creates `scripts/verify_rfc0016_production_identity.py` and its
  isolated tests. The verifier accepts only the exact packet validation,
  sanitized registration observation, route-closed token-success/read-only
  course, expected public client selection, and secret-free artifact
  inventory; it has a no-traffic `--validate-existing` mode for closeout.
- After the identity evidence passes, the exact macOS sequence is the BPR-0282
  unavailable course, the single macOS selection change, and the enabled
  course from `docs/verification/RFC-BPR-0016.md`. The selection change and
  both outputs are committed as distinct provenance before this card becomes
  terminal.

## Parked on the proceed/defer decision — 2026-08-25

Both dependencies are done: [[BPR-0276]]'s attended capture was admitted on
2026-08-24 and [[BPR-0282]]'s macOS native and security course passed on
2026-08-25 with production selection still `device_flow_unavailable` and zero
requests on every platform. Nothing is queued in front of this card and no
unattended work remains in it.

What it waits on is one plain sentence from the product owner:

> Proceed with registering a separate distributable GitHub.com application
> identity for BetterPullReview, or defer.

That is deliberately **not** a digest-bound packet. The choice is reversible
and changes nothing on its own — deferring leaves every platform visibly
`device_flow_unavailable` with zero requests, which is exactly where the tree
already is, and RFC-BPR-0016 simply stays open. A packet binds exact identity
to an authority, and there is no identity to bind until the answer is
"proceed".

If the answer is proceed, this card then creates the bounded
`rfc0016-production-app-registration-v001` operator packet, because the next
action after that registers an external production identity and authorizes
exact real-service traffic — which is precisely the kind of decision that does
need exact identity behind it. `docs/verification/reviews/rfc0016-production-app-registration-v001/`
currently holds only `registration-observation.json`; the packet, response,
claims contract, and validation record are this card's to write.

Nothing downstream can move first. [[BPR-0310]] and [[BPR-0311]] consume the
same recorded identity before their own target-native enablement, and
[[BPR-0285]] cannot replay an enabled final-head course on a build that has no
identity to be enabled with.