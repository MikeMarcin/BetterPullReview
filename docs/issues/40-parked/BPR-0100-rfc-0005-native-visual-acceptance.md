---
id: BPR-0100
title: Run the inherited native visual-acceptance pass
status: Parked
rfc: RFC-BPR-0005
depends_on: [BPR-0097, BPR-0186, BPR-0187, BPR-0233]
parked_on: awaiting-owner-verdict
---

# Run the inherited native visual-acceptance pass

## Final-state correction from BPR-0225

BPR-0187 now serves the recorded UIforETW pull request. What remains before
this attended pass is BPR-0233: the recording is not yet driven through a real
selected-file diff path, and the current public test's saved window is not
asserted to contain a comparison or intraline content. Attendance must judge a
known rendered review state, not merely a window that saved successfully.

The RFC-BPR-0003 and RFC-BPR-0004 ledgers both defer the integrated-workbench presentation-polish and native visual-acceptance pass to RFC-BPR-0005. This card owns it.

On the real macOS composition with native Cocoa and QMacStyle, inspect the assembled workbench for presentation quality at supported window sizes and scales, with readable contrast and correct native control rendering.

DEFERRED CLASS: owner-attended. It needs a person at the keyboard to authorise and observe a live appearance context and to judge visual acceptance, which automation cannot assert. Batch it with the other attended checks. It does not block any other RFC-BPR-0005 card.

## Two dependencies were added after this card was written

Attendance is necessary and was not sufficient. BPR-0186 records that no launch
of this application can be looked at by a person against fixture data — a
fixture launch schedules an automated run and exits, and a launch with no
fixture takes the real credentialed path. There was no window to inspect.

BPR-0187 records the second half. The existing fixture is hand-written payloads,
which is sound for driving a deterministic run and weak ground for judging
presentation, because the content was written to satisfy the code that reads
it. The product owner designated `microsoft/DirectXMath#336` for this, and it
reads unauthenticated.

Judge the assembled workbench against the recorded review, not against
synthetic content.

## An attended run happened on 2026-08-06 and did not satisfy this card

The product owner was at the keyboard and authorised a live appearance change.
`--native-appearance-probe` ran on a real Cocoa/`QMacStyle` display, Light →
Dark, and passed every marker — the first time it has ever passed anywhere,
because its automated contract can only prove it *refuses* offscreen.

That run does not close this card, for a reason worth stating plainly: it
inspected `github_window`. This card asks for the **assembled workbench**, and
the rendered evidence corroborated BPR-0188 visually — no drawers, no primary
navigation toolbar, no files or conversations panels appear, matching
`workbench_window` reporting zero symbols in the shipped binary.

What the run did establish, and what is worth keeping:

- The composed window renders natively under `QMacStyle` and follows a real
  operating-system scheme change: window, epoch control, and diff workbench
  palettes invert, the diff font is unchanged, and geometry rebuilds exactly
  once.
- Two defects that the probe's own assertions do not cover, now carded as
  BPR-0194: the selected line measures 1.11:1 against its background in Light,
  and a menu button's dropdown indicator renders outside its frame.

Both were found by a person looking at the captured rasters after every
automated marker had reported OK. That is the argument for this card existing
at all, and it applies again once the shell is the window.

## Packet prepared — 2026-08-18

Every dependency is in `60-done/`: the attended launch exists, the recording
is served, and BPR-0233 drives it through the real selected-file path with
the v006 batch accepted. What remains is exactly this card's class: a person
judging presentation on a real display. The digest-bound procedure is
`docs/verification/reviews/native-visual-acceptance-v001/operator-packet.md`,
binding the same dev executable as the two other pending packets, with
`application_attended_launch_contract` and the recorded selected-diff
contract green at those bytes as preflights. The card parks awaiting that
judgment.

## Attended result — rejected, 2026-08-18

The owner ran the pass, reached the real C++ comparison through the Files
drawer, and rejected on a defect no automated marker had caught, asking in
their own words:

> How come there's no syntax highlighting?

`UIforETW/UIforETWDlg.cpp` renders monochrome in the dark profile while
`SYNTAX_CLASSIFIER_COMPOSED_OK` stays green — the defect is recorded and
root-caused as [[BPR-0390]], a delivery race between span adoption and the
diff projection that no contract observes. This is precisely what the pass
exists to produce; the rejection is evidence, not failure. The card stays
parked: the packet is re-presented against repaired bytes after BPR-0390
lands, and no acceptance is inferred from the aspects the owner did not
reach.

## Re-presented against repaired bytes — v002, 2026-08-19

[[BPR-0390]] repaired the defect this card's rejection found. Adoption now
happens at display time against the comparison the surface is actually showing,
rather than depending on which of two unordered deliveries landed second. On the
recorded course the left pane went from zero token spans to 1,361, and
`application_uiforetw_selected_diff_contract` now fails outright if the selected
comparison renders unclassified — the check the rejection proved was missing.

The live packet is
`docs/verification/reviews/native-visual-acceptance-v002/operator-packet.md`,
bound to the bundled executable
`sha256:4e93884097bc92d4c56eb8f12712245ed4e0c2b173e1bed7a441cc8072e20c64` at
51,050,832 bytes. v001 is preserved as history, and it had two independent
reasons it could no longer be run: its digest is stale, and the path it names
(`build/dev/src/app/better_pull_review`) stopped being a file when [[BPR-0392]]
made the tests-enabled macOS build an application bundle. The v002 launch
command is the corrected one.

Two things are stated in the packet rather than left for the run to discover.
The syntax criterion is now named explicitly in the judgment list, because it is
what the previous pass rejected on. And one blob is known to classify late —
`UIforETW.rc`'s right-hand side loses a race with the automated course's own
shutdown, which a person at the keyboard does not — so a pane still uncoloured
after a second or two is named as a finding worth rejecting on rather than
something to wait out.

The preflights pass at these exact bytes: `application_attended_launch_contract`,
`application_uiforetw_selected_diff_contract`,
`application_diff_workbench_public_path`,
`application_workbench_composition_contract`, and `qt_diff_workbench_scale_100`,
5/5. The same executable backs the pending BPR-0114/BPR-0220 and BPR-0168
packets, so one attended session can serve all three.

The card stays parked awaiting that judgment.
