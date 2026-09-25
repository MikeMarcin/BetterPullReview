# Product acceptance matrix

All **90** rows start **Not run**. Archive validation results are separate. Each implementation card must record direct test/gesture, result, producing revision and actual environment.

| Criterion | Requirement | Implementation cards | Closeout | Status |
|---|---|---|---|---|
| RC53-01 | Create and reopen a local cycle with no PR, remote, Git commit, account or agent, including an empty initial diff. | PACK-CYCLE-02 | PACK-CYCLE-44 | Not run |
| RC53-02 | Persist distinct cycle, revision, item, round, response, verification and publication identities without fake provider IDs or counter reuse. | PACK-CYCLE-02 | PACK-CYCLE-44 | Not run |
| RC53-03 | Use bounded provider-independent material/application ports while preserving the existing PR authority and epoch contracts. | PACK-CYCLE-02, PACK-CYCLE-04 | PACK-CYCLE-44 | Not run |
| RC53-04 | Crash during source/catalog/draft/freeze persistence leaves complete recoverable records or explicit unavailable state, never a partial ready round. | PACK-CYCLE-03, PACK-CYCLE-10 | PACK-CYCLE-44 | Not run |
| RC53-05 | Expected revisions and operation IDs reject stale/conflicting writes and deduplicate identical retries across interfaces. | PACK-CYCLE-03 | PACK-CYCLE-44 | Not run |
| RC53-06 | Core cycle services initialize and operate without agent libraries, provider configuration or executable discovery. | PACK-CYCLE-04 | PACK-CYCLE-44 | Not run |
| RC53-07 | Existing PR profiles, caches, receipts and drafts survive additive migration without fabricated local-cycle ownership or altered historical evidence. | PACK-CYCLE-04 | PACK-CYCLE-44 | Not run |
| RC53-08 | Pinned source/round/verification evidence survives publication and quota pressure; explicit removal previews irrecoverable local history loss. | PACK-CYCLE-03 | PACK-CYCLE-44 | Not run |
| RC53-09 | Scope versions and partial coverage distinguish unknown source from deletion and do not imply project-wide review. | PACK-CYCLE-02 | PACK-CYCLE-44 | Not run |
| RC53-10 | Current product/domain/security documentation records the narrow new local/publication scope and unchanged shared-GitHub authority. | PACK-CYCLE-01, PACK-CYCLE-05, PACK-CYCLE-43 | PACK-CYCLE-44 | Not run |
| RC54-01 | Open Git/linked-worktree/dirty checkout, plain folder and single-file scopes without network/authentication or a prerequisite commit. | PACK-CYCLE-06 | PACK-CYCLE-45 | Not run |
| RC54-02 | Capture changes neither source bytes, user index nor refs and runs no hooks, filters, textconv, lazy fetch or repository scripts. | PACK-CYCLE-07 | PACK-CYCLE-45 | Not run |
| RC54-03 | Reject root replacement, symlink/junction escape and unsafe descendant paths under native filesystem semantics. | PACK-CYCLE-06, PACK-CYCLE-41 | PACK-CYCLE-45 | Not run |
| RC54-04 | Detected instability retries finitely; capture-quality metadata never asserts global atomicity from ordinary copying or two matching passes. | PACK-CYCLE-07, PACK-CYCLE-10 | PACK-CYCLE-45 | Not run |
| RC54-05 | Scope exclusions, sparse/missing content, explicit deletions and new scope versions remain distinguishable in manifests/comparisons. | PACK-CYCLE-07 | PACK-CYCLE-45 | Not run |
| RC54-06 | CRLF/LF/BOM/no-final-newline, raw hashes, encoding offsets, file mode and path identities are preserved without lossy normalization. | PACK-CYCLE-08 | PACK-CYCLE-45 | Not run |
| RC54-07 | Binary, unsupported encoding, LFS/filter-managed, symlink/gitlink and platform-collision cases remain visible with accurate capability limits. | PACK-CYCLE-08 | PACK-CYCLE-45 | Not run |
| RC54-08 | Large enumeration/capture/diff access is paged/cancellable and measured without UI-thread hashing or unbounded core state. | PACK-CYCLE-09, PACK-CYCLE-42 | PACK-CYCLE-45 | Not run |
| RC54-09 | Retained material resolves the exact captured bytes after external edits, deletion or restart; corruption/missing pins are not replaced by live files. | PACK-CYCLE-09, PACK-CYCLE-10 | PACK-CYCLE-45 | Not run |
| RC54-10 | Explicit workspace rebind/import mapping prevents the same path/alias in another checkout from silently becoming the review subject. | PACK-CYCLE-06 | PACK-CYCLE-45 | Not run |
| RC55-01 | Author range/file/module/cycle notes and one multi-location discussion from full source or a diff without mandatory per-item metadata. | PACK-CYCLE-11 | PACK-CYCLE-46 | Not run |
| RC55-02 | Original source and ranges remain immutable; current exact/ambiguous/deleted/unavailable correspondence is separately represented. | PACK-CYCLE-11 | PACK-CYCLE-46 | Not run |
| RC55-03 | Plain and all twelve shared labels preserve authored Markdown and do not imply patches, severity, permissions or verification. | PACK-CYCLE-12 | PACK-CYCLE-46 | Not run |
| RC55-04 | Suggestion authoring preserves original/replacement bytes, insertion/deletion, explicit intent, byte ranges and before/after preview. | PACK-CYCLE-13 | PACK-CYCLE-46 | Not run |
| RC55-05 | Exact application refuses stale whole-blob preconditions and overlapping/same-boundary edits; nonoverlapping edits use original coordinates. | PACK-CYCLE-13 | PACK-CYCLE-46 | Not run |
| RC55-06 | Editing a suggestion does not write the checkout; actually applied edits are observed/labeled instead of dispatched for double application. | PACK-CYCLE-13 | PACK-CYCLE-46 | Not run |
| RC55-07 | Native IME, undo, multi-caret/virtual-space and clipboard workflows preserve exact text and selections in the suggestion composer. | PACK-CYCLE-14, PACK-CYCLE-40 | PACK-CYCLE-46 | Not run |
| RC55-08 | Human, adapter-observed, self-reported and unknown provenance remain distinct and private unless selected for disclosure. | PACK-CYCLE-12 | PACK-CYCLE-46 | Not run |
| RC55-09 | Whole-round preparation preserves the reviewer wording and suggestions without a required model call, summarizer or second combined prompt. | PACK-CYCLE-14 | PACK-CYCLE-46 | Not run |
| RC55-10 | Cycle threads reuse the shared narrow private-thread/lineage seam and do not fork or falsely complete deferred RFC0023. | PACK-CYCLE-11, PACK-CYCLE-12 | PACK-CYCLE-46 | Not run |
| RC56-01 | Freeze every selected note, exact suggestion, source binding, explicit action and chosen order into one complete deterministic payload. | PACK-CYCLE-16 | PACK-CYCLE-47 | Not run |
| RC56-02 | Frozen payloads/digests never change; identical freezes reuse identity and edits create a new round/amendment. | PACK-CYCLE-16 | PACK-CYCLE-47 | Not run |
| RC56-03 | Clipboard, Markdown, bundle, CLI retrieval and adapter delivery preserve the same canonical selection and byte semantics. | PACK-CYCLE-17, PACK-CYCLE-24 | PACK-CYCLE-47 | Not run |
| RC56-04 | A human without BPR can understand the complete Markdown assignment and proposed changes; rendering safely handles embedded fences and whitespace. | PACK-CYCLE-17 | PACK-CYCLE-47 | Not run |
| RC56-05 | Bundle/import validation rejects bad hashes, duplicate keys/members, traversal, symlink entries, compression abuse and invalid versions before admission. | PACK-CYCLE-18, PACK-CYCLE-20, PACK-CYCLE-41 | PACK-CYCLE-47 | Not run |
| RC56-06 | Unselected scratch/history/provenance, absolute local paths and credentials do not leak into default outputs. | PACK-CYCLE-16 | PACK-CYCLE-47 | Not run |
| RC56-07 | Copy/save/retrieval/adapter/acknowledgment observations remain distinct and do not imply delivery completion or review acceptance. | PACK-CYCLE-19 | PACK-CYCLE-47 | Not run |
| RC56-08 | Limits/pagination never silently drop feedback; completeness/counts/digest are explicit and oversized assignment partitioning requires a user decision. | PACK-CYCLE-17 | PACK-CYCLE-47 | Not run |
| RC56-09 | Interrupted/repeated freeze/export/import operations recover without partial ready state or duplicate logical round identity. | PACK-CYCLE-16 | PACK-CYCLE-47 | Not run |
| RC56-10 | Export policy applies consistently to all channels, including plain folders of unknown origin, without disguising a denied disclosure as a fallback. | PACK-CYCLE-19 | PACK-CYCLE-47 | Not run |
| RC57-01 | Capture R1/R2 and continue manually without delivery receipt, structured report, BPR-managed agent, Git commit or completion callback. | PACK-CYCLE-21, PACK-CYCLE-24 | PACK-CYCLE-48 | Not run |
| RC57-02 | Full source/This round/Since my last review/All cycle/exact-pair views resolve the intended immutable material and never retarget explicit tabs. | PACK-CYCLE-09, PACK-CYCLE-21 | PACK-CYCLE-48 | Not run |
| RC57-03 | Reconciliation distinguishes exact, ambiguous, deleted and unavailable source and never turns proximity/code disappearance into proof of resolution. | PACK-CYCLE-23 | PACK-CYCLE-48 | Not run |
| RC57-04 | Reports/manual replies attach to exact rounds/items as append-only untrusted recipient statements with observed provenance. | PACK-CYCLE-22 | PACK-CYCLE-48 | Not run |
| RC57-05 | Identical report replay is idempotent; reused report IDs with different bytes and late/wrong-cycle output conflict or refuse. | PACK-CYCLE-22 | PACK-CYCLE-48 | Not run |
| RC57-06 | Recipient addressed status and reviewer verification are separate; verification binds checked item/material and becomes visibly historical when relevant code changes. | PACK-CYCLE-23 | PACK-CYCLE-48 | Not run |
| RC57-07 | Next-round selection/amendment preserves prior assignments and can include unresolved/new feedback without auto-dispatch or forced per-item interaction. | PACK-CYCLE-23 | PACK-CYCLE-48 | Not run |
| RC57-08 | Human-only, mixed-recipient, partial-response and prose-only returns work without JSON/protocol requirements or invented missing responses. | PACK-CYCLE-20, PACK-CYCLE-22 | PACK-CYCLE-48 | Not run |
| RC57-09 | Local capture order, imported source DAG and provider upload chronology remain separate, with gaps/divergence explicit. | PACK-CYCLE-21 | PACK-CYCLE-48 | Not run |
| RC57-10 | Claimed code/test outcomes remain reported evidence; importing them executes no tests, writes no source and grants no verification/publication authority. | PACK-CYCLE-22 | PACK-CYCLE-48 | Not run |
| RC58-01 | Agent-free headless cycle commands run without desktop/GUI/display/provider installation and share the desktop application services. | PACK-CYCLE-25 | PACK-CYCLE-49 | Not run |
| RC58-02 | Normal terminal output is human-readable; explicit JSON, stderr separation, noninteractive refusal and typed errors meet the CLI contract. | PACK-CYCLE-25 | PACK-CYCLE-49 | Not run |
| RC58-03 | Commands bind exact profile/workspace/cycle/round identities; ambiguous targets and active-window changes cannot redirect work. | PACK-CYCLE-27, PACK-CYCLE-29 | PACK-CYCLE-49 | Not run |
| RC58-04 | External reads expose only admitted prepared payload/context and never require database scraping or a generic file/HTTP server. | PACK-CYCLE-27 | PACK-CYCLE-49 | Not run |
| RC58-05 | Desktop-open and CLI-alone paths use one owner with qualified native IPC, bounded messages, peer checks and no competing writer. | PACK-CYCLE-26, PACK-CYCLE-29 | PACK-CYCLE-49 | Not run |
| RC58-06 | Cross-interface edits/freeze/undo preserve dirty text/IME and revision conflicts without lost updates or duplicate application. | PACK-CYCLE-26, PACK-CYCLE-29 | PACK-CYCLE-49 | Not run |
| RC58-07 | Consumers can obtain and verify every instruction page before implementing; counts/digests and limit outcomes are explicit. | PACK-CYCLE-27, PACK-CYCLE-29 | PACK-CYCLE-49 | Not run |
| RC58-08 | The thin bpr-cycle skill uses the active harness, no second model/orchestrator, no direct provider publisher, and no silent skill installation. | PACK-CYCLE-28 | PACK-CYCLE-49 | Not run |
| RC58-09 | Response-only automation appends reports but cannot verify, alter original instructions, export other history or publish. | PACK-CYCLE-27 | PACK-CYCLE-49 | Not run |
| RC58-10 | Manual terminal/Markdown and observed supported harness paths remain useful; missing skill support/reachability does not block the cycle. | PACK-CYCLE-28, PACK-CYCLE-29 | PACK-CYCLE-49 | Not run |
| RC59-01 | Distinguish R0, Rn and publication parent/target and visibly handle preexisting dirty work, selected improvements and base drift. | PACK-CYCLE-30 | PACK-CYCLE-50 | Not run |
| RC59-02 | Candidate/squash preparation uses isolated objects/index and never resets, stashes, cleans, commits or rewrites the user checkout/branch. | PACK-CYCLE-31 | PACK-CYCLE-50 | Not run |
| RC59-03 | Outgoing complete trees preserve untouched base entries; partial scopes, deletions, selected dependencies and empty candidates are handled explicitly. | PACK-CYCLE-30 | PACK-CYCLE-50 | Not run |
| RC59-04 | Bytes/modes/object format/filter/LFS/signing/protected-path requirements are preserved or capability-blocked with truthful external preparation. | PACK-CYCLE-31, PACK-CYCLE-33 | PACK-CYCLE-50 | Not run |
| RC59-05 | Native object/new-branch/PR creation is a separately admitted finite GitHub capability with exact host/account permissions and no implicit credential-helper push. | PACK-CYCLE-33, PACK-CYCLE-34 | PACK-CYCLE-50 | Not run |
| RC59-06 | Exact candidate/destination/title/body/attribution preview plus current authorization is required; edits or changed authority invalidate the plan. | PACK-CYCLE-32 | PACK-CYCLE-50 | Not run |
| RC59-07 | Object/ref/PR uncertainty, cancellation and concurrent drift recover without duplicate PRs, blind retries, force updates or unearned rollback claims. | PACK-CYCLE-32, PACK-CYCLE-34, PACK-CYCLE-36 | PACK-CYCLE-50 | Not run |
| RC59-08 | Squashing outgoing code and publishing retain every pinned local revision, round, response and verification until explicit history removal. | PACK-CYCLE-35 | PACK-CYCLE-50 | Not run |
| RC59-09 | Link the actual PR and reuse existing PR/tour/publication semantics without uploading private history or fabricating GitHub review events. | PACK-CYCLE-35 | PACK-CYCLE-50 | Not run |
| RC59-10 | Native supported-host publication has designated live evidence; external publication/linking stays usable but is not misreported as native qualification. | PACK-CYCLE-34, PACK-CYCLE-36 | PACK-CYCLE-50 | Not run |
| RC60-01 | An agent-free build/package genuinely omits agent-specific libraries/resources/helpers and starts without agent/provider discovery or credentials. | PACK-CYCLE-37, PACK-CYCLE-43 | PACK-CYCLE-51 | Not run |
| RC60-02 | The complete two-round source/suggestion/Copy-Save/capture/verification/publication-preparation course works with integrations absent. | PACK-CYCLE-37, PACK-CYCLE-40, PACK-CYCLE-43 | PACK-CYCLE-51 | Not run |
| RC60-03 | Local automation is separately removable/disableable and does not remove core GUI/manual iteration or destroy stored cycles. | PACK-CYCLE-37, PACK-CYCLE-43 | PACK-CYCLE-51 | Not run |
| RC60-04 | GUI/CLI/file/clipboard/adapter disclosure uses consistent scope/enterprise policy and never treats a skill/provider claim as permission. | PACK-CYCLE-05, PACK-CYCLE-39 | PACK-CYCLE-51 | Not run |
| RC60-05 | Untrusted source/Markdown/imports cannot execute code, mint authority, alter original rounds or mark feedback verified. | PACK-CYCLE-05, PACK-CYCLE-39, PACK-CYCLE-41 | PACK-CYCLE-51 | Not run |
| RC60-06 | Private data, pins, IPC bounds/identity and diagnostic redaction follow documented native boundaries without false same-user sandbox/encryption claims. | PACK-CYCLE-39, PACK-CYCLE-41 | PACK-CYCLE-51 | Not run |
| RC60-07 | Optional Send uses the exact frozen payload, one selected assignment and the same response model as external/manual channels. | PACK-CYCLE-38 | PACK-CYCLE-51 | Not run |
| RC60-08 | Existing provider/harness/account/host restrictions, explicit session selection and separate credentials remain in force. | PACK-CYCLE-38 | PACK-CYCLE-51 | Not run |
| RC60-09 | Late, failed, uncertain or removed-adapter deliveries preserve the cycle and never auto-resend, auto-fallback, auto-capture or auto-verify. | PACK-CYCLE-38 | PACK-CYCLE-51 | Not run |
| RC60-10 | Native installed-package, startup effects, IPC, attack corpus and removal evidence distinguish real qualification from fixtures/cross-compilation. | PACK-CYCLE-39, PACK-CYCLE-40 | PACK-CYCLE-51 | Not run |
| RC61-01 | Open Project/Review Cycles offers a simple no-account/no-agent local entry with a clear Local cycle identity distinct from PRs and Review Tours. | PACK-CYCLE-15, PACK-CYCLE-43 | PACK-CYCLE-52 | Not run |
| RC61-02 | Cycle controls integrate the 0052 title/review bar, folding Updates line, tabs and tray without a new unapproved persistent row or PR regression. | PACK-CYCLE-15 | PACK-CYCLE-52 | Not run |
| RC61-03 | Initial/full-source browsing is single-pane and remains available after comparisons; unchanged/historical local source stays commentable. | PACK-CYCLE-15 | PACK-CYCLE-52 | Not run |
| RC61-04 | Native note/suggestion workflows support primary/related anchors, accurate before/after, keyboard editing and explicit stale/ambiguous states. | PACK-CYCLE-14 | PACK-CYCLE-52 | Not run |
| RC61-05 | Compose/Share prepares one complete assignment with first-class Copy and Save and no mandatory AI setup, second prompt or per-item ceremony. | PACK-CYCLE-19 | PACK-CYCLE-52 | Not run |
| RC61-06 | Whole-round feedback and exact delivery observations remain legible; optional direct/external routes add no hidden context or automatic dispatch. | PACK-CYCLE-19 | PACK-CYCLE-52 | Not run |
| RC61-07 | Capture, response display, comparison, reviewer verification and next-round composition work without callback/protocol prerequisites. | PACK-CYCLE-24 | PACK-CYCLE-52 | Not run |
| RC61-08 | Publication preview differentiates bases and selected final content and links the actual PR while preserving local history/privacy. | PACK-CYCLE-35 | PACK-CYCLE-52 | Not run |
| RC61-09 | Native width/focus/shortcuts/accessibility/IME/tab-restoration courses preserve accepted workbench design and record actual platform/user evidence. | PACK-CYCLE-15, PACK-CYCLE-40 | PACK-CYCLE-52 | Not run |
| RC61-10 | Named-corpus latency/memory and the eight-item/two-suggestion human-steering course measure benefit/limitations without unverified savings claims. | PACK-CYCLE-42, PACK-CYCLE-43 | PACK-CYCLE-52 | Not run |
