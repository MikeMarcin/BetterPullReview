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

## Rebound once more, and this time the bytes are settled — 2026-08-19

[[BPR-0397]] bounded three unbounded waits in the epoch host and rebuilt the
executable, so the previous packet's digest no longer matched anything on disk.
Third rebuild in a row to invalidate a pending packet; caught before an operator
hit it rather than after.

The live packet is
`docs/verification/reviews/native-visual-acceptance-v003/operator-packet.md`,
bound to
`sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93` at
51,085,872 bytes.

Why this one should hold where three did not: every card that could change these
bytes is in `60-done/`, and nothing in an active lane is executable without a
Windows or Linux host, a product decision, or an owner verdict. The bytes are
settled because the board is.

The preflights are stronger than any earlier version's, too — the complete
193-test `dev` matrix passed at exactly these bytes rather than a focused
selector set.

The card stays parked on the operator.

## Operator direction — launch it or hand over the command — 2026-08-20

Recorded verbatim:

> I will judge if launched or given launch command

The judgement stays the owner's, which is correct and unchanged — this is
RFC-BPR-0005's native visual acceptance and no command can hold it. What the
direction settles is that getting to the point of judgement is not the owner's
work. Either the application is launched for them, or they are handed the exact
line to run.

The specific thing to look at first is on the record from the last rejection:

> "How come there's no syntax highlighting?"

so syntax classification on a real file is the first check, not a general
impression.

The packet's bound binary is unchanged, so a launch now is a launch of exactly
the bytes `native-visual-acceptance-v003` describes.

## Accepted against the bound native executable — 2026-08-20

The owner closed the application normally after the attended pass and returned
the packet's acceptance statement byte-for-byte:

> I ran the BPR-0100 native visual-acceptance pass against executable
> sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93
> on a real macOS display under Cocoa/QMacStyle, inspected the recorded
> UIforETW review through the real selected-diff path at supported window
> sizes and appearance profiles, saw the C++ comparison rendered with syntax
> colouring in both panes, and I accept the assembled workbench's
> presentation quality.

The immutable response is
[`operator-response.md`](../../verification/reviews/native-visual-acceptance-v003/operator-response.md),
and its exact-statement and artifact binding is recorded in
[`validation-record.json`](../../verification/reviews/native-visual-acceptance-v003/validation-record.json).
The record binds the reviewed packet at
`sha256:58b44c8d870173a8ec1564b96f1c1b166747a063adf172bc1608f12e9f7cc1d1`,
the response at
`sha256:d849ba890499a71e0110368c3f9fe84e10492e1ca164ab46299cbb504f5d91a2`,
and the executable at the accepted `sha256:7b9d6b…fcd93` identity.

This verdict is only BPR-0100's native presentation acceptance. In the same
response the owner said they were not looking for a notification banner, so no
BPR-0168 observation or rejection is inferred. The owner separately authorized
replacing the protected executable for BPR-0409, BPR-0410, and BPR-0334; that
direction releases the byte hold but does not widen this visual verdict.

Because that authorized batch relinks the exact executable the verdict binds,
this card remains Parked as the owning final-head attended procedure. v003 is an
accepted, frozen result rather than a pending question; after the shared batch,
only a successor packet bound to the materially changed executable can close
the card. No part of the accepted v003 record is rewritten or downgraded.

## The final-head successor is v004 — 2026-08-21

The authorized BPR-0409/BPR-0410/BPR-0334 relink batch is complete at committed
source HEAD `145f6d4c6345c65c42c6d5ea00dfc9b67fa514b0`. The live final-head judgment is
[`native-visual-acceptance-v004`](../../verification/reviews/native-visual-acceptance-v004/operator-packet.md),
bound to executable
`sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28`.
The accepted v003 response and validation record remain immutable and valid for
their original bytes. This card remains parked until the owner accepts or
rejects the new exact identity.
