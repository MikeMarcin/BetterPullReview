# Acceptance traceability

All **43** RFC acceptance requirements are mapped below. Product status is **Not run** for every row. This pack has not implemented or exercised BPR. Course names are proposed integration-test names, not assertions that those CTest selectors already exist. The detailed machine-readable matrix contains expected scenarios and evidence classes.

| Requirement | Owning card aliases | Behavioral proof required |
|---|---|---|
| `AC39-01` | PACK-TOUR-04, PACK-TOUR-16 | Complete the entire manual fixture with no agent profile or agent process; inference count is zero. |
| `AC39-02` | PACK-TOUR-04 | Round-trip raw Markdown through editing/preview; preview issues no writes or resource fetches. |
| `AC39-03` | PACK-TOUR-02, PACK-TOUR-05 | Reorder duplicate titles, split/merge prose and related links, undo local structure operations; identities remain stable. |
| `AC39-04` | PACK-TOUR-02, PACK-TOUR-08 | Select real contiguous lines and an unchanged related contract; never derive an anchor from a visual row. |
| `AC39-05` | PACK-TOUR-03 | Autosave/restart against real SQLite; race two windows; deny another profile; retain text on failed save. |
| `AC39-06` | PACK-TOUR-04, PACK-TOUR-05, PACK-TOUR-23 | Native keyboard/IME/screen-reader/scaled layout and navigation with zero remote status effects. |
| `AC39-07` | PACK-TOUR-04 | Disable rendering, source reads, auth and AI separately; retain readable/editable manual draft state. |
| `AC39-08` | PACK-TOUR-02, PACK-TOUR-23 | Exercise 1/12/100/256 stops, per-field and aggregate limits, lazy source loads and explicit refusal above bounds. |
| `AC40-01` | PACK-TOUR-06 | Render every published object in the website; all explanations/order are normal comments and visible Markdown. |
| `AC40-02` | PACK-TOUR-06, PACK-TOUR-07 | Shuffle server responses and use repeated list numbering/duplicate titles; follow root textual order by comment identity. |
| `AC40-03` | PACK-TOUR-07, PACK-TOUR-16 | Delete local fixture state and reopen from fully paged GitHub objects with zero inference requests. |
| `AC40-04` | PACK-TOUR-06 | Edit mapped Problem only in a CRLF template containing fences, quoted headings and disclosures; untouched bytes match exactly. |
| `AC40-05` | PACK-TOUR-06, PACK-TOUR-07, PACK-TOUR-22 | Duplicate IDs, foreign PR/account links, malformed URLs, incomplete pages, ambiguous roots and nonowned comments never enable writes. |
| `AC40-06` | PACK-TOUR-08 | Native multiline, off-hunk file fallback, original/base range, wholly untouched file and unavailable source retain honest subject kinds. |
| `AC40-07` | PACK-TOUR-16, PACK-TOUR-23 | Website-only reviewer follows actual index, next/previous/overview links and replies under the normal comment. |
| `AC40-08` | PACK-TOUR-05, PACK-TOUR-15 | Leave route, inspect a caller, return; resolved/outdated/unavailable source remains visible; no viewed/resolve/approve effects. |
| `AC41-01` | PACK-TOUR-09, PACK-TOUR-12 | Create with event omitted, verify unsubmitted state, then submit the exact review once with COMMENT. |
| `AC41-02` | PACK-TOUR-09, PACK-TOUR-10 | Reorder/create response records and inspect all pending members; no guessed URLs, positional pairing or missing/extra stop succeeds. |
| `AC41-03` | PACK-TOUR-10, PACK-TOUR-12, PACK-TOUR-22 | Existing pending review, extra website comment, external early submit/delete, actor switch and head movement stop unauthorized effects. |
| `AC41-04` | PACK-TOUR-11, PACK-TOUR-23 | Native and admitted CLI produce equivalent normalized operations; unavailable CLI platforms stay refused; body never enters argv/token logs. |
| `AC41-05` | PACK-TOUR-10, PACK-TOUR-22 | Kill before and after every creation/edit/submission witness/receipt; reconcile without a second visible creation or submission. |
| `AC41-06` | PACK-TOUR-08, PACK-TOUR-09, PACK-TOUR-12, PACK-TOUR-23 | Qualify file-subject staging; unsupported batch and historical/untouched fallback require explicit non-atomic choice. |
| `AC41-07` | PACK-TOUR-12 | Fail description update after review success; retry only the known body-update step and preserve published tour IDs. |
| `AC41-08` | PACK-TOUR-09, PACK-TOUR-10, PACK-TOUR-11, PACK-TOUR-22 | 403/404/422/429, partial pagination, GraphQL errors, oversized/invalid payloads, redirect changes and cancellation remain explicit. |
| `AC41-09` | PACK-TOUR-12, PACK-TOUR-16 | Run Prepare → Preview → Publish and reopened Resume through the shipped application controls; no bypass effect path. |
| `AC42-01` | PACK-TOUR-14, PACK-TOUR-16 | Website edits then refresh repopulate fields; local divergence is preserved as conflict without AI. |
| `AC42-02` | PACK-TOUR-13 | Move one stop and insert a later-batch stop; unchanged comments receive no edits and all original IDs/replies survive. |
| `AC42-03` | PACK-TOUR-13 | Retire a replied-to stop, split/merge or retarget it; shared history is preserved and replacements link to originals. |
| `AC42-04` | PACK-TOUR-14, PACK-TOUR-22 | Three-way raw-body merge and heading-offset drift; simulate a write racing website changes and report limits without automatic rollback. |
| `AC42-05` | PACK-TOUR-13, PACK-TOUR-22 | Interrupt after new destinations, navigation patches and root update; partial results remain visible and resume without duplicates. |
| `AC42-06` | PACK-TOUR-15 | Force push/rename/move/changed caller with identical snippet; retain original identity and require attention rather than silent reanchoring. |
| `AC42-07` | PACK-TOUR-03, PACK-TOUR-14 | Cold import and profile purge preserve shared tour; deleted root/lost private provenance stays unknown and no server delete is issued. |
| `AC42-08` | PACK-TOUR-15, PACK-TOUR-16, PACK-TOUR-23 | Website-only reader navigates updated tour and replies; BPR local change highlights do not imply reading on another machine. |
| `AC43-01` | PACK-TOUR-17, PACK-TOUR-20 | Manual entry, edit, settings reload, selection, preview, import and publish each produce zero implicit AI invocations. |
| `AC43-02` | PACK-TOUR-17 | Override/reset all four hooks, reopen settings, change profile and explicitly test a sample without publishing. |
| `AC43-03` | PACK-TOUR-18, PACK-TOUR-19 | Selection beats whole-field even for whitespace; toolbar keeps selection; caret-only and empty-field paths choose correct modes. |
| `AC43-04` | PACK-TOUR-18, PACK-TOUR-19 | Multiline CRLF, emoji/surrogate, combining and Markdown-boundary splice preserves exact prefix/suffix bytes; reject invalid boundaries. |
| `AC43-05` | PACK-TOUR-18, PACK-TOUR-19 | Adjacent typing + Enhance + Undo/Redo uses one distinct edit; exact selections restore, equal/error output adds none, redo makes zero calls. |
| `AC43-06` | PACK-TOUR-18, PACK-TOUR-19, PACK-TOUR-22 | Edit then undo back to same text before result; IME, changed cursor, superseded request, hidden editor and account/PR change never misapply output. |
| `AC43-07` | PACK-TOUR-20 | Empty field recovers explicit PR text first; inference is distinguished; missing rationale does not invent facts or trigger Jira/issue lookup. |
| `AC43-08` | PACK-TOUR-21 | Reject body/Ready output in skeleton; open placeholder then leave before result; require current visible stop completion and no bulk bypass. |
| `AC43-09` | PACK-TOUR-20, PACK-TOUR-21, PACK-TOUR-22 | Injected instructions in source/prose/settings/model result cannot broaden span, call tools, publish, mark Ready or leak credentials. |
| `AC43-10` | PACK-TOUR-19, PACK-TOUR-20, PACK-TOUR-23 | Native editor uses actual undo history and real agent delivery seam; model quality is evaluated separately from canned protocol tests. |
