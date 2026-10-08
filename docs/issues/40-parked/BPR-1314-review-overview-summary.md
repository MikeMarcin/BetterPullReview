---
schema: "implement-rfc.card.v1"
id: "BPR-1314"
title: "Verify composed resizable Overview and prepare owner review"
status: "Parked"
rfcs: ["RFC-BPR-0063"]
depends_on: ["BPR-1313"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r4"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"9fa43a9b3c05b903e015a42b4f25750cf0c7b8d3","evidence":{"kind":"command","detail":"v003 correction: all10 focused Overview/reader/composed rows pass (24s) incl. new toggle-in-reader-corner and reader-right-edge==Files-right-edge assertions in normal/full/return; exact Select All copy, source partial exact, rendered partial equivalent, no HTML MIME retained. Self-contained exact frozen public 3/3 pass 14.16s; clean-export core/architecture boundaries 3/3 pass 11.37s. Format and changed-line tidy pass; 12 offscreen previews inspected (reader and Files share right edge ~962px vs v002 reader ~930px). Native appearance unclaimed."}},{"criterion":"C2","outcome":"pending","revision":"9fa43a9b3c05b903e015a42b4f25750cf0c7b8d3:5e12d562bbcc040bdcdf031093e5daee1759dfa0b6de5f67f6bcf51032a0de0b:0536774777e1f3f25f5f5d96e67743c1c963bb183680456430b10b94ab370aec","evidence":{"kind":"artifact","detail":"v002 Reject consumed (decision 46ae88c6, session 0b30e747 cursor 5, note: horizontal space wasted). Fresh session 3a8115e690eed93b0fa8d23de6e3bbfa cursor 3, candidate BPR-1314-overview-v003 fingerprint 32a89710658fe5904c982defee8d2e0e1571b85ccc4ecdf521401e0f32167d3f, one Accept/Reject question; owner native judgment pending."}}]
parked_on: "human-review"
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
