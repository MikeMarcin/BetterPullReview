---
id: "RFC-BPR-0049"
title: "Label Counters, Resolution-Aware Overview and Settings"
status: "Proposed"
area: "BPR"
milestone: "Post-tour feedback conventions"
depends_on: ["RFC-BPR-0047", "RFC-BPR-0018", "RFC-BPR-0024", "RFC-BPR-0037"]
pack_version: "conventional-comments-v1"
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "proposed_id_requires_live_reconciliation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# RFC-BPR-0049: Label Counters, Resolution-Aware Overview and Settings

## Outcome and scope

Extend the existing PR Overview file table with optional per-label counter
columns selected in Settings. Keep the existing comment counter as a catchall
for anything not broken out. Support all twelve labels, even when none has a
visible dedicated column. Do not redesign Home or invent per-developer quality
scores, label analytics dashboards, or review-performance metrics.

The current-root source already projects `ThreadCount` and
`UnresolvedThreadCount` in `overview_file_row`. Extend the shared semantic
projection rather than implementing separate counting inside delegates. The
source audit identifies an important distinction: GraphQL review threads carry
resolution, while anonymous REST comment groups deliberately do not. Preserve
that distinction instead of converting a default false boolean into a fact.

## Count logical conversations, not replies

One normalized shared conversation contributes one unit. Its root post supplies
its label under RFC-BPR-0047. Ten replies do not turn one issue into eleven issues.
The same thread appearing in provider paging, a tour index, graph and source
surface contributes only once by exact scoped identity.

Replies still receive their own visual labels. Separate post-level search may
match them, but a counter's drill-down uses the same root classification,
conversation identities, scope, resolution selection and accepted revision as its
count. UI text must explain that these are conversation counts.

Private notes, unsent drafts and pending local publication attempts are not shared
counts. They retain existing dedicated private/draft surfaces. A confirmed post
enters the shared population once its identity is reconciled, not once per retry.
Deleted conversation units leave the current population; retained historical
records remain history. A still-existing thread with a missing/deleted root uses
catchall classification, preserving its independently known resolution.

## Disjoint label partition

Let `S` be the selected set of dedicated labels, and `T` the included normalized
conversation population for one scope and resolution mode. A recognized root
label in `S` goes to its corresponding dedicated column. Every other member of
`T` goes to catchall: supported but unselected labels, ordinary prose, unknown or
malformed labels, and unavailable root classification.

For an exact included population:

`sum(dedicated[label] for label in S) + catchall == included_total`.

A separate explicitly named overall total may be shown as a total, but is never
called the catchall. Enabling Issue moves those units out of catchall; disabling
it returns them. No double counting, reclassification on GitHub or data migration
is involved. With no dedicated columns, catchall equals the included total. With
all twelve enabled, catchall still holds plain/unrecognized/unavailable entries.
Folder counts aggregate disjoint child conversation identities; they do not sum
already-overlapping ancestors. Sorting uses numeric counts and an explicit
unknown/partial ordering, not rendered strings or icon names.

## Resolution is not blocking or read state

Carry four disjoint facts for every classified bucket:

| State | Meaning |
| --- | --- |
| `unresolved` | Provider positively reports an unresolved native review thread |
| `resolved` | Provider positively reports that thread resolved |
| `unknown` | Resolution cannot currently be established for a potentially resolvable thread |
| `not_applicable` | This conversation surface has no native thread-resolution concept |

The initial primary numeric mode is `unresolved`, which includes only positively
known unresolved units. Offer an explicit include-resolved/all mode. In all mode,
include the complete admitted population and retain the state breakdown; unknown
and non-applicable units must remain distinguishable from both resolved and
unresolved. The UX decision chooses a compact representation and the exact name
of that mode. It must not present an all-state sum as an unresolved count or
present non-applicable comments as completed work.

Unknown/non-applicable units excluded from unresolved-only numeric counts remain
discoverable through a clear accompanying status and route to those conversations.
A source with no resolution information must not look like a clean "0 unresolved"
review: it reports that unresolved status is unavailable. When some states are
known, show that coverage is partial where relevant. A genuinely complete empty
scope can show zero. Classification-budget failures are also explicitly qualified
rather than silently hiding possible Issue or Question entries in a reassuring
zero dedicated count.

A `praise` or `note` on an unresolved native thread still counts as unresolved.
A blocking issue on a resolved native thread is not counted in unresolved mode.
Neither condition changes the personal defect workflow. Read, accepted, outdated,
collapsed, verified and non-blocking are independent facts, not resolution
aliases. Incoming reopens and resolutions update the appropriate buckets.

## Scope and unavailable content

The file table counts only conversations bound to that exact file identity by
the existing normalized anchor/location contract. Outdated anchors are still
counted when the historical association is exact. Do not move a missing anchor
to the nearest changed line or hide it because it is outside a current hunk.
For renamed/deleted files use existing exact retained identity; uncertain mapping
belongs in a PR-wide unanchored category rather than a guessed row.

PR-wide projection includes those file-associated conversations plus unanchored
native threads, top-level PR discussion and review summaries admitted by the
existing conversation model. Those last two are `not_applicable`, not unresolved.
Do not attach a review summary to every inline comment in its review. An off-hunk
permalink publication uses its existing identity and native resolution capability;
do not fabricate one from the link. Tour index links are references, not duplicate
conversation units. A plain tour root remains catchall; a deliberately labelled
root uses the selected label bucket. There is no silent tour-only exclusion.

The equality above applies independently to each declared scope. File totals
need not equal the PR-wide total when unanchored/top-level units exist; expose the
remainder rather than falsifying file attribution. All modes must use the same
published normalization contract, so toggling columns cannot change scope.

## Completeness, refresh and settings

Counts carry accepted source generation and collection coverage. Partial paging,
rate limits, missing comment bodies, stale retained snapshots and unavailable
permissions are not complete totals. Either retain a qualified prior complete
snapshot or present a qualified partial projection under existing product policy;
never relabel one as fresh and exact. Deduplicate before aggregation; apply a
coherent snapshot or delta transaction so a moved unit is not counted in both old
and new buckets. Concurrent website edits update root labels independently of
commit/head changes.

Settings support an ordered unique subset of the twelve IDs, initially empty,
and a default resolution mode of unresolved. Separate these from the existing
Comments drawer's unread/resolved filter; do not change that unrelated preference
by accident. The active overview resolution selection belongs to existing view
state ownership. The selected default, supported column ordering and persistence
scope are exposed through the existing typed profile settings host/store;
RFC-BPR-0050 decides the controls, not a second persistence mechanism.

Migrate existing settings with defaults only when fields are absent. Preserve
other settings, custom agent hooks and per-window state. Invalid duplicate/unknown
column IDs produce a safe, explicit recovery or validation error; they do not
silently delete valid persisted configuration. Successful setting edits propagate
to windows sharing the applicable profile; another profile is unaffected.
Concurrent edits use existing versioned write/conflict behavior. A failed save
must not claim persistence. Reset is scoped to this feature and reversible where
existing settings transactions support cancellation.

## Cost and architecture

Core owns inclusion, partition and aggregation policy and produces inspectable
results. Application adapts the accepted read model into core inputs, owns parse
caches and accepted generations, and publishes bounded deltas. Qt renders already
computed counters, icons, tooltips and navigation intents. Changing visible columns
repartitions cached catalog buckets; it does not reparse every post or fetch more
provider data. A root edit adjusts one conversation's contribution and relevant
ancestors; unchanged bodies do not cause document resets.

Build counts from one normalized traversal plus a fixed twelve-label catalog,
not twelve scans or requests. Keep resource limits and overflow checked; failed
preparation retains an honest unavailable state. Counter columns must not cause
N-by-label network work, eager loading of every body outside existing bounds, or
full-workbench refreshes per edited token. Performance evidence uses the existing
large-review workload and a larger synthetic core workload within justified limits.

## Acceptance

- R20: Settings can select/order dedicated counters for every supported label independently of support or authoring mode.
- R21: Dedicated buckets and catchall form an exact disjoint partition for every selected subset and declared scope.
- R22: One logical conversation counts once by root identity; replies, pages and links cannot inflate it.
- R23: Unresolved is the initial primary mode; including resolved retains a discoverable resolution distinction.
- R24: Unknown/non-applicable resolution and incomplete classification/collection coverage are explicit, never false clean zeros.
- R25: File, folder and PR scopes preserve exact attribution, unanchored remainder, tour roots and identity deduplication.
- R26: Website edits, deletes, reopens, partial paging and stale observations update only the accepted relevant population.
- R27: Counter drill-down and sorting use the same numeric population and snapshot semantics as the displayed counts.
- R28: Settings persist through the existing typed authority with scoped reset, cross-window propagation and profile isolation.
- R29: Cached classification and bounded incremental projection avoid per-paint parsing, per-label fetches and unnecessary resets.

`fixtures/counter-cases.json` and `specs/COUNTER-EXAMPLES.md` give concrete expected
results. The pack validator exhausts all 4,096 label subsets against the normalized
fixture oracle; implementation must independently exercise the real core and
composed Overview path.
