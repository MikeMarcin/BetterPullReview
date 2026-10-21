---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-L2"
title: "Read a filtered diff's line facts and publish a placed comment through the existing admission"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-PENDING-L1","BPR-1687"]
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"milestone":"RFC-BPR-0089 M4","related_cards":["BPR-PENDING-L1","BPR-PENDING-L3"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_review_mutation_application_host|application_review_mutation|github_mutation_contract|sqlite_review_mutation_recovery_storage|github_contract_fixture_security|github_recorded_fixture_contract)$'","detail":"The mutation host repeats the core's placement from the source observation's facts before any effect: a line unchanged at the head is bound at the head's line, a line the head changed on C1 (RemoteLine and Commit); a line GitHub resolves on no commit, a commit outside the accepted chain, lines not yet read, a filtered diff without its facts and a reply whose right side is not the head are unsupported with nothing submitted (negative controls). valid() and the coordinator admit a commit other than the head only for a right-side placed line; the provider sends it natively at its commit and line, with no range, reply or file-comment fallback, and recognises GitHub's outdated answer (line null) by original_commit_id and original_line. Someone else's outdated comment in a reconciliation list is a well-formed comment that is not ours. The recovery journal writes record version 3 for a placed child and the legacy table refuses one. The recorded GraphQL contract admits commit and originalCommit. macOS."}}]
---

# BPR-PENDING-L2: Read a filtered diff's line facts and publish a placed comment through the existing admission

RFC-BPR-0089 M4, the host and provider half of owner decision Q1 and RFC
AC07: "Both reconcile once and weaken no admission check (negative control: a
commit outside the pull request is refused before any write)."

## Change

- **Epoch host.** `resolve_filtered_comment_lines` reads, under the filter
  authorization the touches job set, the merge base's, the diff's sides' and
  the head's blobs (retained cache or a sparse review's chain store, bounded at
  8 MiB a side) and computes the four correspondences with the review's diff
  engine. A refresh that keeps the review and its head keeps the filter
  authorization under the new snapshot (negative control: without it the
  reread after a publication refuses every filtered read).
- **Mutation host.** `review_mutation_source_observation::Filtered` carries the
  facts; `validate` places the comment through the core and binds
  `RemoteLine` (and an earlier `Commit`) with `legacy_line` placement.
- **Contract and coordinator.** `admitted_anchor_commit`; `remote_line`.
- **Provider.** The mutation body and capability key use the remote line;
  `submit_publication` admits a placed line only from a filtered source whose
  right commit it names; GitHub's outdated answer is matched by its original
  commit and line.
- **Journal.** Record version 3 keeps the side and the remote line.
- **Private publication.** The lineage admits the same commits.

## Acceptance

- **AC1.** The negative controls refuse before any write; a placed comment
  reconciles once.
