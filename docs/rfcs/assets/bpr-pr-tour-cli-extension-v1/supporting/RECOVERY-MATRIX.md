# What lives where, and what can be recovered

## Canonical authority

| Material | Authority | Fresh machine from GitHub | Same-machine interface switch |
|---|---|---|---|
| Published Problem/Solution | Current PR description (or parent-supported explicitly mapped GitHub text) | Yes, to extent readable and structurally attributable | Refresh through shared importer; preserve local conflicts |
| Published route/order | Selected author review-summary index and ordinary links | Yes, where root/entries remain available | Same parser and canonical remote binding |
| Stops, code subjects, related links, replies | GitHub comments/reviews plus source identities exposed by provider | Yes, available current content; vanished old objects may be unavailable | Same identities and current observations |
| Unsent problem/solution/stop edits and outline order | Profile-private tour store | No, unless separately published; no cloud sync is introduced | Yes, same draft and store, no export/import |
| Private prompts, known AI provenance and author acknowledgements | Bounded local preparation records | No; report unknown, do not infer | Preserve existing records and their qualified origins |
| Undo/redo and local conflicts | Shared local semantic edit journal | No | Yes for retained committed history; visible retention limit |
| Current section/stop hint | Local presentation-neutral resumption hint | Not guaranteed; not part of published meaning | Best effort without redirecting active clients |
| Pending uploaded review/comments | GitHub PENDING review plus local publication journal | Author may recover exposed bytes; complete WIP/attempt ownership is not guaranteed | Reconcile with retained exact local journal |
| Live publish grant | Current application authorization session | Never | Never transferred as durable approval; remaining writes need valid authorization |
| Credentials and hook preferences | Existing profile/agent configuration | Not recovered from PR | Same profile policy, no credential copies in the skill |

These are design guarantees/limits. GitHub documents pending reviews and author-only pending line comments; those facts do not make every local draft field a server-synced field. The parent pack already distinguishes local draft, remote staging and submitted content.

## Local versus staged versus published

1. Writing, adding, moving, undoing and saving WIP stays local.
2. Explicitly authorized publication may upload a PENDING review to obtain real IDs/URLs, wire links and verify membership. These bytes are now on GitHub, even before collaborator visibility.
3. COMMENT submission makes the review the shared tour, subject to parent fallback/partial publication limits. PR-description edits and later multi-comment maintenance remain separate operations.

Stopping at step 2 does not publish; it may leave recoverable remote staging. It also does not establish that everything on the local draft exists there. Never say an unsent field is backed up merely because some comments have been staged. Never use automatic pending-review creation as autosave.

## Cold-open algorithm

Use the parent importer, not a new parser: resolve the exact host/repository/PR identity; read raw description and complete candidate index observations; select an unambiguous root or present choices; traverse its ordered comment links with validated same-PR ownership/type; page necessary comments/discussion and retrieve requested source lazily; expose all missing/truncated/ambiguous material; create a local draft baseline without a remote write. Reading another author's tour does not imply permission to edit it.

A website-created or edited route can be mapped manually with the same parent behavior. An index deleted or edited beyond interpretation cannot be reconstructed from private memory as fact. Present available comments and allow a new deliberate route; do not claim the historical order was recovered. A partial fetch must not create a deletion or empty-description update plan.

## Second-machine acceptance

Use clean profile roots A and B with independent credentials/permissions and no copied SQLite, manifest, skill chat, or hidden metadata. Publish through A. On B, recover with CLI, desktop and skill and compare the shared projection: root/order, raw prose, comments, source/subject distinctions, links and accessible replies. Then make a website edit and repeat. Private provenance/undo must be absent/unknown on B. Count all GitHub writes: cold import and navigation must produce zero.

Perform a separate staged-interruption course: A has pending server data and a journal; B has neither local WIP nor grant. B may show available pending material to the authenticated owner but must not silently finish A's plan or erase another pending review. Compatibility qualification belongs to each claimed GitHub/GHES profile. Neither a fixture nor cached data proves access to a real private server.
