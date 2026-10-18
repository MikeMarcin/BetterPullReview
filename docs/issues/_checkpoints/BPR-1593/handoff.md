# BPR-1593 handoff: v001 under owner review

- Branch `bpr-1593-tours`; product head `eb4990c98` (with `4f0d4b69d`, the
  owner's follow-up: the wheel over Notes scrolls the code). Not on `main`.
  BPR-1592 landed at `7866d902b`; BPR-1603 (pull-request comments at the
  Comments drawer's edge) was allocated and RFC-BPR-0085 amended (D8, AC10,
  conversation-chips milestone) at `f8d3e31a9`.
- v001: `review_tour_editor` has two modes over one set of controls. Route
  (Tour tab, layout A): brief card with rows opening in place; outline drawn
  as the route (`route_delegate`: numbered rings on an amber line, file chips,
  readiness in the ring, Needs attention's warning on the ring); the current
  stop's fields in place as the row's item widget; foot with the draft state
  and Preview and publish. Page (layout C, `workbenchTourPage` in the centre
  stack, drawers hidden through the full-page rule): brief and outline left,
  stop with code excerpt centre, preview and publication right; Back to the
  code / Esc. Section tabs hidden (principle 1); indices 0-3 kept. Stop fields
  are detached from the route only when rows change, so saves keep focus.
  `--attended-tour-look` writes a brief and three stops.
- Known gaps vs the mockups, said in the session: code selection highlight
  instead of an amber wash; Ready, next not the blue pill; right column
  previews the whole tour; related sources a list.
- Verification (focused): every `tour` ctest row run serially (30 with
  round-trip and format), qt_workbench_window_scale_{100,...} and the whole
  window binary (new `test_the_tour_is_a_route_expanding_to_its_page`),
  qt_private_notes_panel (wheel forwarding), qt_scoped_prose_editor,
  qt_hide_drawers_native_menu_key, review submission composition public path,
  start review contract, every application composition contract (round-trip
  resealed), architecture_boundaries, coding_format_check, documentation gate.
- Owner look: frozen root `/Users/jaeger/bpr1593-tours-route-v001.fmh3hj33`
  from `eb4990c98`, executable SHA-256 `6c2d9dc0…91f8`, check reached
  `ATTENDED_TOUR_SHOWN`. Session `9a9c8a15076be0d2dbf02051bf3e0b6b`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1593-tours-route-v001`,
  candidate `BPR-1593-tours-route-v001` (fingerprint `16016d41…`), ready at
  sequence 3; host PID 21499.
- Next: read `status --after 3`. On Accept, merge `main`, focused set plus the
  composition contracts, reseal, docs gate, finish BPR-1593, push, stop PID
  21499, remove the frozen root; then BPR-1594 (Agent tab).

## Landing continuation — 2026-10-18

Owner authorized BPR-1593 as an explicit exception to the older exclusion.
Session intake consumed through sequence 5: Accept at sequence 4, no note,
then handed back. The session belongs to the primary checkout. Main
`a7a45e1c9` merged without conflict; all three round-trip pins match.
BPR-1594 and BPR-1603 remain outside this work. All 44 focused rows pass serially, including every tour row, all window
scales, Notes, the submission path and composition contracts. Format22,
documentation and diff checks pass. Protection released at sequence 6.
The inherited accepted design is being landed without visual edits; the
frontend-design skill is unavailable in this harness. No new design is made.
MASK preflight reports project.not_registered for this worktree; the existing
direct continuation and board helpers remain in use.
