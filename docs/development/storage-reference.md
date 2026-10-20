# Storage and diagnostics reference

Engineering detail behind the [storage, diagnostics and removal
guide](storage-and-diagnostics.md): store internals, diagnostic record format,
export parsing and maintenance recovery. The
[maintenance contract](../architecture/release-maintenance.md) records typed
authority, failure ordering, direct tests and remaining native evidence; the
[native qualification ledger](../verification/RFC-BPR-0014.md) records open
clean-account, credential-vault, crash-privacy and complete-removal courses.

## Profile root

The base location is Qt's `AppLocalDataLocation` with organization and
application both named `BetterPullReview`. The legacy/default profile uses
that base; isolated profiles use `profiles/<opaque root identity>`. Root
admission rejects unsafe permissions and symlink/reparse substitutions.
Native private directories and files use owner-only modes on POSIX and
validated private ACLs/handles on Windows. A kernel-held exclusive runtime
lease prevents two writers owning the same profile.

## Store notes

- **Review ledger.** `review-ledger.sqlite3` stores RFC-BPR-0023 private
  threads, immutable turns, run provenance, exact composition sources, local
  drafts and publication lineage. Schema 9 migrates supported v1-v8 ledgers
  atomically; old notes retain their bytes and become `legacy_unknown`
  revisions. Ordinary draft text is in `private_draft.body`; a
  composition-linked draft references `private_content.body` through its
  `content_id`. Diagnostic output uses identities and digests rather than
  private bodies or credentials.
- **Body images (RFC-BPR-0084).** Encoded image bytes are keyed by a digest of
  host, account and source.
- **Retained Git data.** Exact epoch pruning leaves tombstones and does not
  promise immediate byte reclamation.
- **Agent and MCP material.** Original owners reconcile active or interrupted
  work before purge; populated or unknown workspace state can block it. No
  broad sweep is used.
- **`publication-git-v1/`.** Purge includes it by its exact layout: the
  installed Git isolation files, empty work and cache directories, and the
  `publication/v1` bare object stores. Anything else blocks purge as an
  incomplete inventory.
- **Migration.** Each SQLite store validates its application/schema identity
  and performs its own transactional migration. There is no generic migration
  switch.

## Diagnostic recording limits

Normal ingress uses up to 3,584 events / 7 MiB, with reserved warning/error
room inside 4,096 events / 8 MiB. Smaller per-thread Quill queues can drop
earlier. Loss includes contention, capacity and invalid events. These are
bounds, not delivery promises.

Recent sanitized context (three minutes, 512 events, 2 MiB) is retired by idle
backend maintenance once expired, and is also cleared on logger retirement.
**Clear local diagnostics** invalidates pending records before removing
admitted logs.

The accepted selection is shown until the effect and writer barrier complete.
On load, unknown detail/privacy values use Standard/Anonymized and preserve a
valid Off choice; an unreadable store disables capture and editing with
visible safe-default status. Explicit choices can repair readable compatible
rows.

## Diagnostic record format

New records carry an opaque logger `session`, an increasing `sequence`, UTC and
monotonic time, stable event/category/severity and source/toolchain build
identity. [Basic local diagnostics](build-and-test-reference.md#basic-local-diagnostics)
defines these fields and their bounds. Each newly constructed profile logger
creates a fresh session; older records remain unchanged. These diagnostic
identities grant no authority. Anonymized export remaps joins independently for
each preview; identifying export retains the session and captured numeric IDs.

Epoch records carry originating `window_label` and `request_label` joins.
**Include locally** additionally retains numeric `window_session` and
epoch-local `request` (`0` during catalog opening before request allocation).

CLI Git acquisition refusals carry a closed `git_step`, separating tool version
probes/qualification, identity probes, clone, object checks, local store/pack
work and pack reading. Other transports leave it `none`. The field records
where a refusal occurred, not its unobserved cause: `clone` plus `unavailable`
does not establish an authentication or size limit. It contains no executable
path, command output, repository identity or source.

New capability-error details use classified explanations and public query
symbols with explicit unknown-content omissions. Existing retained files can
contain earlier sanitized provider prose and identity information; historical
records are not rewritten or retroactively enriched.

### Actor-conflict provenance

A rejected pull-request snapshot can report `actor_conflict=` with two closed
collection names and `ascii_case_only` or `different`. This identifies where
the observations disagreed without including logins, actor IDs or provider
content. Anonymized and identifying exports use the same closed grammar;
unclassified or malformed detail is omitted. The classification does not make
conflicting observations admissible. Original incident exports without this
field do not establish their actor provenance.

## Export parsing

- Input across the selected directory is bounded to 16 MiB and 65,536 records.
- Each incomplete file tail counts as one omitted record, independently of the
  next file.
- Legacy v1 lines admit at most 8,192 bytes; canonical v2/v3 JSONL records
  admit at most 16 KiB including the newline.
- The JSONL parser accepts the writer's exact field order and bounded ASCII
  scalar grammar, and omits unsupported encodings or extra fields.
- Every rich field is classified again; legacy v1 retains only its small
  metadata prefix. Unrecognized detail is explicitly omitted.
- Output is at most 1,024 records and 256 KiB. Duplicate file/ring events count
  once.
- Export applies the selected privacy policy to every classified field; the
  legacy metadata-only API remains available for compatibility.
- Cancel before the commit boundary is advisory.

## Maintenance journal and recovery

The `.bpr-maintenance-v1` journal binds the original inventory and the
credential receipt. On restart, recovery runs under the runtime lease before
any producer or store opens. It can finish deletion of that exact inventory and
refuses corrupt, foreign, new or replaced objects.

A profile data reset closes the profile's windows so its stores and logger
close and the lease is released. The profile restarts; under the new lease,
after maintenance recovery and before any store opens, the reset records its
exact inventory in the journal and removes it. A crash part-way leaves the
journal, and the next start finishes exactly that inventory.

Profile purge preparation retires windows and drains producers and stores
before inventory. Purge deletes known remembered credential bindings through
their existing owner before removing binding metadata. Do not edit the
journal, reconstruct credential keys from labels or remove a lock to force
admission; obtain an exact-scope operator diagnosis instead.

## Implementation owners

- `src/adapters/sqlite/src/profile_storage_root.cpp`,
  `release_maintenance_files.cpp` and `profile_runtime_lease.cpp`
- `src/application/src/release_profile_cleanup.cpp`
- `src/application/src/release_diagnostic_snapshot.cpp`
- `src/app/src/profile_desktop.cpp`
- `src/adapters/qt/src/release_maintenance_panel.cpp`
