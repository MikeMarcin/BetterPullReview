---
schema: "implement-rfc.card.v1"
id: "BPR-1281"
title: "Syntax colour in the cycle's comparison views (All cycle changes and the rest)"
status: "Parked"
rfcs: ["RFC-BPR-0061"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner note on BPR-1182-cycle-bar-floor-v001 (attended session cbd48b91, event 950, Accept)","reservation":"docs/issues/_reservations/BPR-1280--BPR-1282--5bcc90f2edc8.json","related_cards":["BPR-1182","BPR-1163","BPR-1157","BPR-1161"],"acceptance":["BPR-1281:history","BPR-1281:colour","BPR-1281:exact","BPR-1281:boundary","BPR-1281:owner-look"]}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1281:history","outcome":"pass","revision":"9a3e8e99d","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"git log -S 'Comparison->setPlainText': introduced plain by 48ba9741d (BPR-1163); BPR-1182's 3ac3d0f1f and f57c828ac changed no comparison or syntax line. Never had colour."}},{"criterion":"BPR-1281:colour","outcome":"pass","revision":"9a3e8e99d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"application_review_cycle_desktop_public_path (real composition, packaged classifier): All cycle changes R1 to R2 paints the removed row's return/check in control-flow/function colours (pixels, none in plain text colour) and the added row's layout carries the same role formats after its line numbers; render desktop-cycle-compare-syntax. Negative control without the classification request fails."}},{"criterion":"BPR-1281:exact","outcome":"pass","revision":"9a3e8e99d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"The rows' text, numbers and signs are unchanged (the course's '+ \\treturn aText ? check(aText) : 0;' check); bands in the panes' diff fills; a classification is adopted only for a side's exact text; rows in view only, unchanged rows not laid out again."}},{"criterion":"BPR-1281:boundary","outcome":"pass","revision":"9a3e8e99d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"build/dev at c1cd6b585: 68/68 cycle, send, share, suggestion, agent-launch, round-trip (pin unchanged) and architecture_boundaries rows, 77/77 agent/composed/workbench rows; 61/61 again at aa6a47104; agent-free build/dev-tour-cli 9/9 incl. two-round course, round-trip contract, composed_application_hosts, test_network_isolation_manifest, architecture_boundaries; clang-format clean; clang-tidy nothing on changed lines. Linux and Windows are station requests."}},{"criterion":"BPR-1281:owner-look","outcome":"pending","revision":"aa6a47104","evidence":{"kind":"freeform","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner Accept/Reject BPR-1281-compare-syntax-v001 in session 2dc9fa2078312808fb2b48afcdd8ee0a (ready at sequence 9) on the frozen root /Users/jaeger/bpr1280-notes-review-v001.APXGtG (git archive of aa6a47104, executable a05b45db...)."}}]
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
