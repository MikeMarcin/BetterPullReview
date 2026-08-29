---
id: BPR-0477
title: Repin the app source digest the palette instrumentation drifted
status: Done
rfc: RFC-BPR-0008
depends_on: []
completed: 2026-08-28
---

# Repin the app source digest the palette instrumentation drifted

Found by BPR-0101's clean-state closeout matrix, exactly the way
[[BPR-0452]] and [[BPR-0453]] were found: a full run a focused slice had no
obligation to make. [[BPR-0475]]'s instrumentation commit (`3309687d`)
edited `src/app/main.cpp` — the palette-identity failure message now names
the observed pixel — and the review-round-trip composition contract pins a
manifest digest over the production application source contents
(`application_review_round_trip_composition_contract.cmake:147`), which no
selector in that card's focused set matches. The matrix failed 216/218 with
both failures this one stale pin:
`application_review_round_trip_composition_contract` directly and
`application_composition_contract` through its
`test_async_mutation_host_inventory_rejects_every_direct_bypass` baseline.

The repin sets the expected manifest digest to
`96766c9463c371eba1be95360659ba5ef7ebdad9133a13109649f6195f581aef`, the
value computed from the committed tree at a clean worktree — the same bytes
every other passing contract ran against. Both contracts rerun green at the
repinned state; the evidence is on [[BPR-0101]]'s closeout record.

The selector-trap lesson already written into the board's
verification-scope rule gains one more concrete instance: a diff touching
`src/app/` must run the composition digest contracts, because no
subsystem-named selector matches them.
