# RFC-BPR-0017 anonymous public-read probe operator response

Date: 2026-08-23

Packet SHA-256: `5b2437ac68aaa75715053e2f8991698ec492d3c2ca93c70b6dd45dc6b04a71b7` (6,305 bytes, the refreshed packet presented this day)

## Exact operator decision

The product owner returned this exact approval in the live attended session:

> I authorize exactly the credential-free read-only requests named below against api.github.com.

## Scope

The approval authorizes the five requests in the packet's authority block,
once. It does not authorize credentials, mutation, private content, retry
past a rate limit, GHES, disposable-repository authority, or any other
target, and it does not close BPR-0298 or RFC-BPR-0017.
