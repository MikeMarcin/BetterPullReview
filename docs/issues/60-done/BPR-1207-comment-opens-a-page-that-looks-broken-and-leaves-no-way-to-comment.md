---
schema: "implement-rfc.card.v1"
id: "BPR-1207"
title: "Comment opens a page that looks broken and leaves no way to comment"
status: "Done"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1133"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner note on attended candidate BPR-1133-arrow, 2026-09-25","session":"06ad5734ed9f7c2fd6d0a62c8328fa0a","candidate":"BPR-1133-arrow","decision":"b3d1d32d-02c6-4951-98bf-d5ecd4d36d0f","reviewed_revision":"ba9d272339ad235f1d99688c5c1af198b0c9c7ef","reviewed_executable_sha256":"a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83","criteria":["AC-08"],"related_cards":["BPR-1133","BPR-1135"],"reservation":"docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"},"owner_handback49":{"session":"d7cf21b16656c6ec19c8787255aa4006","cursor":49,"source_revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","binary_sha256":"96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b","decisions":[{"candidate":"BPR-1207-comment","criterion":"AC3","fingerprint":"feaaefd6e30de336587c5c074e017c49611be7993ed763a8014f21ca5f1bafe5","decision_id":"e1878b04-19ec-40a8-ab7c-ed1b35417f65","outcome":"reject","note":"Meta feedback: I don't know why the link to the PR to open isn't in a copyable code block on the review page. And the instructions are not clear as I would expect from the $wizard skill\n\nI cannot add a comment with the instructions and fixture provided, comment button is not active. \n","saved_at":"2026-09-26T21:00:27.749392+00:00","actor":"human"}]},"owner_handback32":{"session":"0a3f0caf7998162c936e25e6d3f68672","handback_cursor":32,"source_revision":"1971a9ab82b965d57925023469ec9b14513de894","executable_sha256":"b45b64204b0d34fac1e9e9c977905768408536ff1e42cbd8b03021c1ee512715","decisions":[{"candidate":"BPR-1207-comment","fingerprint":"1c1a5d7e8bf6942a13b45dcc76cd41d76f362ff5b616494c0e94669509547a0c","decision_id":"54d90752-daed-457a-a334-bff92b4f340f","outcome":"reject","note":"Says provider does not support publication.","saved_at":"2026-09-26T22:34:30.294213+00:00","actor":"human","event_sequence":29,"attachments":[{"sha256":"01875d834b61bd1bff1ea74626bd894c52ae25d411c865eb7b50074e147e7ec1","name":"handback32-BPR-1207-comment-1.png"}],"criterion":"AC3"}]},"owner_handback12":{"session":"f613e282cad0d66a0813a7c85b3954a0","handback_cursor":12,"source_revision":"c777a33d7467e83cfd0d2217329953e2044f1d41","executable_sha256":"8589abf9dca824127e1531aa26699c9ee9d0456626fdca090c04eacf217b50ba","decisions":[{"candidate":"BPR-1207-comment","fingerprint":"966b3f2f5a2b0dc8d0f3ec27a33dcde644a25726bc3d8e2a2f79074d80a3fb2f","decision_id":"8d7d2682-13bf-4ff1-970f-c4335f326b16","outcome":"accept","note":"","saved_at":"2026-09-26T23:48:07.513284+00:00","actor":"human","event_sequence":11,"attachments":[],"criterion":"AC3"}]}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"356bbdb22624beabe7b8f611e6c54c8d9abdad3f","evidence":{"kind":"observation","detail":"Exact frozen m2 native UIforETW fixture reproduced unusable page before deliberate change and after current-head change selection; closed PR has no admitted mutation draft. Card records executable digest and route; no publication."}},{"criterion":"AC2","outcome":"pass","revision":"c777a33d7467e83cfd0d2217329953e2044f1d41","evidence":{"kind":"command","detail":"Corrected target admission: core/host/editor/window/composed Comment and threaded Reply courses, actual left-target forced-enable negative control, architecture3/3, source pins and changed-TU tidy pass. Native final frozen app typed local Comment on right; left new target refused with right-side explanation; right restored. No publication. Logs and native-final-observations.json at /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-r3-rb0nbkl2/evidence"}},{"criterion":"AC3","outcome":"pass","revision":"c777a33d7467e83cfd0d2217329953e2044f1d41","evidence":{"kind":"observation","detail":"Owner Accept on the local-writing/no-publication criterion: session f613e282cad0d66a0813a7c85b3954a0, BPR-1207-comment, fingerprint 966b3f2f5a2b0dc8d0f3ec27a33dcde644a25726bc3d8e2a2f79074d80a3fb2f, decision 8d7d2682-13bf-4ff1-970f-c4335f326b16, event11, hand-back12. Saved 2026-09-26T23:48:07.513284+00:00. No note or attachment. Exact executable 8589abf9dca824127e1531aa26699c9ee9d0456626fdca090c04eacf217b50ba. Both prior Rejects and all earlier Accepts remain bound to their reviewed candidates; see owner-handback12.json."}}]
---

# BPR-1207: Comment opens a page that looks broken and leaves no way to comment

## Owner observation

In attended session `06ad5734` (packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)), candidate `BPR-1133-arrow` on the
frozen m2 app (`ba9d2723`), the owner kept the interim arrow and menu (decision
`b3d1d32d-02c6-4951-98bf-d5ecd4d36d0f`, sequence 49, `actor: human`, "Keep the
current arrow and menu") and added, verbatim:

> note: clicking comment takes me to this screen in the screenshot which seems completely broken, Doesn't seem like I can actually leave a comment yet.

The screenshot is kept with the decision in the session's external state. It
shows, in Review, with the bar at "8 changes" and "file 4 of 5": Back; Publish
and Discard; the line "Select a current-head diff line to compose a review
comment."; Reload accepted draft and Reapply my text; and three empty dark
panels. The status bar reads "Actionable file 4 of 5. Private progress:
unread, not accepted. Review history is ready." The line is
`review_draft_editor.cpp`'s empty-target state.

RFC-BPR-0052 AC-08 says Comment starts a comment on the current line.
Whether a current-head line existed in the owner's state is not established.

## Acceptance

- AC1 Reproduce on the frozen candidate's fixture what Comment does from the
  owner's state (a file open, no change selected), and whether selecting a
  current-head line and then Comment gives a usable editor.
- AC2 Comment never opens an unusable page. With no current-head line it is
  either unavailable, with the reason in its tooltip and menu, or it takes a
  defined line. The inline page never shows empty panels or draft-revision
  actions that cannot act. A composed course and a negative control pin it.
- AC3 The owner writes a comment through Comment on the fixture, without
  publishing, and judges it on macOS: one question.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]].

## Engineering evidence — 2026-09-26

AC1: the exact frozen m2 executable (SHA-256
`a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83`)
ran natively with the uiforetw-74 fixture and the explicit automated fixture
boundary. Home opened google/UIforETW#74. On file 4/5,
UIforETW/UIforETWDlg.cpp, Comment reproduced the empty panels and disabled
controls before a deliberate change-selection gesture. After Next change
selected line 216, Comment still showed an unusable page. The recorded PR is
closed, so a current diff line does not imply an admitted mutation draft.
No publication was attempted. The frozen process exited normally; the earlier
failed sandboxed GUI launchers and fixture processes also ended. This records
the observed route without inventing the owner's exact previous clicks.

Decision: enable Comment only for the selected target's admitted editable
draft or actionable recovery of its retained text, with its reason in the tooltip, menu and accessible description. This
preserves existing authority and publication decisions. Activation rechecks
the projection. The inline page hides empty workshop panels and recovery
controls that cannot act, retaining rejected text and its explicit recovery.
An accepted reapply or Reload clears the stale conflict sentence.

The composed fixture course selects a current-head line, opens Comment,
types a comment and observes its accepted local text with zero provider
submissions, comments or replies. Renders cover 100/125/200. The negative
control forces an unavailable Comment action enabled; activation still
refuses to open the unusable page. Editor, conversation and existing Compose
workshop courses pass. Full-window 100/125/200 courses pass after updating
former always-enabled assumptions. A rendered inspection then caught native
tool buttons remaining visible despite their hidden QAction; visibility now
follows the action and the composed assertion checks the buttons themselves.
The final control-specific rerun and exact candidate identity are recorded
in the acceptance evidence. AC3 remains pending: the owner's no-publish
writing judgment must use the open synthetic fixture, not closed UIforETW.

Self-review added a retained-conflict route: Comment may reopen the exact
target's usable Reload/Reapply recovery without enabling Publish. Its new
check exposed hidden QAction enablement being used to decide visibility;
both properties now follow the retained-candidate facts directly. The final
editor/workshop/window/composed rerun passes 4/4, including actual button
visibility, retained-conflict entry and zero mutation. Earlier scale passes
remain in the evidence directory; no failing run is counted as acceptance.

## Owner hand-back 49 — 2026-09-26

The owner reviewed frozen source `8b5bc31c`, executable SHA-256
`96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b`,
in session `d7cf21b16656c6ec19c8787255aa4006`. Full decision identities and
fingerprints are retained in the card metadata.

- **AC3 — Reject**, candidate `BPR-1207-comment`, decision `e1878b04-19ec-40a8-ab7c-ed1b35417f65`.

  > Meta feedback: I don't know why the link to the PR to open isn't in a copyable code block on the review page. And the instructions are not clear as I would expect from the $wizard skill
  >
  > I cannot add a comment with the instructions and fixture provided, comment button is not active.

The rejected course is not accepted by its automated tests. The replacement
preparation must use the open authenticated synthetic fixture, prove the requested
public action and provide the Wizard steps before the next owner judgment. The
owner attachments show public preview, which correctly refuses these actions.

## Authenticated successor preparation — 2026-09-26

The native app selected current-head review.cpp line30, exposed enabled Comment, opened the editable Review comment body and accepted local draft text. Nothing was published. The final app repeated admission and editable-page entry after the public seed reset. Closed/public-preview refusal remains correct; the successor setup supplies the eligible target.

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

- **AC3 — Reject**, candidate `BPR-1207-comment`, decision `54d90752-daed-457a-a334-bff92b4f340f`.

  > Says provider does not support publication.

AC3 remains unsatisfied. The screenshot shows typed local text with an unsupported
publication status. Diagnose the exact fixture and status path before preparing
one successor judgment; do not enable a live provider or relax publication admission.

## Hand-back32 diagnosis and target admission correction

The attached image shows Changes 2/4 and the exact message “This provider does
not support publication.” The fixture does compose its safe mutation provider.
The owner's exact Publish gesture was not observed, so the image alone does not
establish their entire route. The public composition reproduced that message
by selecting the left-side deletion at zero-based line 45, opening Comment,
typing disposable text and invoking Publish: Comment was enabled, the host
returned `unsupported`, and provider submissions remained zero. The diagnostic
negative control is retained privately in `r3` evidence `left-diagnostic.log`.

The cause is inconsistent target admission: the host already refuses a new root
comment on the left side, while the workbench offered Comment and the editor
Publish for that draft. The core now owns that shared target-shape rule. New
comments require the right/current-head side; an existing-thread reply remains
valid on either side. The host preserves its authority, exact binding, current
revision and endpoint checks. An invalid side also fails closed. Qt projects
the same rule, explains an unavailable left-side root, retains its text, and
rechecks Comment activation. The generic terminal message now names an
unavailable target without falsely attributing every refusal to the provider.

### Source pin review before reseal

Reviewed baseline `9bd38bf1` through this product slice. There is no intervening
`src/app` commit. The only pinned-source change is `promote_private_item`'s
preflight: require an editable body rather than Publish before Comment converts
a selected left-side thread into a reply. After that public gesture, the existing
Publish check remains, together with the exact draft scope, anchor, revision,
accepted text and rejected-candidate checks. No provider composition, credentials,
authority grant, source deletion, publication dispatch or lifetime path changes.
The host's target check is factored into a pure core rule; it admits no additional
valid target, and refuses unknown sides instead of allowing an unknown-side
reply. The tests-disabled production mutation branch remains byte-identical,
SHA-256 `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.
The full-TU and production-manifest pins are resealed in this same slice after
this review. This is implementer review, not independent or human acceptance.
AC3 remains rejected until the owner reviews the corrected frozen candidate.

### Corrected candidate verification

The core rule, application mutation host and draft editor pass. The existing
threaded-reply public course passes in 23.57 s. The final window course passes
in 20.64 s, explicitly selecting the right side before testing Comment, refusing
forced left-new activation, and retaining a current left-thread Reply with
Publish available. The composed Comment course passes in 14.61 s: actual local
typing, zero provider mutation, ordinary right-side Publish availability, and
Notes promotion/source retention remain covered. The composition source-pin
contract passes in 11.73 s.

The core-only core rule and both architecture tests pass 3/3 in 111.78 s, using
the existing verified shared dependency cache with offline acquisition. The
plain preset initially reset that cache path and failed configuration; the
explicit existing cache corrected the environment without a source change.
Changed production/core/editor translation units pass strict tidy; six existing
mechanical test warnings (five lambda parameter names and one size-cast
placement) were corrected, the two affected courses replayed, and strict tidy
on both test translation units passes. Changed paths are formatter-clean.
Logs live in `/Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-r3-rb0nbkl2/evidence/`
under `comment-*`. The owner criterion remains pending a successor review;
these automated checks do not reverse the recorded Reject.


## Corrected single-criterion successor

Frozen source `c777a33d7467e83cfd0d2217329953e2044f1d41`, executable SHA-256
`8589abf9dca824127e1531aa26699c9ee9d0456626fdca090c04eacf217b50ba`,
was checked natively with a synthetic fixture seeded directly at its final
fixed path. Copying the seeded directory correctly refused its bound history;
preparation was corrected without changing the path guard. Right-side Comment
accepted local text and exposed Publish. Left-side new Comment was disabled
with the right/current-head explanation; a right selection restored entry.
No Publish was clicked. The proof draft was discarded and the owned app closed.

Only AC3 is now prepared for its required owner review in session
`f613e282cad0d66a0813a7c85b3954a0`, candidate `BPR-1207-comment`, fingerprint
`966b3f2f5a2b0dc8d0f3ec27a33dcde644a25726bc3d8e2a2f79074d80a3fb2f`, initial
cursor 10. The page has copyable absolute launch and PR fields plus one-action
Wizard steps. It explicitly selects a right-side green line before Comment and
stops after local typing. Both previous owner Rejects stay in this card's
metadata; no decision has been substituted. The other five accepted criteria
are not repeated.

[[BPR-1214]] separately tracks the native Discard/Back return projection: drawer
content disappeared until Next defect restored it. This observation does not
claim a failure to enter the corrected editor, and is not silently dropped.
The runtime handoff names the two live review/Wizard servers, protected bytes,
owner apps, cleanup argv and armed coordinator continuation.

## Owner hand-back 12 — 2026-09-26

The owner **accepted AC3** on the corrected candidate in session
`f613e282cad0d66a0813a7c85b3954a0`, decision
`8d7d2682-13bf-4ff1-970f-c4335f326b16`, event 11, hand-back 12.
The [decision receipt](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/owner-handback12.json)
retains the exact fingerprint, source, executable and timestamp. The note is
empty and there are no attachments. This is the owner judgment of local
writing without publication required by AC3, not publication approval.

AC1 and AC2 retain their reproduced and corrected public-path evidence above.
The frozen executable was rehashed during intake and still matches its recorded
digest. Source, tests and build definitions on integrated main `7ca26dfa` are
identical to reviewed source `c777a33d`; their passing focused, negative-control
and native evidence remains applicable. No product edit or repeated review is
needed. All three original criteria are satisfied; the previous Rejects remain
recorded for their own candidates. BPR-1214, the separate Discard/Back drawer
defect, remains open. Native Windows/Linux and full RFC acceptance remain on
BPR-1135; this card does not close that debt.
