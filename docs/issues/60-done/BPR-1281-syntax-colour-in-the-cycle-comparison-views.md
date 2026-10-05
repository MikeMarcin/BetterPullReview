---
schema: "implement-rfc.card.v1"
id: "BPR-1281"
title: "Syntax colour in the cycle's comparison views (All cycle changes and the rest)"
status: "Done"
rfcs: ["RFC-BPR-0061"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner note on BPR-1182-cycle-bar-floor-v001 (attended session cbd48b91, event 950, Accept)","reservation":"docs/issues/_reservations/BPR-1280--BPR-1282--5bcc90f2edc8.json","related_cards":["BPR-1182","BPR-1163","BPR-1157","BPR-1161"],"acceptance":["BPR-1281:history","BPR-1281:colour","BPR-1281:exact","BPR-1281:boundary","BPR-1281:owner-look"]}
acceptance: [{"criterion":"BPR-1281:history","outcome":"pass","revision":"9a3e8e99d","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"git log -S 'Comparison->setPlainText': introduced plain by 48ba9741d (BPR-1163); BPR-1182's 3ac3d0f1f and f57c828ac changed no comparison or syntax line. Never had colour."}},{"criterion":"BPR-1281:colour","outcome":"pass","revision":"9a3e8e99d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"application_review_cycle_desktop_public_path (real composition, packaged classifier): All cycle changes R1 to R2 paints the removed row's return/check in control-flow/function colours (pixels, none in plain text colour) and the added row's layout carries the same role formats after its line numbers; render desktop-cycle-compare-syntax. Negative control without the classification request fails."}},{"criterion":"BPR-1281:exact","outcome":"pass","revision":"9a3e8e99d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"The rows' text, numbers and signs are unchanged (the course's '+ \\treturn aText ? check(aText) : 0;' check); bands in the panes' diff fills; a classification is adopted only for a side's exact text; rows in view only, unchanged rows not laid out again."}},{"criterion":"BPR-1281:boundary","outcome":"pass","revision":"9a3e8e99d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"build/dev at c1cd6b585: 68/68 cycle, send, share, suggestion, agent-launch, round-trip (pin unchanged) and architecture_boundaries rows, 77/77 agent/composed/workbench rows; 61/61 again at aa6a47104; agent-free build/dev-tour-cli 9/9 incl. two-round course, round-trip contract, composed_application_hosts, test_network_isolation_manifest, architecture_boundaries; clang-format clean; clang-tidy nothing on changed lines. Linux and Windows are station requests. Linux bpr-main-linux passed 8/8 view and send rows at 126940be4 (d7ed57b8f), Ctrl+Enter label included; Windows owed, not gating."}},{"criterion":"BPR-1281:owner-look","outcome":"pass","revision":"aa6a47104","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner Accept, empty note: BPR-1281-compare-syntax-v001 in session 2dc9fa2078312808fb2b48afcdd8ee0a, decision 8bf335b8-fc27-4ba8-af84-cf13b637d498, event 11 (02:45:56Z 2026-10-05), on the frozen root /Users/jaeger/bpr1280-notes-review-v001.APXGtG (git archive of aa6a47104, executable a05b45db...). Handed back at event 13; released by candidate name at events 14 to 16."}}]
---

# BPR-1281: Syntax colour in the cycle's comparison views (All cycle changes and the rest)

## Origin

The owner accepted `BPR-1182-cycle-bar-floor-v001` (RC61-09) with this note,
quoted exactly, and a screenshot of All cycle changes R1 → R2 on
`src/parser.cpp` in plain text:

> Bar width is okay but why did we lose syntax highlighting on all cycle cahnges?

## Outcome

Find whether the comparison view lost its colours or never had them, and
say which with the commits. Then colour every cycle comparison (All cycle
changes, This round, Since my last review and Choose revisions all share
one pane) through the shared classifier and painter path that the source
pane and the share sheet already use: the packaged classifier through the
desktop's syntax host, and `code_sample_painter`'s format ranges and role
colours. No second classifier and no colour chosen outside the role table.

## Acceptance

- **BPR-1281:history.** The cause is recorded with the commits that show it.
- **BPR-1281:colour.** A rendered comparison, through the real composition,
  paints keywords, comments and strings in the role colours on both removed
  and added rows. A negative control without the classification shows plain
  text and fails the check.
- **BPR-1281:exact.** The text, line numbers, signs and row bands are
  unchanged; a stale classification (of other bytes) is never applied.
- **BPR-1281:boundary.** The view rows, the desktop course and
  `architecture_boundaries` pass; the round-trip pin is unchanged.
- **BPR-1281:owner-look.** The owner's Accept/Reject on a frozen build.

## Result (2026-10-04): done unattended; the owner's look pending

Never had it: the comparison pane has been plain text since `48ba9741d`
([[BPR-1163]]), and BPR-1182's `3ac3d0f1f` touched none of its lines. Landed
as `9a3e8e99d` (candidate `aa6a47104`): both sides are classified by the
desktop's code syntax host and the rows in view painted through
`code_sample_painter`, in the panes' diff fills. The desktop course checks
pixels and formats on both rows and fails without the classification.
Owner question `BPR-1281-compare-syntax-v001` in session `2dc9fa20…` (ready
at sequence 9). Station requests and the full record are in the ledger's
"BPR-1280, BPR-1281 and BPR-1282" section of
[RFC-BPR-0053-0061.md](../../verification/RFC-BPR-0053-0061.md).

**Note: Linux run at `126940be4` (holds `126940be4`; station `bpr-main-linux`,
WSL2, GCC 15.2.0, Qt 6.11.2, offscreen; no remote mutation).** The `dev`
build succeeded with no warnings and 8 of 8 passed:
`qt_review_cycle_view_1_0` and `_2_0`,
`application_review_cycle_desktop_public_path`,
`application_review_cycle_send_public_path`, `review_cycle_send`,
`core_review_cycle_send`, `application_review_round_trip_composition_contract`
and `architecture_boundaries`.

The view rows check that Save note and Save suggestion end with
`QKeySequence(Ctrl+Enter).toString(NativeText)`. A separate probe against the
same Qt printed that as `Ctrl+Enter` on Linux. No pixel check failed at 1x or
2x, so there are no counts to report.

## Owner verdict and Done (2026-10-04)

Session `2dc9fa20…` was handed back at event 13 and read with
`status --after 9`. `BPR-1281-compare-syntax-v001` is Accept with an empty note (decision
`8bf335b8-fc27-4ba8-af84-cf13b637d498`, event 11), on the frozen root
`/Users/jaeger/bpr1280-notes-review-v001.APXGtG` (`aa6a47104`). Released by
candidate name at events 14 to 16; the session's host is stopped. Linux
passed at `126940be4`; the Windows station run is owed and not gating, since
no criterion of this card is Windows-only. The full record is the ledger's
"three owner Accepts, and Done" section of
[RFC-BPR-0053-0061.md](../../verification/RFC-BPR-0053-0061.md).
