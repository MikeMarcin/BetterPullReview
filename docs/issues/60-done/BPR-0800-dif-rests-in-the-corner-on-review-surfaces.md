---
schema: "implement-rfc.card.v1"
id: "BPR-0800"
title: "Dif rests in the bottom-left corner on review surfaces too"
status: "Done"
rfcs: ["RFC-BPR-0031", "RFC-BPR-0032"]
depends_on: ["BPR-1209"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"owner mandate r2, 2026-09-15; the remainder BPR-0775 bounded out and d8 left un-carded; allocated on main in d9","acceptance":["RFC-BPR-0031 B5","RFC-BPR-0031 B6"],"carried_by":"BPR-1209"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1971a9ab82b965d57925023469ec9b14513de894","evidence":{"kind":"observation","detail":"BPR-1209 AC1/AC2/AC5: one28px leading-corner widget on ordinary review surfaces, no pointer/focus path, transient-message hiding; explicit-scale100/125/200 placement and negative control pass. Size amended by owner hand-back49."}},{"criterion":"AC2","outcome":"pass","revision":"1971a9ab82b965d57925023469ec9b14513de894","evidence":{"kind":"observation","detail":"BPR-1209 AC3/AC5: one-widget rule, shared decoded catalogue, first-launch Home and accepted Compose workshop prime spots remain unchanged; placement courses pass."}},{"criterion":"AC3","outcome":"pass","revision":"1971a9ab82b965d57925023469ec9b14513de894","evidence":{"kind":"observation","detail":"Owner2026-09-25 direction removes the drawer slot before implementation; recorded decisions on BPR-1209 resolve the corner/slot question and preserve cue triggers."}},{"criterion":"AC4","outcome":"pass","revision":"1971a9ab82b965d57925023469ec9b14513de894","evidence":{"kind":"observation","detail":"RFC-BPR-0031 retains original acceptance and records the2026-09-25 owner placement amendment; BPR-1209 carries the new implementation evidence."}},{"criterion":"AC5","outcome":"pass","revision":"1971a9ab82b965d57925023469ec9b14513de894","evidence":{"kind":"observation","detail":"Native macOS owner Accept at hand-back32, session0a3f0caf7998162c936e25e6d3f68672, BPR-1209-corner decisionc91d25c2-3b77-4e7c-a123-b545cedb4ad1; exact frozen source/executable and fingerprint recorded on card. No other native platform inferred."}}]
---

# BPR-0800: Dif rests in the bottom-left corner on review surfaces too

## Outcome

The owner's direction of 2026-09-15, verbatim:

> fwiw regarding Dif I want him to act mostly like Nib from AllTheThings. He can
> show up on the home page for the first time login experience, but otherwise he
> should mostly sit in the very bottom left corner and stay out of the way.

**Half of that shipped.** [[BPR-0775]] reconciled it with RFC-BPR-0031 for
**Home**: signed out — the first-time experience — Dif keeps RFC-BPR-0031's
landing placement at 96 px beside the card's sentence, and on every other Home,
signed in and Home over a review, the same single widget rests at 40 px over
`workbenchDifCornerSlot`, the first widget of the status bar, with no pointer or
focus path, hidden while the status bar shows a transient message. The owner
accepted that on 2026-09-16 (`bpr-0779-dif-corner`, decision 123f144b).

**"Otherwise" is more than Home.** A reviewer spends their time on the review
surface, and there Dif is not in the corner: RFC-BPR-0031 places him in the AI
tools slot (B6) and hides him elsewhere. So the direction's own word — *mostly* —
is not met yet for the surface the application is mostly on. BPR-0775 bounded
this out deliberately, as window-level work rather than Home work, and recorded
it as a follow-up for main to allocate. d8 named it again and left it un-carded
as scope creep at a closeout boundary. This card is the home it never had.

## Acceptance

- On the review surface, the same single Dif widget rests at the corner slot at
  the leading edge of the status bar, at the size Home already uses, out of the
  way: no pointer path, no focus path, no space taken from a control, and hidden
  while the status bar carries a transient message.
- The one-sprite rule holds. There is never a Dif in the corner and a Dif in the
  AI tools slot at the same time, in the same window or across windows, and the
  existing single decoded retained catalog is still shared (RFC-BPR-0031 B10).
- **What happens to the AI tools slot is the open question and must be answered
  before any code is written.** RFC-BPR-0031 B6 requires one explicit visible AI
  tools slot that never overlays code or shared comments; B5's "all other
  surfaces hide it" courses assert the opposite of this card. Either the corner
  replaces the slot on review surfaces, or the slot keeps its cues and the corner
  is Dif's resting place between them. Whichever it is, say why, and carry the
  answer into RFC-BPR-0031 rather than leaving the courses to contradict it.
- RFC-BPR-0031 is `Auto-Verified`. Its accepted criteria are not rewritten to fit
  an implementation; an amendment naming the owner's direction is the honest
  path, exactly as RFC-BPR-0032's Home section already records the Home half.
- The rendered corner on a review surface gets its own attended verdict, one
  question for this criterion alone, as the Home corner did. Nothing about how it
  reads at 40 px beside live review text is inferred from an offscreen course.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run** at card
authoring: nothing here has been implemented or observed.

Where the existing evidence lives, for whoever takes this:

- The Home half and its reconciliation: [[BPR-0775]] "Decisions — d3, mandate
  r2", and RFC-BPR-0032's Home section, "Dif on Home (owner direction,
  2026-09-15)".
- The courses that would change: `qt_workbench_window_scale_100/125/200`
  `test_dif_moves_to_the_explicit_ai_area_without_owning_it`, and RFC-BPR-0031's
  B5 and B6 courses.
- The owner's acceptance of the Home corner: attended session
  e966c88f3320638208adf4fcf0651e31, candidate `bpr-0779-dif-corner`, decision
  123f144b, bound to executable sha256 `d500ef7a…` from source `ed2b3f14`.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference
is authorized by this card. The core decides, the host performs, Qt presents: a
mascot is presentation and reaches no store, provider or process. No new sprite
work: this is placement, not identity — sprite revision is RFC-BPR-0030's. The
Appearance page's Dif controls are [[BPR-0790]] and are not reopened here.

## Related owner decision: Compose — 2026-09-25

For the Compose review page only, the owner chose a visible workshop slot over
the corner (attended session `06ad5734`, candidate `BPR-0966-compose-dif-v001`,
decision `dba44cfd`): while a workshop edit is on Compose, Dif moves into a
small AI tools area on that page and plays ready and acknowledge there. That
is [[BPR-0966]]'s AC1. It does not decide this card's corner-versus-slot
question for the other review surfaces, and the one-sprite rule above applies
to it as well.

## Owner direction, 2026-09-25: the question is answered, and BPR-1209 carries this card

In chat, relayed by the coordinator, the owner directed on [[BPR-1204]]'s tab,
verbatim: "Dif living in a tab that's likely hidden forever makes no sense,
definitely doesn't belong there. [...] Move Dif out of the right drawer
entirely, he can live in the lower left corner of the status bar by default
just like AllTheThings unless he's got a prime spot on a screen like on the
first time experience home."

**The open question above is answered.** Dif leaves the conversations drawer,
so the AI tools slot there cannot keep him. On review surfaces the corner
replaces the slot. That is the owner's answer, not an implementer's choice.

**Decided under the owner's delegation.** The coordinator relayed that the
owner delegates the Dif conflict and every other open choice in this batch.
Each of these is recorded with its rationale on [[BPR-1209]]:

- **The Compose workshop slot is a prime spot.** [[BPR-0966]]'s workshop slot
  on Compose (decision `dba44cfd`) keeps Dif while a workshop proposal is
  shown and after its acceptance, and that verdict is unchanged.
- **Cues play where Dif is.** Every RFC-BPR-0031 cue keeps its trigger and
  plays wherever Dif is, and B7 is not amended.
- **This card folds into [[BPR-1209]].** BPR-1209 carries this card's
  acceptance: the review-surface corner, the one-sprite rule, the
  RFC-BPR-0031 amendment (now recorded in RFC-BPR-0031 as the owner's
  amendment of 2026-09-25) and the attended corner verdict. This card depends
  on BPR-1209 and closes on its evidence, with no separate build.

## Carried acceptance — 2026-09-26

BPR-1209 is Done on source `1971a9ab82b965d57925023469ec9b14513de894`.
Its AC1–AC5 focused placement and cue courses establish this card's review
corner, one-widget and original-trigger requirements. The owner directed removal
of the drawer slot before implementation; the recorded RFC-BPR-0031 amendment
and preserved Compose prime spot resolve the original question.

The owner's subsequent hand-back49 amended the corner size to28 logical pixels.
BPR-1209's corrected100/125/200 courses and native inspection establish that
size. At hand-back32, session `0a3f0caf7998162c936e25e6d3f68672`, the owner
accepted `BPR-1209-corner`, decision `c91d25c2-3b77-4e7c-a123-b545cedb4ad1`,
fingerprint `19daf83f60ffb542aba4b984db8c771f2cdfc165c634ad7ff6dbbc66eb7c436f`,
for executable SHA-256
`b45b64204b0d34fac1e9e9c977905768408536ff1e42cbd8b03021c1ee512715`.
This supplies the separate native macOS review-surface judgment. It claims no
Windows/Linux native observation or release qualification. No separate build
or repeated owner question was required by the explicit carried-by decision.
