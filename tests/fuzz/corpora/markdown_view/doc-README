# Synthetic examples, not recorded GitHub evidence

All hostnames, repositories, IDs, comments, source paths, commit IDs and code behavior here are illustrative fixtures. `github.example.invalid` is intentionally non-routable. Do not run these requests against a real host by replacing the hostname. No API call, source test or deployment was performed to produce these examples.

`manual-tour/` shows the ordinary PR description, review-summary index and three linked comments that a website-only reviewer would receive. The lifetime-handle scenario is invented to demonstrate good explanation; it is not a diagnosis of the user's project. The fourth file illustrates RFC-0022's bare-permalink-first file-subject fallback.

`api/` separates pending creation, returned review/comment identities, body updates and final COMMENT submission. The batch deliberately omits `event`; the submit request contains it. Returned IDs are fixture associations, not IDs manufactured by the product. GraphQL node strings are opaque placeholders: production code uses the server-returned node ID for the created review and verifies exact membership. The optional file-thread operation must be qualified on the connected host before use. The standalone file-comment payload is a separate endpoint shape and must not be inserted into REST `comments[]`.

The final index body is supplied at review submission, so an extra pending-summary PUT is not mandatory. A host may need the fixed, qualified GraphQL comment-edit route instead of REST PATCH while pending; record that exact capability rather than assuming it from this fixture.

`ai/` contains transport-independent captured requests/results. Responses cannot set target/range/readiness/publication. The existing BPR agent envelope binds request identity. `text-transactions.json` defines raw-byte selection examples; it does not replace actual Qt editor tests.

`github-route.json` is a local **test fixture only**. It is never published as hidden metadata, is not a shared-tour manifest, and is not needed by a website reviewer or cold import. The normal comments/index carry the route.
