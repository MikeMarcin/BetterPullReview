---
id: BPR-0115
title: Verify and close RFC-BPR-0006
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0104, BPR-0105, BPR-0106, BPR-0107, BPR-0108, BPR-0109, BPR-0110, BPR-0111, BPR-0112, BPR-0113, BPR-0114, BPR-0118, BPR-0181, BPR-0182, BPR-0183, BPR-0184, BPR-0185, BPR-0220, BPR-0233, BPR-0236, BPR-0237]
---

# Verify and close RFC-BPR-0006

Replay the complete RFC-BPR-0006 claim matrix from clean state, resolve every documentation-ledger row, and obtain an independent review pass.

This card cannot mark RFC-BPR-0006 `Auto-Verified`. Closure is strictly ordered and RFC-BPR-0005, RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002 are all still `Proposed` pending real Windows x64 evidence. It also depends on the owner-attended BPR-0114.

It stays in `backlog/` and makes no closure claim.

## Historical composition finding and current correction

BPR-0180 measured that nothing RFC-BPR-0006 built is connected to the
application: `appearance_application_host` and `packaged_syntax_classifier`
have zero symbols in the shipped binary, `appearance_profile` is constructed
only by tests, and the composition never links
`BetterPullReview::AdapterSyntax`. BPR-0181 through BPR-0185 close that, and
BPR-0118 retires the one setting the product owner decided not to keep.

That composition finding is historical. At final HEAD the predefined profiles
and syntax classifier are composed only when durable settings open. BPR-0236
owns the missing default fallback, and BPR-0237 owns semantic palette
customization, preview, persistence, and safe import.

When this card is eventually prepared, the replay must be run against the
composed application. Replaying the component suites would reproduce exactly
the gap BPR-0180 found, since those suites all passed while the product had
no appearance model and no syntax highlighting at all.

Ask the linker as part of the replay. `nm` on the shipped binary reporting
symbols for every component this RFC specifies is a cheap check that no amount
of green component testing substitutes for.
