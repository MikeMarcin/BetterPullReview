---
schema: "implement-rfc.card.v1"
id: "BPR-1279"
title: "Offer an explicit rebind for a moved or replaced cycle folder in the terminal and the window"
status: "Done"
rfcs: ["RFC-BPR-0054"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1183 recovery corpus","reservation":"docs/issues/_reservations/BPR-1279--BPR-1279--b3893733ba1f.json","related_cards":["BPR-1183","BPR-1148"]}
acceptance: [{"criterion":"BPR-1279:AC1","outcome":"pass","revision":"36edd89a1953dbf2e46e1cf4fcfa3beef3258f68","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 build/dev-tour-cli: cycle_cli_public_path and application_review_cycle_recovery_corpus drive the shipped bpr: rebind without --confirm is interaction_required (exit 3) with the material check, a 16-character code and the full Next; the exact code binds through bind_cycle_workspace (binding 2, captures unchanged) and the next capture reads the new location; a repeat is rebound:false; in a pty the word rebind binds. Linux and Windows station runs owed with the commands in the ledger."}},{"criterion":"BPR-1279:AC2","outcome":"pass","revision":"36edd89a1953dbf2e46e1cf4fcfa3beef3258f68","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"application_review_cycle_recovery_corpus through the real desktop (offscreen): a refused capture puts Rebind Folder on the review bar; the folder is named in the application's own dialog; the window-modal confirmation shows the check; Rebind checks again and binds, Capture revision then reads the folder. review_cycle_workbench and qt_review_cycle_view cover the host flow (a changed folder is shown again before binding) and the presentation (bar only when offered, menu always, Rebind only when confirmable, Cancel and Escape)."}},{"criterion":"BPR-1279:AC3","outcome":"pass","revision":"36edd89a1953dbf2e46e1cf4fcfa3beef3258f68","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Another cycle's folder is conflict/bound_elsewhere in the terminal and offers no Rebind in the window; no code, a wrong code, a stale code after the folder changed, a check alone and Cancel bind nothing; a file for a folder is invalid_input; --yes and --force are unsupported_option. Negative controls: binding without a code fails cycle_cli_public_path; ignoring another cycle's root fails core_review_cycle_workspace and cycle_cli_public_path."}},{"criterion":"BPR-1279:AC4","outcome":"pass","revision":"36edd89a1953dbf2e46e1cf4fcfa3beef3258f68","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"A replaced root's capture refusal says to rebind the cycle or move the folder back, and a root moved away says it is no longer at its location, in the terminal (Next: bpr cycle rebind RC-n --to PATH) and in the window (same words from review_cycle_words, with Rebind Folder offered); asserted by the corpus, the CLI course and review_cycle_workbench."}},{"criterion":"BPR-1279:AC5","outcome":"pass","revision":"36edd89a1953dbf2e46e1cf4fcfa3beef3258f68","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"architecture_boundaries and architecture_boundary_contract pass; sqlite_tour_owner_channel round-trips rebind under protocol 5 and refuses an unknown command and a --force option; application_review_cycle_security_qualification passes with the protocol 5 greeting; cycle_cli_documentation passes against the updated cycle-cli.md (Moving a project); the reviewer guide and the domain model describe the rebind; clang-format 22 and 23 clean; clang-tidy clean on touched src units; DOCUMENTATION_OK."}}]
---

# BPR-1279: Offer an explicit rebind for a moved or replaced cycle folder in the terminal and the window

## Observation

[[BPR-1183]]'s recovery corpus showed that a project folder which is moved or
replaced can rejoin its cycle only by being moved back. The core supports an
explicit rebind ([[BPR-1148]]), but neither `bpr cycle` nor the window offers
it, so the reviewer has no way forward from the refusal.

## Outcome

Offer an explicit, confirmed rebind of a cycle to its folder's new location (or
to a replacement checkout) in the terminal and the window, through the core's
existing rebind rules, with the refusal words pointing at it. Never rebind
silently. Cover it in the recovery corpus with negative controls.

## Acceptance (authored at claim, 2026-10-04)

The card named an outcome and no criteria. These were written at the claim,
under RFC-BPR-0054's RC54-10 and the coordinator's mandate for this card:

- **AC1, terminal.** A confirmed `bpr cycle rebind` binds a cycle to its
  folder's new location, or to a replacement checkout, through the core's
  binding rules and material check (`bind_cycle_workspace`, BPR-1148), and the
  next capture reads it.
- **AC2, window.** The window offers the same rebind from the refused capture,
  shows the check, and binds only on the reviewer's Rebind.
- **AC3, negative controls.** Another cycle's folder is refused. Nothing
  rebinds without a confirmation: none, a wrong one, a stale one, a check alone
  or Cancel. A file never replaces a folder.
- **AC4, words.** The capture refusal's words, and the terminal's `Next:`,
  point at the rebind instead of a retry.
- **AC5, upkeep.** Layering and the owner-channel round trip stay green;
  cycle-cli.md (`cycle_cli_documentation`) and the reviewer guide describe it.

## Result (2026-10-04)

Promoted at `d9857a85e`, claimed at `60090666e`, landed as `9a1506282`, with
`36edd89a1` (the confirmation grows to hold its words). The decisions, every
command and its result, and the negative controls are in the
[ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1279-an-explicit-rebind-in-the-terminal-and-the-window-2026-10-04).

- Core: `admit_workspace_rebind` says before anything is asked whether a
  candidate may be offered. It is `unchanged` for the bound root at its
  location, `bound_elsewhere` for another active cycle's root, `kind_changed`
  for a file in place of a folder, and `file_scope`, `archived` or `unbound`
  for the others. `workspace_rebind_confirmation` is the 16-character code the
  reviewer confirms: a digest of the cycle, its record revision and binding
  version, the candidate as observed and the check. The binding rules
  themselves are unchanged.
- Host: `review_cycle_runtime::check_rebind_selected` opens the candidate,
  compares it with the latest capture and names any other cycle bound to it;
  it installs and changes nothing. The material check now returns the root it
  read, which is the root a rebind binds.
- Terminal: `bpr cycle rebind [RC-n] --to PATH [--confirm CODE]`. Without the
  code it is `interaction_required` (exit 3), with the check, the code and the
  full `Next:` command. A code that no longer matches is `stale_revision`,
  with the new check. In a terminal it asks for the word `rebind`. The owner
  protocol is 5.
- Window: **Rebind Folder…** appears on the review bar beside Capture
  revision when a capture is refused because the folder moved or was
  replaced, and it is always in Capture revision's menu. A window-modal
  confirmation shows the check. **Rebind** checks the folder again and binds
  only what is shown; a changed folder is shown again to confirm.
- Words: a replaced root says to rebind the cycle or move the folder back. A
  root moved away with nothing in its place now says so ("no longer at its
  location"), not "no longer exists". The terminal's `Next:` is `bpr cycle
  rebind RC-n --to PATH`.

No owner look was asked for. No criterion needs an owner judgment. The window
flow is driven through the real window by the recovery corpus. The two
renders `cycle-rebind-offered.png` and `cycle-rebind-confirm.png` come from
`qt_review_cycle_view`.

Linux and Windows station runs are owed. Their commands are in the ledger.

**Note: Linux run at `1fb3cd77d` (holds `e7ddaad63`; station `bpr-main-linux`,
WSL2, GCC 15.2.0, `build/bpr1279` with `BPR_BUILD_TOUR_CLI=ON`, offscreen,
serial; no remote mutation).** The full tree built, 2,122 steps, with no GCC
warnings (the build log has no line containing "warning"). 13 of 13 passed:
`core_review_cycle_workspace`, `core_review_cycle_command`,
`sqlite_tour_owner_channel`, `review_cycle_capture`,
`review_cycle_workbench` (63.1 s), `qt_review_cycle_view_1_0` and `_2_0`,
`cycle_cli_public_path`, `cycle_cli_documentation`,
`application_review_cycle_security_qualification` (80.8 s),
`application_review_cycle_parity_public_path` (37.4 s),
`application_review_cycle_two_round_public_path` and
`application_review_cycle_recovery_corpus` (76.5 s). `build/bpr1279` was
removed. Done is not reopened.
