---
schema: "implement-rfc.card.v1"
id: "BPR-2187"
title: "The agent setup note, Explain this file note and finding promotion take Local identities from the store"
status: "Done"
rfcs: ["RFC-BPR-0127"]
depends_on: ["BPR-2183"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2187--BPR-2187--944ce7a0a99c.json","origin":"follow-up to BPR-2183 (critical architecture review of 03aae05cb, N4): the same race in next_private_note_id()","criteria":["AC01"]}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"273377ff9","evidence":{"kind":"command","detail":"application_agent_invocation_host test_a_promoted_finding_reaches_only_the_note_authority (an unnamed promoted note takes the store's next identity); application_review_ledger_host test_a_new_note_takes_its_identity_from_the_store; application_local_comments_composition_public_path; next_private_note_id() is gone from src/"}}]
---

# BPR-2187: The agent setup note, Explain this file note and finding promotion take Local identities from the store

Part of [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md); a
follow-up to [[BPR-2183]]. That card moved Save's and Keep local's new Local
comments to identities the store chooses (N4 of the critical architecture
review of `03aae05cb`). Three more writers had the same race:
`next_private_note_id()` minted `max + 1` from the window's own snapshot for
the agent setup note (`window_agent_setup.cpp`), the Explain this file note
(`window_explain_file.cpp`) and a finding kept local (`window_agent_chat.cpp`,
through `promoted_note`). Two windows could mint the same identity.

## Acceptance

- **AC01:** Each of the three writes a new Local comment with identity zero,
  and the store gives it the next free one in the transaction that writes it.
  `application::valid_new` accepts a new comment whose identity is zero (and
  is otherwise `valid`), and both the review ledger host and `promoted_note`
  use it. `next_private_note_id()` is removed. Each caller already answers
  with the stored identity.

## Notes

Nothing the owner sees changes, so there is no look.

## Delivered

`273377ff9`: `valid_new` in `review_ledger_port.hpp` and its use in the host
and `promoted_note`; the three callers write identity zero;
`next_private_note_id()` removed. `application_agent_invocation_host`,
`application_review_ledger_host` and
`application_local_comments_composition_public_path` pass.
