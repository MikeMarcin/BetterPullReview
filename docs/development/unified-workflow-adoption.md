# Shared workflow adoption — 2026-09-05

AgentSkills RFC-ASK-0003 supplies implement-rfc, attended-review and wizard through MASK links for Claude and Codex. External context preserves the product architecture, index sequencing, public-path hazards and platform requirements. One-off conversion normalized 611 live cards including Done history and 27 RFCs; Markdown bodies and acceptance remain intact. BPR-0153 is queryable as human-review; three live-GitHub waits remain distinct.

Verified old skill/adapters are retained outside discovery in ignored `.mask/bootstrap-retired/unified-workflow-20260905/`. General workflow forks are retired. Specialized RFC-0016/17 evidence validators and rendering assets remain repository-owned under `scripts/verification/rfc0016_attended_contract/`, with source provenance and immutable recorded commands preserved. No migration tooling is installed as a skill.

Guarded MASK deployment/doctor and actual documentation gate passed. Documentation62, relocated historical operator contract4 and public allocator2 tests passed. Live allocation dry-run returned BPR-0620 without mutation. Prepared fixture checks additionally covered RFC0017 44, capture38 plus recorded-fixture coverage, and retained verdict checks. No application build or product-wide revalidation occurred. Shared tools passed native Windows/macOS integration; live host skill loading was not asserted. No push occurred.

## Remote history reconciliation — 2026-09-08

Reviewed the five remote-only commits through `39063364`: four merge commits
and one September 4 remote-station launcher change. The latter modifies four
vendored skill implementation/test files retired by the September 5 adoption
(`683cb32c`), plus their usage documentation. Reconciled that history while
retaining the shared-workflow implementation and current station instructions;
no retired skill files were restored.

The incoming change adds per-runtime fresh-context and service-selection
controls, archived-task detection, and clearer Codex bootstrap/writer errors.
Those are candidates for a separate AgentSkills change, not implemented here:
the currently linked shared launcher does not expose those controls. Applying
only the incoming documentation would advertise unsupported commands. Its
switch back to the compact Claude server also makes fresh station visibility
depend on a controller connection, reversing the immediate-visibility behavior
recorded in [[BPR-0491]]. That behavior change is not adopted with this merge.
The original implementation and tests remain recoverable from `39063364` in
the merged history. No shared-source update or live station launch was made.


The owner subsequently requested the reset controls and compact server mode in
shared AgentSkills. ASK-0047 ports them into the canonical source, preserving
consumer context and clearing old Claude bindings before a fresh launch. The
station usage document now follows that shared implementation. The earlier
merge disposition above records the decision before this explicit follow-up.
