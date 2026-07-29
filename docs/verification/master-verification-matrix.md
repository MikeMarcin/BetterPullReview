# Master Verification Matrix

Each live RFC must expand its rows into the repository verification ledger with concrete commands, interactions, results, and evidence paths.

| ID | Claim | Direct surface | Command or interaction | What proves it | RFCs |
|---|---|---|---|---|---|
| PC-1 | GitHub remains shared source of truth | Mixed-client contract harness | Create and modify threads through BetterPullReview and GitHub fixture clients; delete local DB | All shared content remains and refresh converges to GitHub | 0001, 0008 |
| ARC-1 | Native app contains no embedded browser or listener | Linked dependency scan and runtime port inspection | Inspect package dependencies and run app under socket monitor | No WebEngine/Chromium/Node and no listening socket | 0001, 0014 |
| ARC-2 | Core decides, host performs, Qt presents | Full composition integration harness | Trigger a real QAction workflow and inspect intent, observations, transition, request, completion, and incremental projection trace | No widget effect call or host policy duplication occurs | 0001–0014 |
| ARC-3 | Functional core is deterministic and Qt/effect-free | Core-only build and replay suite | Build without Qt/providers and replay event/observation sequences | Same outputs result; forbidden dependencies are absent | 0001–0014 |
| ARC-4 | Async completions are serialized and stale-safe | Manual executor integration | Reorder, delay, cancel, and deliver stale GitHub/Git/diff/AI/store completions | Only matching session/revision/generation changes state | 0002–0013 |
| ARC-5 | Qt models and actions are incremental projections | Qt adapter instrumentation | Change one selection, receipt, comment, command, and queue row | Correct QAction/model signals occur without whole-model reset | 0005–0013 |
| ARC-6 | Routine work scales with active set plus changed set | Structural performance harness | Repeat bounded interactions on small and very large reviews | Work/counts stay proportional to affected records, not total PR | 0004–0013 |
| GH-1 | GHES 3.18 read path is selected | Provider facade with recorded GHES 3.18 contracts | Launch configured GHES fixture and inspect capability/transport trace | Normalized PR data comes from 3.18-compatible REST/GraphQL requests | 0002 |
| GH-2 | Credentials never enter logs or unrelated processes | Auth transport integration tests | Inject canary secrets and inspect logs, argv, environment, diagnostics | No canary appears outside credential boundary | 0002, 0014 |
| EPOCH-1 | Retained epoch survives force-push | App-owned bare cache | Observe head A, force fixture to head B, compare A to B after remote ref removal | Old objects and named epoch remain available | 0003 |
| EPOCH-2 | Left and right selectors compare arbitrary retained epochs | Real workbench | Select base/R2, R1/R3, and last-review/latest | Displayed OIDs and file material match fixture expectations | 0003, 0005 |
| DIFF-1 | Displayed line identity is correct | Diff public model and rendered harness | Navigate known fixture hunks and inspect source OIDs/line numbers | Every displayed row maps to expected old/new line | 0004 |
| DIFF-2 | Connector groups do not invent correspondence | Rendered connector harness | Render unequal and adjacent groups | Bands preserve order, minimize crossings, and match group extents | 0004, 0005 |
| DIFF-3 | Lower pane is true line-over-line selected detail | Real workbench interaction | Select each group and compare lower pane expected rows/spans | Pane spans center width and matches selected old/new lines | 0004, 0005 |
| UI-1 | Primary navigation is top-level and correct | Qt UI automation | Use previous/next file and change buttons and shortcuts | Selection follows filtered actionable order with correct enabled states | 0005 |
| UI-2 | File and conversation panes are collapsible drawers | Qt UI automation | Resize, collapse, reopen, restart | Center width returns and drawer state persists | 0005 |
| APP-1 | System/light/dark switching is immediate and correct | Rendered application | Change OS scheme and explicit mode | Resolved profile and native hints update without reparsing source | 0006 |
| APP-2 | Syntax state is correct across hunk boundary | Complete-blob highlighter fixture | Open hunk inside multiline comment/string | Displayed token roles match full-file state | 0006 |
| LEDGER-1 | Receipts invalidate only when material changes | Domain and SQLite integration | Advance epoch with changed and unchanged files/comments | Only changed material or newer comment versions reopen state | 0007 |
| LEDGER-2 | Private state never crosses account/repository | Persistence integration | Use same PR number across hosts/accounts/repos | No state is reused across strong identity boundary | 0007 |
| ROUND-1 | Published comments are ordinary GitHub comments | Safe mutation fixture | Publish, read through independent GitHub client, reply externally | No BetterPullReview metadata is required; round-trip succeeds | 0008 |
| ROUND-2 | Stale-head mutation is blocked or explicitly revalidated | Mutation coordinator | Move head between draft and submit | User sees mismatch and mutation cannot silently target wrong revision | 0008 |
| DEFECT-1 | Fix claim is not automatic verification | Defect workflow integration | Change code near defect and add author reply | State becomes needs verification; only reviewer action verifies | 0009 |
| QUEUE-1 | Observer rules do not alter GitHub reviewer state | Queue sync integration | Match author/path rule against fixture PR | PR enters local queue and GitHub assignments remain unchanged | 0010 |
| AI-1 | Agent uses its own authentication | Agent launch integration | Launch canary profile and inspect BetterPullReview process/IPC | No GitHub/provider credential is passed by BetterPullReview | 0011 |
| AI-2 | Corporate host cannot launch forbidden personal provider | Host-policy integration | Attempt override on corporate GHES profile | Invocation is blocked with policy explanation | 0011 |
| AI-3 | AI result is bound to analyzed revision | AI workflow integration | Move head during and after analysis | Result records old OID and is visibly stale | 0011, 0012 |
| AI-4 | AI output cannot publish automatically | AI findings workflow | Return finding and suggested comment | Only local proposal is created until explicit publish action | 0012 |
| PERF-1 | Large review remains responsive within budgets | Benchmark and real UI harness | Load configured large PR/file/comment fixtures while navigating/canceling | Budgets and no-freeze thresholds pass | 0013 |
| REC-1 | Crash/restart recovery preserves committed local state | Process-kill integration | Kill during refresh and restart | Database and cache recover; partial effects are reconciled | 0013 |
| PKG-1 | Packages install and launch on supported platforms | Clean machine/VM package test | Install signed package and open fixture PR | App launches and selected provider/runtime paths are present | 0014 |
| SEC-1 | Release evidence contains no secrets or source by default | Diagnostics and release gate | Generate logs, crash/diagnostic bundle with canaries | Only metadata and allowlisted artifacts are present | 0014 |
