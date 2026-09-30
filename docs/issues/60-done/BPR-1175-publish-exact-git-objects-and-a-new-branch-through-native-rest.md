---
schema: "implement-rfc.card.v1"
id: "BPR-1175"
title: "Publish exact Git objects and a new branch through native REST"
status: "Done"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1173", "BPR-1174", "BPR-1147"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-04", "RC59-05"], "pack_alias": "PACK-CYCLE-33", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC59-04","outcome":"pass","revision":"3419781ad510473a82511dc41943518b17f8cb9e","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share, macOS arm64 github_publication_contract and review_cycle_publisher: SHA-256 candidates refused as object_format with zero writes, workflow files protected_path, blobs over 128 KiB blob_too_large, gitlinks not compiled, bytes not hashing to the named blob refused before any request, a server identity other than the local one refused and marked possibly stored; modes travel exactly. Local preparation share is BPR-1173's."}},{"criterion":"RC59-05","outcome":"pass","revision":"3419781ad510473a82511dc41943518b17f8cb9e","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share (objects and new branch): the session refuses other hosts, accounts and the GHES contract before any request; native_https_transport_fixture publication-course sends exact repository/parent/base/new-branch/blob/tree/commit/ref requests through the production transport and admits 404 and 422-exists as observations, never an update; no update/delete/force/fork request exists and installed Git has no push; review_cycle_publisher reads capability first (zero writes on every limit), journals each step, recovers by reading and never overwrites or adopts a foreign branch. PR creation is BPR-1176's share; live evidence is RC59-10's and not run."}},{"criterion":"BPR-1175:boundary","outcome":"pass","revision":"3419781ad510473a82511dc41943518b17f8cb9e","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"133 focused ctest rows pass on macOS arm64 after a full -k 0 dev build (only BPR-1211's test_scoped_writing_agent fails), including every native HTTPS loopback scenario, github contracts, architecture_boundaries and test_network_isolation_manifest with the new pin; clang-format and clang-tidy clean. Linux and Windows runs owed with the selectors on the card."}}]
---

Pack alias: **PACK-CYCLE-33** (live `BPR-1175`, allocated at import).

# BPR-1175: Publish exact Git objects and a new branch through native REST

## Outcome and bounded work

Add only the admitted blob/tree/commit/new-ref operations to the GitHub provider through existing native HTTPS and current host/account policy. Verify expected identities and tree/parent/metadata, required permissions and non-overwriting new-ref behavior.

## Acceptance and direct evidence

Isolated provider fixtures for each request/result, permission/protected-path/version/ref-conflict/identity mismatch, quotas/rate limits and unknown host. Keep installed Git credential-free; actual designated live evidence is separately authorized.

Criteria: RC59-04, RC59-05.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `3419781ad`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^github_publication_contract$|^native_https_transport_fixture$|^review_cycle_publisher$|^core_review_cycle_publication$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`):
`ctest --test-dir build\dev -R "^github_publication_contract$|^review_cycle_publisher$|^core_review_cycle_publication$|^test_network_isolation_manifest$" --output-on-failure`
(measure the new `review_cycle_publisher` pin contained).

## Live evidence

Not run. No designated safe GitHub target exists; RC59-10's live course is BPR-1176's and BPR-1178's and stays owed.
