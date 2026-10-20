---
schema: "implement-rfc.card.v1"
id: "BPR-1667"
title: "Show the accepted comment bodies in Discuss"
status: "Done"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner v0.3.0 report, 2026-10-19; v0.3.1 release blocker","reservation":"docs/issues/_reservations/BPR-1667--BPR-1667--599178c63b5c.json","milestone_plan":{"id":"bpr-1667-discuss-body","path":"docs/rfcs/plans/bpr-1667-discuss-body/milestone-plan.json"},"milestone_plan":{"id":"bpr-1667-discuss-body","path":"docs/rfcs/plans/bpr-1667-discuss-body/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ffa429dd7af67de7ed2e5b0e5a2aedfeb8ba49f8","evidence":{"kind":"observation","detail":"Final exact-test negative controls substitute only the original818de projection, then only its summary renderer. Raw projection fails both Overview-new-comments GUI and real permalink placement tests (child1, CTest8;1.04s/3.41s); old summary renderer independently fails the empty Approved-summary assertion (child1, CTest8;1.10s). Sources restored byte-for-byte; all3 affected checks pass10.64s. Logs /tmp/bpr-qt612-run/discuss-final-negative-*.log and results.json."}},{"criterion":"AC2","outcome":"pass","revision":"ffa429dd7af67de7ed2e5b0e5a2aedfeb8ba49f8","evidence":{"kind":"observation","detail":"Real Home Open -> admitted read host -> Overview new comments -> Discuss -> exact comment context menu Open in Review preserves raw Markdown, copy bytes and identity. Raw-only changed body under same identity updates; present-empty raw remains empty despite stale display text, absent raw falls back to legacy display. GraphQL root/reply, REST group root and issue comment stay separate. Real raw-permalink placement contract passes. Candidate route passes100/125/200 under normal network-deny wrapper."}},{"criterion":"AC3","outcome":"pass","revision":"ffa429dd7af67de7ed2e5b0e5a2aedfeb8ba49f8","evidence":{"kind":"observation","detail":"Owner confirmed Approved and Commented cards. Empty review summaries explicitly say no overall review comment and name linked file-comment count; exact filename/line link and accessible Review destination are asserted. Empty approval without file comments retains explicit contextual absence; actual empty issue and inline bodies retain No body provided. No body is borrowed. Inspected offscreen summary and original-body renders; existing exact summary-link action dispatch still passes."}},{"criterion":"AC4","outcome":"pass","revision":"ffa429dd7af67de7ed2e5b0e5a2aedfeb8ba49f8","evidence":{"kind":"observation","detail":"At ffa429dd7 final7-row integration passes37.94s: architecture, composed GitHub100, cards, composed Markdown100, raw/summary route100, actual permalink and real application read/reply persistence. Reader100/125/200 passes3/3 in4.27s; same isolated route125/200 passes1.63/2.09s. Strict clang-format22.1.8 checks9 touched files; strict clang-tidy22 passes all8 touched TUs (5 unchanged passing units from r2,3 final changed). DOCUMENTATION_OK;67/67 verifier tests; diff check. Release owner retains nativeWindows/Linux/package qualification."}}]
---

# Show the accepted comment bodies in Discuss

Mandate `discuss-body-blocker-r1`, assignment `/root/discuss_body_fix`.
Baseline `818de796b1f06b02cc5dda24e0ed86363c413154`.
The owner opens Overview -> new comments -> Discuss and sees many comments
saying “No body provided.” Opening the same comment in Review shows its body.
Preserve the existing design, exact content identity and actual empty-body state.
Use sanitized deterministic fixtures and offscreen product paths; no live GitHub,
private data, owner desktop, release version or publication actions.

## Acceptance

- AC1: Reproduce the mismatch through the production Overview new-comments route
  and show a deterministic regression fails against the old code.
- AC2: Discuss and Review show the same accepted body for the same comment;
  changed content and reused identities update without stale or cross-body text.
- AC3: Actual empty bodies retain their empty state; independent bodies and
  original Markdown source remain exact.
- AC4: Focused public-path and adjacent checks, touched-file coding checks and
  documentation gates pass; commit the result and hand off quiescent commands.

## Diagnosis and owner clarification

The owner confirmed seeing both Approved and Commented cards. Those can be
review summaries with no overall prose while independent inline comments exist.
Their empty state now names the missing overall comment and points to the exact
linked file comments without borrowing any body. Actual empty inline/PR bodies
retain their explicit empty state.

A separate deterministic source mismatch is reproduced: the GraphQL record
holds plain `bodyText` in `Body` and original Markdown in optional `RawBody`.
Ordinary projection used only `Body`; selecting a file can recover raw reviewer
prose through permalink placement. A raw-present/display-empty fixture fails old
production through Home Open -> accepted read host -> Overview new comments ->
Discuss. Projection now prefers present raw Markdown, including explicit empty;
only absent raw observations fall back to legacy display text. Raw-only edits
under the same comment identity republish exact content. This source mismatch
is verified independently; it is not claimed to explain every owner summary.
