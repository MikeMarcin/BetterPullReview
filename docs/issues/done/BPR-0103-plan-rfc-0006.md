---
id: BPR-0103
title: Plan RFC-BPR-0006 and decide syntax dependency acquisition
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0099]
---

# Plan RFC-BPR-0006 and decide syntax dependency acquisition

Allocate the RFC-BPR-0006 task set, choose the architecture allocation, build the verification matrix before implementation, and settle how KSyntaxHighlighting is acquired.

Order the work so the appearance, font, and theme surfaces — which need no new dependency — land before the build foundation changes. That delivers the unblocked half first and keeps the dependency risk isolated to its own slice.

## The acquisition decision

RFC-BPR-0006 mandates KSyntaxHighlighting through its low-level API, so the
library is not optional and only the route was open.

Caller-installed discovery was investigated first, because KSyntaxHighlighting
is a Qt-dependent KDE Frameworks library and the dependency boundary treats Qt
as a caller-installed package that is never fetched. That route is unavailable
here: this host has no Homebrew formula for KSyntaxHighlighting, and none for
the KDE Frameworks generally. Requiring a package that cannot be installed
would make the RFC unimplementable rather than merely deferred.

CPM source acquisition is therefore the only route that satisfies the RFC at
all, following the existing SQLite precedent of pinned version, cryptographic
digest, and offline-capable cache. This is a forced choice rather than a
preference, and it has a cost that BPR-0109 must record rather than absorb
silently: KSyntaxHighlighting builds with extra-cmake-modules, so ECM becomes a
transitive build dependency needing its own pinned version and digest in the
inventory.

Acquisition does not relax the RFC's non-goals. Definitions and approved themes
stay packaged as resources and standard-path discovery and downloads stay
disabled in the production build. Build-time source acquisition and runtime
grammar downloading are different things, and only the second is forbidden.

## Ordering

Thirteen cards as BPR-0104 through BPR-0115. The appearance, typography, and
theme-import surfaces need no new dependency and land first; the build
foundation is isolated in BPR-0109 and the syntax work follows it. That
delivers the unblocked half before any dependency risk is taken, and stops a
build regression from blocking unrelated slices.

## Gate classification

One owner-attended gate: BPR-0114, the live operating-system scheme change.
The static fresh-install-defaults-to-System claim is deliberately separated
onto BPR-0106, because it is a default-value check that needs no appearance
change and would otherwise have been parked behind an attended gate for no
reason.

Nothing here needs target-native Windows, a GitHub mutation, a credential, or
a runtime network service. The single build-time network need is the first CPM
fetch, after which the cache serves offline configures.

## Result

RFC-BPR-0006 is planned with its verification matrix built before
implementation, its one genuinely forced dependency decision recorded with the
reasoning rather than just the outcome, and its work ordered so the dependency
risk is isolated. BPR-0103 is complete.
