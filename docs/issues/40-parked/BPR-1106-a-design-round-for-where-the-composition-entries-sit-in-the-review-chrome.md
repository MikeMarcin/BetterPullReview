---
schema: "implement-rfc.card.v1"
id: "BPR-1106"
title: "A design round for where the composition entries sit in the review chrome"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin":"owner note on attended candidate BPR-0653-entry-row-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision 007a6eb5-adf3-427c-8473-547f9d0c4d9e, 2026-09-24","origin_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0037"],"related_cards":["BPR-0653","BPR-0929","BPR-0940","BPR-0795","BPR-1036","BPR-1021","BPR-0737"],"reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json","disposition":"standalone follow-up; not an RFC-BPR-0028 closeout dependency","owner_decision":{"date":"2026-09-25","session":"7083474240820608cdf9ff551ddd298f","candidate":"BPR-1106-placement-v001","decision":"ece7a6f3-3e54-459f-9c41-eba06732a91d","selected":"Other: discuss outside the attended review flow in a separate session"}}
parked_on: "owner-design-discussion"
acceptance: [{"criterion":"AC1 The design page shows at least three placements with the current row as the baseline, each with its stated costs, rendered and inspected headless at both sizes in both appearances","outcome":"pass","revision":"4e9037e6","evidence":{"kind":"artifact","detail":"docs/design/BPR-1106-composition-entries-v1.html: five placements on the accepted RFC-BPR-0028 full review surface (A the current row as baseline, B the strip's review menu, C the Updates line's right end with the readiness wording in its track, D the conversation drawer's foot, E a Compose control in the strip), each with its height from the comparison, Windows strip width at the 902 floor, clicks to each entry, keyboard reach and readiness line, drawn to scale and side by side. 50 frames from docs/design/BPR-1106-composition-entries-frames-v1.html (every placement at 1280x852 and 1920x1080, light and dark, macOS and Windows chrome, plus loading at 1280x852 on macOS in both appearances) and 8 close-ups under docs/verification/BPR-1106-composition-entries/, rendered with Chrome --headless=new --screenshot at exact window size and inspected; the page itself rendered headless at 1360 px and inspected. No product source changed.","path":"docs/design/BPR-1106-composition-entries-v1.html"}},{"criterion":"AC2 The owner's choice is recorded here, one question on its own candidate, with the fingerprint-bound verdict","outcome":"pending","revision":"4e9037e6","evidence":{"kind":"observation","detail":"Prepared, not asked: candidate BPR-1106-placement-v001, one question (A, B, C, D or E; proposed C), with the design page as its evidence. No attended session was started by this dispatch.","path":"docs/issues/40-parked/BPR-1106-a-design-round-for-where-the-composition-entries-sit-in-the-review-chrome.md#proposal-and-the-prepared-ask"}},{"criterion":"AC3 Whatever the owner chooses is reflected in RFC-BPR-0028 AC-01 and its chrome paragraph, and any implementation is carded, not done on this card","outcome":"pending","revision":"4e9037e6","evidence":{"kind":"observation","detail":"Waits on AC2. The page lists, per placement, what AC-01, the chrome paragraph, BPR-1036's floor (+22 macOS, +20 Linux) and BPR-0940's window course assertion would need."}}]
---

# BPR-1106: A design round for where the composition entries sit in the review chrome

## Why

On 2026-09-24 the owner answered [[BPR-0653]]'s ask 3 in attended session
`cbd48b91000b00f1e72e1ad62c7c384c` (packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
candidate `BPR-0653-entry-row-v001`, decision `007a6eb5`, 18:54:09Z). He chose
to amend RFC-BPR-0028 AC-01 so that it admits the entry rows between the strip
and the epoch line, and added, verbatim:

> We should do a UX design iteration with the /frontend-design skill to figure
> out where best to put these. For now you can ammend the card but open a
> follow-up for the ux design.

The amendment is landed in
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) AC-01 and
its chrome paragraph. This card is the design iteration.

## What sits there today

`build_central_surface` (`src/adapters/qt/src/workbench_window.cpp`) stacks,
between the strip and the epoch line:

- the composition entry row from RFC-BPR-0037 (`cc038748`, [[BPR-0929]]):
  Compose review and Draft comment or reply, 20 logical pixels on Linux and
  Windows and 22 on macOS;
- while a pull request loads, [[BPR-0795]]'s readiness line.

The inputs to the design:

- The owner's [[BPR-0246]] space budget: every surface other than the code
  panes and their gutter is on a strict budget in the review state. The
  UIforETW course measures the comparison at 842/1000 of the window on Linux
  and 835/1000 on macOS with the row, and 862 and 857 without it
  ([[BPR-1036]]).
- On Windows the strip is the title row and fits at its 902-pixel floor with
  nothing to spare ([[BPR-0653]]), so it cannot simply take two more controls.
- RFC-BPR-0037's own layout study put the Compose review entry "in toolbar"
  (`conversation-surface-layout-study.md`, its Read row).
- [[BPR-0737]]'s direction put the epoch line directly under the strip, above
  the drawers and their headings. The amendment keeps it above the drawers.
- The strip's existing review controls menu was one option on the ask. It
  costs the strip no width.

## Scope

- Run a design round with the `frontend-design` skill: at least three
  placements drawn on the accepted full review surface
  ([RFC-BPR-0028 full review surface](../../design/RFC-BPR-0028-full-review-surface-v1.html)),
  light and dark, at 1280x852 and 1920x1080, on macOS and Windows chrome. The
  current row is one of them, as the baseline.
- For each placement, state what it costs: height taken from the comparison,
  strip width at the Windows floor, clicks to Compose review and to Draft
  comment or reply, keyboard reach, and where the readiness line goes.
- Put the placements to the owner as one attended question: which placement,
  or the current row.
- If the owner picks a placement other than the current row, amend
  RFC-BPR-0028 AC-01 and the chrome paragraph again, and card the
  implementation, including [[BPR-1036]]'s floor and [[BPR-0940]]'s window
  course assertion. If the owner keeps the row, record that here and close.

## Acceptance

- AC1 The design page shows at least three placements with the current row
  as the baseline, each with its stated costs, rendered and inspected headless
  at both sizes in both appearances.
- AC2 The owner's choice is recorded here, one question on its own candidate,
  with the fingerprint-bound verdict.
- AC3 Whatever the owner chooses is reflected in RFC-BPR-0028 AC-01 and its
  chrome paragraph, and any implementation is carded, not done on this card.

## Boundaries

Design only: no product source changes on this card. The core decides, the
host performs, Qt presents; a placement that needs a new command or state is
named as such for the implementation card. This card is not linked to
RFC-BPR-0028 and does not hold [[BPR-1021]]'s closeout.

## Design round v1, 2026-09-24 (bpr-main-mac)

The round ran with the `frontend-design` skill. Nothing under `src/` or
`tests/` changed.

- **The design page**:
  [BPR-1106-composition-entries-v1](../../design/BPR-1106-composition-entries-v1.html).
  It holds the question, the five placements at a glance and close up, the
  costs to scale, the costs side by side, each placement's frames and what
  it would need, and the proposal.
- **The frames**:
  [BPR-1106-composition-entries-frames-v1](../../design/BPR-1106-composition-entries-frames-v1.html).
  It is one window drawn with the accepted full review surface's tokens,
  strip, Updates line, drawers and threads, and the query picks the placement,
  appearance, platform, size and state. Any two frames therefore differ only in
  the placement.
- **The renders**: `docs/verification/BPR-1106-composition-entries/`, 58
  files. Each placement is drawn at 1280x852 and 1920x1080, light and dark,
  macOS and Windows chrome: 40 frames. Each is also drawn while a pull request
  loads, at 1280x852 on macOS in both appearances: 10 more. The other 8 are 1:1
  close-ups cut from those frames. Headless Chrome (`--headless=new
  --screenshot`) rendered every frame at exactly its window size, device scale
  1. The frames were then reduced to 256-colour PNGs (12.8 MB became 4.9 MB).
  The frames and the page itself were rendered and looked at. Two fixes came
  from that inspection: the strip's left group overran the modality on
  Windows at 1280, and D's foot did not fit the 280-pixel drawer with icons.

### The five placements

- **A, the row as built (baseline).** Compose review and Draft comment or
  reply on their own row between the strip and the Updates line.
- **B, the strip's review menu.** The Private progress button keeps its place,
  its 55 pixels and Alt+Shift+4. Its menu gains a "Your review" section above
  Seen, Read, Accept and Exclude.
- **C, the Updates line's right end.** The two labelled buttons follow
  "Comparing … ▾" after a divider. While a pull request loads, the readiness
  wording and its bar take the track's place.
- **D, the conversation drawer's foot.** A bar at the bottom of the drawer.
- **E, a Compose control in the strip.** Pen and arrow before Review details.
  A click composes a review, and the arrow offers Draft comment or reply.

### What each costs

| | A | B | C | D | E |
|---|---|---|---|---|---|
| Height from the comparison | 22 px macOS, 20 Windows and Linux, every mode | 0 | 0 | 0 (the drawer's thread column gives 36) | 0 |
| Strip width at the Windows 902 floor | 0 | 0 | 0 | 0 | +59 (55 + 4): floor to 961 or a new yield |
| Clicks to Compose review | 1 | 2 | 1 | 1, or 2 with the drawer hidden | 1 |
| Clicks to Draft comment or reply | 1 | 2 | 1 | 1, or 2 with the drawer hidden | 2 |
| Keyboard reach | Tab stops after the Updates line's controls | Alt+Shift+4, then C or D | A's Tab stops, after "Comparing" | Alt+Shift+2, then the drawer's last Tab stops | a strip Tab stop before Review details |
| Readiness line | its row above the entry row, about 28 px | its own row, as today | inside the Updates line, no row comes or goes | its own row, as today | its own row, as today |

Where the numbers come from:

- **Heights.** These are the mock's own heights, and its row heights match the
  product: 22 on macOS and 20 on Windows (BPR-1036, BPR-0653). At 1280x852 on
  macOS the comparison is 746 with the row (875 per mille) and 768 without it
  (901).
- **Windows width.** The floor numbers are BPR-0653's measurements of the
  product. E's 55 pixels are the Private progress button's width as measured
  by BPR-1036's capture.
- **The track.** C takes 303 pixels of the Updates line's track at every
  width, measured on the mock: 1000 becomes 697 at 1280.

### Proposal and the prepared ask

The page proposes **C**:

- the comparison gets its pixels back;
- nothing is added to the Windows strip;
- each entry keeps one click and a visible label;
- the readiness line gets a home that does not move the window when loading
  ends.

The runner-up is D. B is the cheapest to build but hides both entries. E
spends width the Windows strip does not have.

The ask is prepared for one attended candidate, `BPR-1106-placement-v001`.
Nothing has been asked yet, and no session was started. The page is the
candidate's evidence. The question, verbatim:

> Where do Compose review and Draft comment or reply go? A, keep the row as it
> is; B, the strip's review menu; C, the right end of the Updates line (the
> loading line moves into it too); D, the foot of the conversation drawer; or
> E, a Compose control in the strip. Proposed: C.

Whatever the owner picks, it is recorded here with the fingerprint-bound
verdict (AC2). If the pick is B, C, D or E, RFC-BPR-0028 AC-01 and its chrome
paragraph are amended again, and the implementation is carded (AC3). That card
carries BPR-1036's floor (+22 on macOS, +20 on Linux) and BPR-0940's window
course assertion, plus the placement's own needs listed on the page.

## Owner answer, 2026-09-25: settle it outside attended review (attended session 70834742)

Source: attended session `7083474240820608cdf9ff551ddd298f`, packet
[owner-decisions-20260924-scoped-v001](../../verification/reviews/owner-decisions-20260924-scoped-v001/operator-packet.md),
hand-back at sequence 32. Candidate `BPR-1106-placement-v001`, revision
"4e9037e6 (parked a7dd1cd2)", fingerprint `a115198c…` equal to the
prepared candidate's in
[session-identity.json](../../verification/reviews/owner-decisions-20260924-scoped-v001/session-identity.json),
decision `ece7a6f3-3e54-459f-9c41-eba06732a91d`, sequence 28,
2026-09-25T00:14:58Z, `actor: human`: **select** "Other: my answer is in the
note", with this note, verbatim:

> Let's discuss this outside of the attended review flow, give me a handoff
> prompt to tackle this properly in a separate session thread.

No placement is chosen: not A to E, and not the proposed C. AC2 stays
pending, and AC3 waits on it. RFC-BPR-0028 AC-01 stays as the owner amended it
on 2026-09-24. The design page, its frames and the costs above are the
material for the separate session; the coordinator gives the owner its
handoff prompt. The placement is not asked again through attended review.

The card leaves `human-review` and is Parked on
`owner-design-discussion`: it waits on the owner's own design session, not on
an attended queue. Whatever that session decides is recorded here as an owner
decision, and AC3's amendment and implementation card follow from it.
