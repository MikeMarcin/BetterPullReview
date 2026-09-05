# Supporting Architecture and Data Model

This file is implementation guidance for RFC-BPR-0022. The RFC is normative;
exact C++ and SQL spelling may change during the required live architecture
allocation, but semantic distinctions may not be collapsed.

## End-to-end command paths

### Reviewer adds a private turn

```text
Qt add-turn intent
  -> core validates thread/revision/actor/body command
  -> semantic persistence request
  -> host transaction allocates turn/revision IDs and commits
  -> typed completion
  -> core publishes thread/turn delta
  -> Qt inserts or updates only changed stable identities
```

### Reviewer asks an agent inside a private thread

```text
Qt send intent with exact selected thread/revision/context IDs
  -> core validates session/entry point/staleness/private selection
  -> host reloads and revalidates selected private sources
  -> host transaction commits reviewer prompt + agent_run admission
  -> existing agent_invocation_host dispatches
  -> protocol adapter returns typed stream + metadata + terminal result
  -> host correlates exact run/session/binding/generation
  -> host transaction commits provenance + complete assistant turn/findings
  -> core publishes bounded deltas
  -> Qt presents transcript/provenance
```

The prompt commit precedes process dispatch. If persistence fails, no agent is
started. A result that does not correlate to the exact admitted run is refused.

### Squash to Draft

```text
Qt selection intent (exact revision/finding IDs)
  -> host supplies bounded immutable source observations
  -> core validates sources, digests, completion, scope, target partition
  -> composition workspace is durably admitted
  -> reviewer writes/copies OR explicitly dispatches an agent-assisted squash
  -> immutable candidate revision is stored and presented
  -> reviewer accepts candidate
  -> core creates/updates local draft with composition correlation
  -> host atomically persists draft + composition state
  -> Qt presents draft editor
```

No mutation capability is reachable from this command path.

### Publish

```text
Qt Publish intent
  -> existing RFC-BPR-0008 draft transition/proposal
  -> existing mutation coordinator revalidates target/head/identity/safety
  -> GitHub mutation and server reconciliation
  -> host transaction records successful publication lineage
  -> core applies typed completion
  -> Qt presents ordinary shared GitHub state plus local lineage badge
```

## Suggested core values

```cpp
using private_review_thread_id = strong_id<...>;
using private_review_turn_id = strong_id<...>;
using private_review_turn_revision_id = strong_id<...>;
using agent_run_id = strong_id<...>;
using review_composition_id = strong_id<...>;
using review_composition_revision_id = strong_id<...>;
using review_publication_lineage_id = strong_id<...>;

enum class private_thread_lifecycle : std::uint8_t {
    open,
    resolved_locally,
    archived,
};

enum class private_revision_completion : std::uint8_t {
    complete,
    incomplete,
    failed,
    cancelled,
};

struct reviewer_actor {
    std::optional<github_account_id> Account;
};

struct agent_actor {
    agent_run_id Run;
};

struct legacy_unknown_actor { };

using private_review_actor = std::variant<
    reviewer_actor,
    agent_actor,
    legacy_unknown_actor>;
```

The actor belongs to the exact revision. `edited_from` preserves derivation and
does not transfer actor identity.

## Suggested subject shape

```cpp
using private_thread_subject = std::variant<
    pull_request_subject,
    file_comparison_subject,
    source_line_ref,
    source_boundary_ref,
    semantic_selection_ref,
    change_group_ref,
    alignment_segment_ref,
    move_relation_ref,
    review_conversation_id,
    durable_agent_finding_ref>;
```

Use existing identity types where available. Do not create a second spelling for
an RFC-BPR-0004 identity. A PR-level subject is explicit rather than represented
by a null line.

## Provenance fact model

Do not model provenance as one `std::string Model` plus a confidence bit. Retain
independent facts:

```cpp
enum class provenance_fact_basis : std::uint8_t {
    invocation_requested,
    profile_configured,
    protocol_reported,
    content_self_reported,
};

struct provenance_text_fact {
    provenance_fact_basis Basis;
    std::string Value;
};

struct agent_run_provenance {
    agent_run_id Run;
    agent_session_id Session;
    agent_profile_id Profile;
    std::string ProfileDisplayName;
    agent_protocol Protocol;
    agent_revision_binding Binding;
    agent_entry_point EntryPoint;
    std::optional<provenance_text_fact> RequestedModel;
    std::optional<provenance_text_fact> ConfiguredModel;
    std::optional<provenance_text_fact> ReportedModel;
    std::optional<provenance_text_fact> SelfReportedModel;
    std::optional<provenance_text_fact> RequestedEffort;
    std::optional<provenance_text_fact> ConfiguredEffort;
    std::optional<provenance_text_fact> ReportedEffort;
    std::optional<provenance_text_fact> SelfReportedEffort;
    std::optional<std::string> ReportedToolVersion;
    std::optional<std::string> RemoteSession;
    std::optional<std::string> RemoteThread;
    std::optional<std::string> RemoteTurn;
    agent_run_outcome Outcome;
    bool Complete;
};
```

The exact implementation can avoid repeated basis fields where the member name
already fixes the basis. The important rule is that requested and reported
values are never overwritten into one slot.

## Suggested lineage representation

```cpp
enum class private_derivation_kind : std::uint8_t {
    replies_to,
    edited_from,
    copied_from,
    refined_from,
    composed_from,
    promoted_from_finding,
    adopted_from_incomplete_run,
};

using private_derivation_source = std::variant<
    private_review_turn_revision_id,
    durable_agent_finding_ref,
    review_composition_revision_id>;

struct private_derivation_edge {
    private_review_turn_revision_id Derived;
    private_derivation_source Source;
    private_derivation_kind Kind;
};
```

Cycle checking is core policy over the bounded candidate edge set and compact
ancestor observation. SQLite foreign keys enforce existence/scope, not semantic
acyclicity.

## Suggested store ports

Avoid widening the existing monolithic private observation. Prefer narrow
application ports such as:

```cpp
class private_review_thread_store_port {
public:
    virtual expected<thread_write_receipt, ledger_store_failure>
        commit(const private_thread_persistence_request&) = 0;

    virtual expected<private_thread_catalog_page, ledger_store_failure>
        load_thread_catalog(const private_thread_catalog_query&) = 0;

    virtual expected<private_thread_header, ledger_store_failure>
        load_thread(private_review_thread_id) = 0;

    virtual expected<private_turn_page, ledger_store_failure>
        load_turns(const private_turn_page_query&) = 0;

    virtual expected<agent_run_provenance_page, ledger_store_failure>
        load_provenance(span<const agent_run_id>) = 0;
};

class review_composition_store_port { /* bounded reads and atomic writes */ };
class review_publication_lineage_store_port { /* lookup by draft/server ID */ };
```

Queries carry exact review scope, cursor, generation, and limits. Results state
whether more data exists; they do not silently truncate and pretend completion.

## Suggested schema relationships

```text
review_scope
  1 -> N private_review_thread
  1 -> N agent_run_provenance
  1 -> N review_composition
  1 -> N review_publication_lineage

private_review_thread
  1 -> N private_review_turn

private_review_turn
  1 -> N private_review_turn_revision

private_review_turn_revision
  N -> N exact source revisions/findings through private_review_derivation

review_composition
  1 -> N review_composition_revision
  N -> N exact source revisions/findings through review_composition_source
  0/1 -> 1 durable review draft

durable review draft/composition
  0 -> N publication attempts
  0 -> N successful publication lineage records
```

Store body digests using an algorithm-tagged form. Do not use `std::hash` or a
platform-dependent digest. Use the project's established cryptographic digest
conventions.

## Atomic transaction boundaries

1. **Create reviewer turn:** allocate turn/revision, insert body/actor/edges,
   advance sequence.
2. **Admit agent prompt/run:** commit reviewer prompt, run snapshot, selected
   source IDs/digests, and high-water advances before process launch.
3. **Complete agent run:** update terminal run fields and insert complete
   assistant revision/finding snapshots atomically.
4. **Accept composition:** verify current source digests, insert accepted
   candidate, create/update durable draft, and transition composition in one
   transaction.
5. **Record publication:** correlate mutation attempt, receipt, reconciliation,
   exact draft/composition/body/target, and server ID atomically.
6. **Migrate v5:** create new schema, convert notes, advance sequences, verify
   counts/digests, and retire old authority in one forward transaction.

## Metadata capture strategy

### Request side

Add bounded optional run settings to the invocation request rather than changing
agent profile into a claim about every run. Profiles may expose configurable
defaults, but each admitted run snapshots the resolved requested values.

### Protocol side

Extend `agent_protocol_event` or terminal protocol metadata with fields only
when the protocol documents and returns them. App Server remote identifiers
currently held inside the decoder should be emitted as typed opaque metadata.
External-terminal mode normally reports no structured runtime metadata and must
therefore display unknown unless the configured harness contract adds a typed
adapter.

### UI derivation

A compact label can use this order:

1. protocol-reported value, labeled as reported in details;
2. requested value with “requested” qualification;
3. configured default with “configured” qualification;
4. `not reported`.

Never substitute a content-self-report into the compact authoritative slot; show
it only in details as self-reported.

## Qt information architecture

Recommended shape inside the conversation drawer:

```text
[All] [Shared] [Private]

Private thread list / subject filter
  -> selected thread transcript
     -> stable turn cards
        author/provenance line
        body
        stale/composed/published badges
        revision history

selection action bar
  [Squash to Draft] [Ask Follow-up] [Resolve Locally] [...]

composition workspace
  sources | candidate | target/provenance
  [Write] [Exact Copy] [Ask Agent to Squash]
  [Accept as Draft]          (never Publish here)
```

The ordinary draft editor retains the separate Publish action. Private and
shared records may be shown in one navigational surface, but they remain
separate models/authorities and the local-only status remains visible.

## Incremental projection expectations

- A new stream chunk may update only the active incomplete agent turn.
- Completion replaces that turn's incomplete projection with its terminal
  revision and metadata; it does not reset the transcript or shared
  conversation model.
- A new turn inserts one stable row/card.
- Editing adds a revision and updates one card/history affordance.
- Composing updates selected source badges and one composition workspace.
- Publishing updates local lineage plus normal GitHub synchronization; local
  and shared models reconcile independently by stable identity.

Record model statistics in tests so a full reset cannot pass unnoticed.
