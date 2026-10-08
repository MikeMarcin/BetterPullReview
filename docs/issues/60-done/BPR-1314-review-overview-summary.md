---
schema: "implement-rfc.card.v1"
id: "BPR-1314"
title: "Verify composed resizable Overview and prepare owner review"
status: "Done"
rfcs: ["RFC-BPR-0063"]
depends_on: ["BPR-1313"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r4"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"5136398e39f94b85969785a5ec707a2ab6866316","evidence":{"kind":"command","detail":"v004 source 3e7376512: 10 focused Overview/reader/composed rows plus frozen public 3/3 and boundaries 3/3; rerun at clean export of HEAD 5136398e3 after later Home/read-model commits: qt_pull_request_overview_page, qt_markdown_body x3, qt_composed_markdown x3, application_overview_summary x3, core/architecture boundaries 13/13 pass (35s)."}},{"criterion":"C2","outcome":"pass","revision":"3e7376512:1e7a5c1bd9f4c1169fc45b7bc53db4ef69ba826acbf8d0f2bea85ff7b2d50e6a:0536774777e1f3f25f5f5d96e67743c1c963bb183680456430b10b94ab370aec","evidence":{"kind":"observation","detail":"Owner Accept on native macOS frozen build: session 98de418935ba1aae897bf1379f9771cc, candidate BPR-1314-overview-v004 fingerprint bf8d70c2..., decision e4b2ef3c-5806-44b9-8c9e-50174cfec4ce, no note, handed back cursor 5. VoiceOver and Windows/Linux native not claimed."}}]
---

# Verify composed resizable Overview and prepare owner review

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Focused and rendered public courses cover all AC01–AC05, file activation/review/full-reader semantics and no unnecessary model resets/provider writes.
- C2: Owner judges exact frozen Overview in a fresh short attended session; required native/appearance observations remain explicit.

## Execution record

Later copy decision, mandate `four-drafts-r4`: the owner accepts equivalent
Markdown for rendered partial selections if Select All followed by Copy
returns the complete original Markdown exactly. Source selections retain their
original bytes, and neither mode exports HTML MIME. This supersedes the
intermediate exact-partial-source request; the read-only feasibility leaf was
cancelled before any parser, index, dependency or product changes. The same
Overview owner continues under dispatch `parallel-integrator-r4-003`.


Owner rejected candidate `BPR-1314-overview-v001` in session
`850a898a7ca136bf05449dc7a20a21cf`, decision
`4fd99d90-766b-49ac-8aa1-f8baa9d8199a`, consumed handback cursor 5.
The owner asks for an enter/exit-full-view control at the reader's top-right
and reclaimed space below. Direct amendment: View source belongs in the
reader context menu; remove the Overview Copy source control and make ordinary
selection copying yield Markdown source rather than HTML. These corrections
retain the original resizing, persistence, independent scrolling, full-reader,
file identity and authority obligations. Old evidence remains bound to v001;
it supplies no verdict for the revised candidate. The correction is resumed
under mandate `four-drafts-r2`, integrator dispatch `parallel-integrator-r2-001`.


Unattended Qt/composed courses and offscreen renders pass at source c4e49f971fa9a34004f29eded0bf19f2131d486f; required owner/native judgment stays pending. Fresh ready/protected session 850a898a7ca136bf05449dc7a20a21cf cursor 3 contains one Accept/Reject candidate BPR-1314-overview-v001, fingerprint ec530b0b1949c6f6d574caf0f81785993f28b8e593a300c4ff495e42961bc5b6. Frozen executable SHA-256: 1dcafcb8a6f1d9d4a547c1ec641476c43b97bcf64b841378bac1a4ad94edfeec. See [ledger](../../verification/RFC-BPR-0062-0065.md); the coordinator owns human intake and the handoff server.

## Corrected candidate v002

Overview source `1c5c28ce9` is included in combined source
`980c088733bbed1938ad322f851f5e4e9fc4e7cb`. All ten focused rows pass;
the self-contained frozen candidate's three public scales pass16.34s and three
unchanged architecture/core boundary checks pass12.10s. The full-selection
copy guarantee and approved partial normalization are recorded above.

Fresh session `0b30e747d6abc3cfdb73b8e7753a1b79`, cursor3, contains one ready
protected candidate `BPR-1314-overview-v002`, fingerprint
`3a57f91c6334ee7b3ea4a3df56f7bcf451d84813dcc73fcb47e57823a229da18`.
Executable SHA-256 is
`8555f3b60861b83f7b318d582a4cfbab2f1ba32c2ab24203d8d4b75ae6233623`.
The frozen source/build/cache and launcher are inside their own v002 root.
Required owner/native judgment remains pending. The coordinator owns server
98005/PID15097, durable intake and manual resume; no new verdict is consumed.
The earlier v001 paragraph is historical, and its rejection remains bound to
that exact candidate. See the current [ledger](../../verification/RFC-BPR-0062-0065.md).

## v002 rejection and v003 candidate (dispatch r5, 2026-10-08)

The owner rejected `BPR-1314-overview-v002` (fingerprint `3a57f91c…a229da18`)
in session `0b30e747d6abc3cfdb73b8e7753a1b79`: decision
`46ae88c6-606b-49f1-9e26-35108e49f5d2`, note "God awful ux waste of space
horizontally now for no reason." (an earlier blank Reject `e1222e0e…` was
superseded by it). Consumed through cursor 5; the session was not handed back.
The cause was the full-view toggle occupying its own grid column beside the
reader, so the reader ended about 30 px short of Files.

`9fa43a9b3` overlays the toggle inside the reader's top-right corner, left of
the scroll bar, filled with the reader's base colour. Only the leading
rendered blocks beside it wrap short of it; Exit full view sits in the same
corner. Reader and Files share one right edge. Everything the owner did not
object to is unchanged. Tests now assert the corner placement and equal right
edges in normal view, full view and after returning.

Fresh session `3a8115e690eed93b0fa8d23de6e3bbfa` (cursor 3) holds one
ready/protected candidate, `BPR-1314-overview-v003`, fingerprint
`32a89710…2167d3f`, bound to source `9fa43a9b3`, frozen binary SHA-256
`5e12d562…de0b` and launcher `05367747…aec` in the self-contained
`artifacts/four-drafts-r1/overview-frozen-v003/`. Known limit: source view
does not wrap, so a line ending in the last ~22 px is under the toggle until
scrolled sideways. C2 remains pending on the owner's native judgment.

## v003 rejection and v004 candidate (dispatch r5, 2026-10-08)

The owner rejected `BPR-1314-overview-v003` (fingerprint `32a89710…2167d3f`)
in session `3a8115e690eed93b0fa8d23de6e3bbfa`, handed back at cursor 5:
decision `a4f6a81d-51ca-428a-bd59-67d6f28e9b94`, note "I don't like this fill
color for exit, keep the sytle of the expand just change the icon", with two
screenshots (kept in ignored `artifacts/four-drafts-r1/overview-v003-intake/`).
The full-width corner placement itself drew no objection. The fill came from
the checked state of the checkable tool button.

`3e7376512` makes the toggle non-checkable. Exit keeps Enter's auto-raised
style; only the glyph, accessible name and tooltip change, and the state comes
from the reader's own expanded flag. The frontend-design skill was loaded
first. Tests assert the toggle is not checkable in either state.

One focused run had an intermittent failure: `application_overview_summary_1_25`
stopped once at "file activation reaches a real composed source comparison"
(23.75 s), then passed three reruns in about 4 s each. The failing step is the
real file comparison, not the toggle. The frozen copy passes 3/3.

Fresh session `98de418935ba1aae897bf1379f9771cc` (cursor 3) holds
`BPR-1314-overview-v004`, fingerprint `bf8d70c2…a73c5`, bound to `3e7376512`,
frozen binary `1e7a5c1b…0e6a` and launcher `05367747…aec` in
`artifacts/four-drafts-r1/overview-frozen-v004/`. C2 remains pending.

## v004 owner acceptance (dispatch r5, 2026-10-08)

The owner accepted `BPR-1314-overview-v004` (fingerprint
`bf8d70c2bbee4a617c6728c8b0a3b9db236e78a1a8499730f5cbbf0eee3a73c5`) in
session `98de418935ba1aae897bf1379f9771cc`: decision
`e4b2ef3c-5806-44b9-8c9e-50174cfec4ce`, outcome Accept, no note, handed back
at cursor 5. The accepted candidate is source `3e7376512` with frozen binary
`1e7a5c1b…0e6a` on native macOS. Because code hosting the Overview changed
afterwards (Home and GitHub read-model commits), the ten focused rows were
rerun in a clean export of `5136398e39f94b85969785a5ec707a2ab6866316`:
`qt_pull_request_overview_page`, `qt_markdown_body_{1_0,1_25,2_0}`,
`qt_composed_markdown_{1_0,1_25,2_0}` and
`application_overview_summary_{1,1_25,2}` all pass, together with the core
and architecture boundaries (13/13, 35 s). The intermittent v004
file-comparison failure did not recur. VoiceOver and Windows/Linux native
behaviour are not claimed.
