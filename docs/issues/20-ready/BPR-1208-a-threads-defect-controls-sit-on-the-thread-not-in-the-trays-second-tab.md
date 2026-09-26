---
schema: "implement-rfc.card.v1"
id: "BPR-1208"
title: "A thread's defect controls sit on the thread, not in the tray's second tab"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1133","BPR-1134"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner direction on BPR-1204's tab, 2026-09-25, in chat, relayed by the coordinator","criteria":["AC-09"],"related_rfcs":["RFC-BPR-0009"],"related_cards":["BPR-1204","BPR-1133","BPR-1134","BPR-1135"],"reservation":"docs/issues/_reservations/BPR-1208--BPR-1210--a47a9558dfa6.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"},"owner_handback49":{"session":"d7cf21b16656c6ec19c8787255aa4006","cursor":49,"source_revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","binary_sha256":"96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b","decisions":[{"candidate":"BPR-1208-thread","criterion":"AC7","fingerprint":"449a814cd9b20b247c4852e9d02caf92a32e091a3ed7b31082dfcf4907233cb3","decision_id":"c061bb76-dc64-4e6d-930a-92dbf7d08cda","outcome":"reject","note":"There's no defect in that fixture to act on.","saved_at":"2026-09-26T20:57:45.943874+00:00","actor":"human"},{"candidate":"BPR-1208-voiceover","criterion":"AC6","fingerprint":"2a02cb5964f621d28ef8df0a87f2be3fde22c997d6af5e6623db1407cc64180e","decision_id":"d1b43cd6-ebe1-4814-a3ef-8156bd80878a","outcome":"accept","note":"","saved_at":"2026-09-26T20:53:54.818915+00:00","actor":"human"}]},"owner_handback32":{"session":"0a3f0caf7998162c936e25e6d3f68672","handback_cursor":32,"source_revision":"1971a9ab82b965d57925023469ec9b14513de894","executable_sha256":"b45b64204b0d34fac1e9e9c977905768408536ff1e42cbd8b03021c1ee512715","decisions":[{"candidate":"BPR-1208-thread","fingerprint":"c12224f9a80d3ccfab81743cc0f3217ba8d1fcdb345e845f13e7460d3f7ed771","decision_id":"01fa9b97-31d9-4ed2-9091-d1a0eafa8338","outcome":"accept","note":"Yes they are although navigation in this right drawer is really wonky. These defects are in this tiny upper section of the pane with an incredibly slow scroll speed. This, presumably filters list, takes up a ton of vertical space. The main diff pane seems to sometimes scroll when you select comments but not consistently. It looks like there is supposed to be more below this second screenshot but it wont scroll anymore and its cut off.","saved_at":"2026-09-26T22:32:18.475056+00:00","actor":"human","event_sequence":28,"attachments":[{"sha256":"19e4fcda207740307266a20d9197e6e2ef5533a9db3ec3bbb6560a762a2a829f","name":"handback32-BPR-1208-thread-1.png"},{"sha256":"f8faf164c66f8225acead048a16e673d02d22bf640b4c746159bd428083e2e14","name":"handback32-BPR-1208-thread-2.png"}],"criterion":"AC7"}]}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"44bd1266e085ec0047d8ce08fa898a4e04673aa8","evidence":{"kind":"command","detail":"2/2 core/host; 3/3 focused; full GitHub/GHES seed/reopen defect public composition; 6/6 final scales and review source pin. Explicit-scale 100/125/200 captures inspected. Evidence /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1208-*. Negative old-tab placement, exact command identity, keyboard menu and no-publication assertions pass."}},{"criterion":"AC2","outcome":"pass","revision":"44bd1266e085ec0047d8ce08fa898a4e04673aa8","evidence":{"kind":"command","detail":"2/2 core/host; 3/3 focused; full GitHub/GHES seed/reopen defect public composition; 6/6 final scales and review source pin. Explicit-scale 100/125/200 captures inspected. Evidence /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1208-*. Negative old-tab placement, exact command identity, keyboard menu and no-publication assertions pass."}},{"criterion":"AC3","outcome":"pass","revision":"44bd1266e085ec0047d8ce08fa898a4e04673aa8","evidence":{"kind":"command","detail":"2/2 core/host; 3/3 focused; full GitHub/GHES seed/reopen defect public composition; 6/6 final scales and review source pin. Explicit-scale 100/125/200 captures inspected. Evidence /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1208-*. Negative old-tab placement, exact command identity, keyboard menu and no-publication assertions pass."}},{"criterion":"AC4","outcome":"pass","revision":"44bd1266e085ec0047d8ce08fa898a4e04673aa8","evidence":{"kind":"command","detail":"2/2 core/host; 3/3 focused; full GitHub/GHES seed/reopen defect public composition; 6/6 final scales and review source pin. Explicit-scale 100/125/200 captures inspected. Evidence /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1208-*. Negative old-tab placement, exact command identity, keyboard menu and no-publication assertions pass."}},{"criterion":"AC5","outcome":"pass","revision":"44bd1266e085ec0047d8ce08fa898a4e04673aa8","evidence":{"kind":"command","detail":"2/2 core/host; 3/3 focused; full GitHub/GHES seed/reopen defect public composition; 6/6 final scales and review source pin. Explicit-scale 100/125/200 captures inspected. Evidence /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1208-*. Negative old-tab placement, exact command identity, keyboard menu and no-publication assertions pass."}},{"criterion":"AC6","outcome":"pass","revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","evidence":{"kind":"observation","detail":"Owner hand-back 49, session d7cf21b16656c6ec19c8787255aa4006; BPR-1208-voiceover accept d1b43cd6-ebe1-4814-a3ef-8156bd80878a. Exact fingerprints and notes retained in metadata. Verdict applies only to the frozen candidate."}},{"criterion":"AC7","outcome":"pass","revision":"1971a9ab82b965d57925023469ec9b14513de894","evidence":{"kind":"observation","detail":"Owner hand-back32 session 0a3f0caf7998162c936e25e6d3f68672; BPR-1208-thread accept decision 01fa9b97-31d9-4ed2-9091-d1a0eafa8338, fingerprint c12224f9a80d3ccfab81743cc0f3217ba8d1fcdb345e845f13e7460d3f7ed771. Exact note and attachment hashes retained in metadata and owner-handback32.json. Applies to the reviewed frozen executable b45b64204b0d34fac1e9e9c977905768408536ff1e42cbd8b03021c1ee512715."}}]
---

# BPR-1208: A thread's defect controls sit on the thread, not in the tray's second tab

## Owner direction

From the owner's direction of 2026-09-25 on the tray's second tab, recorded
verbatim on [[BPR-1204]]:

> I don't think thread controls make any sense there, that's a tab for the whole file not thread. [...] Off hand my thoughts are move the thread controls to well the thread, right click menu maybe or a button.

The second tab ([[BPR-1134]], today "Findings", to be Notes by [[BPR-1204]])
holds RFC-BPR-0009's thread controls, which act on the selected conversation:

- "Classify selected conversation as defect";
- the defect position ("Defects N/M") with Previous defect and Next defect;
- the defect card, which shows the state, the local classification with its
  severity and category selectors, and the context availability;
- the verdicts "Apply local classification", "Verify this fix", "Reopen this
  defect", "Withdraw this defect" and "Accept as is".

Today these are reachable only from that tab. No menu or key reaches them.

## Decision made under the owner's delegation (2026-09-25)

The owner's "right click menu maybe or a button" left the form open. The
coordinator then relayed an owner amendment that delegates the choice: a
context menu, a button on the thread card, or both, to be decided by the
implementer and not left open. The implementer decided **both, split by
purpose**:

- **A context menu on every thread card.** It lists the card's commands:
  the card's existing actions, then "Classify as defect…" for an unclassified
  thread, or, for a defect, its verdicts and "Reclassify…".
- **A defect badge button on a defect's own card.** It shows the state and
  classification as text and opens the same defect actions. An unclassified
  thread gets no new button.
- **Defect navigation joins Next new comment.** Previous defect, Next defect
  and the position move beneath the Comments tab's header, beside Next new
  comment.

Rationale: the menu keeps a rarely used command out of every card while
leaving it one right-click or key away. The badge shows the state the
reviewer tracks and puts its verdicts one click away, the way Reply already
sits on the card. Defect navigation moves between the threads that Comments
lists, as Next new comment does.

## Acceptance

- AC1 The tray's second tab holds none of these controls: Classify, the
  defect position, Previous and Next defect, the defect card and its
  verdicts. A negative control that puts them back fails the tab's content
  course.
- AC2 Every thread card has a context menu, wherever the card appears: in the
  Comments tab, in Discuss and beside the code. The menu opens by right-click,
  and by the context-menu key or Shift+F10 on the selected card. It lists the
  card's existing actions (Reply, Mark read, Resolve or Unresolve, Open in
  code). For an unclassified thread it adds "Classify as defect…". For a
  defect it adds "Verify this fix", "Reopen this defect", "Withdraw this
  defect", "Accept as is" and "Reclassify…". An action the core would refuse
  is disabled, and its tooltip says why.
- AC3 A defect's card shows its state and local classification as a badge
  button. The badge uses text, not colour alone, and it opens the same defect
  actions. Its tooltip and accessible description carry the state's
  explanation and the context availability. "Reclassify…" offers the
  severity and category choices that the defect card offers today. An
  unclassified thread's card gains no button.
- AC4 Previous defect, Next defect and the "Defects N/M" position sit beneath
  the Comments tab's header, beside Next new comment. They move the selection
  between defect threads exactly as today.
- AC5 Every control dispatches the same core defect command, with the same
  conversation, defect and revision identities as the tab's controls did.
  RFC-BPR-0009's defect courses pass unchanged, and nothing publishes.
- AC6 Evidence: the conversation-cards, window, focus-order and composed
  courses assert the menu, the badge and the navigation where this card puts
  them. Keyboard and VoiceOver reach every defect action without a pointer.
  Renders at 100, 125 and 200 percent cover an unclassified card and a
  defect's card.
- AC7 The owner acts on a defect from its thread on macOS, and judges it with
  one question: are the thread's defect controls where you expect them?

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-09 as the owner amended it on 2026-09-25. It moves
[RFC-BPR-0009](../../rfcs/RFC-BPR-0009-defect-fix-verification-workflow.md)'s
controls without changing their semantics. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
[[BPR-1135]].

## Engineering source review — 2026-09-26

The candidate follows the delegated context-menu, badge and Comments-navigation
decision. The existing defect-card actions supply the verdict commands in a
thread-bound menu; they retain the captured defect identity when selection
changes. The existing severity/category editor opens in a local modal dialog.
Only visible cards receive native accessible badge buttons. An unclassified
thread has no badge. Existing conversation indexes and measurement caches remain.

The pinned application scope has no intervening commit after `30748ed6`. This
slice adds a per-accepted-snapshot join from both halves of the durable defect
identity to the exact thread. Ordinary defect projections visit retained defect
records; snapshot/ledger retirement invalidates the join. Ambiguous identities
receive no thread binding. It changes no core defect command, effect ownership,
provider admission, publication or ledger capability. Its tests-only composition
probe checks the accepted thread menu and badge and excludes the private tab.
The full-application and manifest pins are resealed with this recorded review;
the production mutation-branch pin is unchanged.

Native VoiceOver and owner placement judgments remain pending.

The first combined run found two corrected presentation defects: the new
navigation buttons lacked accessible descriptions, and BPR-1205's earlier-pair
filter also hid current threads from the unselected review-wide projection.
The latter stalled both composed courses before their first file selection.
The correction filters a selected file only, retains the complete unselected
forest, and invalidates the placed forest only when file presence changes,
so ordinary file-to-file projection retains BPR-0972's cache. A focused
transition test covers unselected → older file → unselected → older file →
Discuss. The old classification unit fixture now names its actual review.cpp
file; a thread from another file must not classify against the displayed file.
The first composed course exhausted its 40-second bound; the second was stopped
after showing the same initial stall to release the shared Qt slot. Neither is
claimed as passing. Existing core defect workflow and host correlation checks
pass 2/2 after building their previously absent test binaries.

The corrected initial course reaches `DEFECT_THREAD_CONTROLS_OK`. Its later
reversed-pair wait still expected selectable current threads, contradicting
BPR-1205's owner amendment. That test-only gate now requires those identities
while the current pair is used, then asserts their absence on the older pair
alongside the unchanged no-anchor/no-classification, durable correlation,
refused-persistence, explicit verification, reclassification and restart checks.
No product admission or original semantic expectation is relaxed. This
tests-only update is included in the application pin review above.

The same tests-only probe now enters Review through the shared public modality
step and opens Comments through its existing action before selecting a file.
The composed badge check requires actual visible cards, rather than inspecting
a retained but hidden Overview widget. This is included in the pinned source
review; the production mutation branch remains unchanged.

Final engineering checks pass: core/host 2/2, conversation panel/focus/reveal
3/3, the four-process GitHub/GHES defect composition (54.98 seconds), and
window/focus scales plus review-round-trip source pin 6/6 (79.04 seconds).
Logs are under `/Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1208-*`.
The top-card renders at 100/125/200 were inspected. The new bottom-card
capture covers a classified and an unclassified thread together; its first
three selector runs all used 100 percent because CTest overrides the caller's
scale variable. The 125/200 bottom captures remain to be rerun with explicit
isolated commands, rather than mislabelled as scale evidence. The screenshot
change is test-only. Native VoiceOver and the separate owner judgment remain
pending regardless of those captures.

The corrected direct network-isolated selector runs set and assert 1.25 and
2.0 explicitly; both passed (`1208-correct-scale-{125,200}.log`). Their
1600×1125 and 2560×1800 captures were inspected: the classified badge and
unclassified card, along with the Comments navigation, are readable at each
scale. This completes the automated/rendered part of AC6; VoiceOver remains
pending.

## Combined main integration — 2026-09-26

The integration pass caught a stale expectation in the composed-GitHub course:
it still demanded the old defect buttons in Notes. The test now retains its
complete Notes width/layout sweep, includes the private note and promotion
controls, and refuses thread controls there. It measures Comments navigation,
requires an exact-thread text badge, and opens the thread menu with Shift+F10
to check all five explained defect actions. No product command or admission
rule changes. The corrected composed course passes at 100/125/200 percent
(3.84/3.51/4.15 seconds); the three Comments crops and full Notes column were
inspected. The main defect public course also passes in 53.91 seconds.
The [combined integration record](../../verification/combined-main-integration-2026-09-26.md)
binds this additional evidence to main. It does not replace the frozen owner
candidate or satisfy native VoiceOver or the owner's placement judgment.

## Owner hand-back 49 — 2026-09-26

The owner reviewed frozen source `8b5bc31c`, executable SHA-256
`96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b`,
in session `d7cf21b16656c6ec19c8787255aa4006`. Full decision identities and
fingerprints are retained in the card metadata.

- **AC7 — Reject**, candidate `BPR-1208-thread`, decision `c061bb76-dc64-4e6d-930a-92dbf7d08cda`.

  > There's no defect in that fixture to act on.

- **AC6 — Accept**, candidate `BPR-1208-voiceover`, decision `d1b43cd6-ebe1-4814-a3ef-8156bd80878a`. No note.

The rejected course is not accepted by its automated tests. The replacement
preparation must use the open authenticated synthetic fixture, prove the requested
public action and provide the Wizard steps before the next owner judgment. The
owner attachments show public preview, which correctly refuses these actions.

## Authenticated successor preparation — 2026-09-26

The existing public seed course created two defect records. The native UI selected the visible open critical/security thread, opened Reclassify, applied major/security locally and displayed the updated badge. The fixture was recreated through that public seed course, and the final native app again displayed critical/security. AC6 VoiceOver remains accepted from hand-back49 and is not queued again.

The frozen successor is source `1971a9ab82b965d57925023469ec9b14513de894`, executable
SHA-256 `b45b64204b0d34fac1e9e9c977905768408536ff1e42cbd8b03021c1ee512715`.
It uses the open synthetic `octo-org/review-repository#17` fixture after
Connect (Command–Shift–C), without changing authority or target admission.
The attended page supplies a copyable launch command and PR URL, and its
Wizard supplies one action per step with the expected result. Native proof
is an objective observation; each criterion still needs its own Accept or
Reject. The original rejection and fingerprint remain above.

Session `0a3f0caf7998162c936e25e6d3f68672` queues only the six previously rejected
criteria. [The successor handoff](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/HANDOFF-m2b-r2-2026-09-26.md)
binds the fixture, immutable app, dependencies, commands and continuation.

## Owner hand-back 32 — 2026-09-26

The owner reviewed source `1971a9ab82b965d57925023469ec9b14513de894`, executable SHA-256
`b45b64204b0d34fac1e9e9c977905768408536ff1e42cbd8b03021c1ee512715`, in session `0a3f0caf7998162c936e25e6d3f68672`.
The [decision receipt](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/owner-handback32.json)
binds every verdict, fingerprint, exact note and attachment digest; original
attachment bytes remain in private runtime storage. Earlier verdicts are retained.

- **AC7 — Accept**, candidate `BPR-1208-thread`, decision `01fa9b97-31d9-4ed2-9091-d1a0eafa8338`.

  > Yes they are although navigation in this right drawer is really wonky. These defects are in this tiny upper section of the pane with an incredibly slow scroll speed. This, presumably filters list, takes up a ton of vertical space. The main diff pane seems to sometimes scroll when you select comments but not consistently. It looks like there is supposed to be more below this second screenshot but it wont scroll anymore and its cut off.

AC7 is accepted. The additional drawer scrolling, vertical-space and clipped-reply
observations remain engineering work in this round; no new acceptance is inferred
for those symptoms and the thread-control judgment will not be asked again.
