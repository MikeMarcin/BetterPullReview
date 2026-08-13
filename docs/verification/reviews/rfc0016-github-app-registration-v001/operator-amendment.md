# RFC-BPR-0016 private-app operator amendment

Identity: `rfc0016-private-owner-only-2026-08-13`

Status: **accepted plain operator direction**

After creating the verification GitHub App, the operator gave this exact
direction:

> Yes make it private, i dont see any reason to be public

This later direction supersedes only the any-account/public-app portions of
facts 1, 5, and 10 in `decision-subject.txt`. The `BPR-Test` verification app
is private, owner-only to `MikeMarcin`, and installed only on the selected
`MikeMarcin/BPR-Test` repository. Every permission, endpoint, no-mutation,
secret-handling, refresh, and designated-repository limit from the accepted
decision remains unchanged.

This is an authority-narrowing, reversible registration choice, so it is
recorded as plain operator direction rather than opening another digest-bound
decision packet. Evidence derived from the private app must bind both the
original decision-subject digest and this amendment identity; citing the
original subject alone would misstate the selected installation policy.

The private verification identity does not establish a distributable
any-account production identity. RFC-BPR-0016 implementation and hermetic or
owner-attended verification may proceed with it, but production selection for
other GitHub accounts and RFC closeout remain unavailable until a separately
appropriate identity exists.
