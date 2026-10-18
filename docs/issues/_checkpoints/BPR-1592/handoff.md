# BPR-1592 handoff: accepted and landed

- Branch `bpr-1591-notes-margin`; product head `c2487b258`. Not on `main`.
  BPR-1591 (the line anchor: core `place_note`, ledger schema 10's
  `private_note_line`) landed on `main` at `4d0e58539`.
- v001: Notes is the private notes as margin notes beside the code
  (`private_notes_panel` rewritten): dashed lock cards at their lines, open in
  place to write (blue edge), PR chip at the top, whole-file and other-file
  chips at the bottom, line chips for notes out of view, outdated notes after
  the placed ones with their reason, N on the selected line, Save Cmd+Return,
  Esc twice to discard, Draft comment at the note's own line (the host selects
  the line and promotes once its draft is the selected one). Gutter dashed
  cells; a noted line's mark opens its note. The agent's panels, private
  history and findings moved unchanged to a provisional Agent tab (spark icon)
  after Tour; below 340 pixels the tray's tabs show icons only. The shared
  drawer card now edges a private note in the writing blue while written.
  `--attended-notes-look` writes three notes through the tab's gestures.
- Verification (focused): qt_private_notes_panel (Light and Dark, margin,
  chips, open in place, Cmd+Return, Esc twice, N, Draft comment availability,
  elsewhere list, keyboard, scope reset), qt_workbench_window_scale_{100,125,200}
  and the whole window binary, qt_hide_drawers_native_menu_key,
  qt_drawer_parts_scale_*, application_review_submission_composition_public_path
  and scales (real app: PR note, line note with path and line in the ledger,
  refusal, discard, delete, Agent tab, Draft comment at the note's line with no
  GitHub mutation, restart keeps the line), application_start_review_contract,
  core_note_placement, sqlite ledger storage, every application composition
  contract (round-trip resealed), architecture_boundaries, coding_format_check,
  documentation gate.
- Owner look: frozen root `/Users/jaeger/bpr1592-notes-margin-v001.nmlvh99c`
  from `c2487b258`, executable SHA-256
  `b0371a5cb51d848e4c83240dc9c0ad1380b4d5a1888361ab6496a95374a3f694`, check
  reached `ATTENDED_NOTES_SHOWN`. Session `23a47c6b542ce5e92f47a335594b3a42`,
  state `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1592-notes-margin-v001`,
  candidate `BPR-1592-notes-margin-v001` (fingerprint `8a34ecb4…`), ready at
  sequence 3; host PID 87346.
- Owner accepted v001 with no note, decision
  `1d30aced-1d77-4ee1-a30f-3c4f29e90d2c` (session cursor 5). Landed with no
  new commits on `main` since BPR-1591, so the focused set and contracts run
  on `c2487b258` stand. Host PID 87346 stopped; frozen root removed.
- Follow-up asked by the owner's coordinator: the mouse wheel over Notes
  scrolls the code, as over Comments; done with BPR-1593.
