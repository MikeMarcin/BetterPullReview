# RFC-BPR-0022 live import

Imported on `main`, 2026-09-04. User scope: reconcile and import both supplied
packs; **do not implement them now**. RFC status is Deferred; all imported cards
are Backlog. Source prompts, Proposed statuses, and claimed implementation
permissions remain provenance only and do not override this direction.

RFC base: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.

This is the committed live baseline immediately before import edits, not the
older source dump or a self-referential import commit. The import commit is the
first Git commit adding this record (`git log --diff-filter=A -- <this path>`).
The checkout initially contained unrelated BPR-0559 session diagnostics edits;
their owning session committed them as `5a616354` during inspection. This import
changes documentation only and includes none of those edits as its own work.

## Retained source

- Source folder: `/Users/jaeger/Downloads/betterpullreview_rfc_bpr_0022_off_hunk_comments_v1`.
- Retained pack: [betterpullreview-rfc-bpr-0022-off-hunk-comments-v1](betterpullreview-rfc-bpr-0022-off-hunk-comments-v1/README.md).
- Digest manifest: [SHA256SUMS](betterpullreview-rfc-bpr-0022-off-hunk-comments-v1/SHA256SUMS); SHA-256 `737332e9c387dd7da96577ba5ae3268a359fc32f5bd320cd2d466f9619697552`.
- Source dump SHA-256: `0c73fc6d9b0c19e89ef43d728ef5d7a3bdc86ddf8cc6e2828ae7abf6a0d257d2`.
- Every pack file is retained byte-for-byte, including original RFC/card IDs,
  manifest, order, prompts, and supporting material. No retained file was edited.

## Live destinations and IDs

| Source | Live destination |
|---|---|
| Original RFC-BPR-0022 | [RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md](../RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md) |
| Issue plan | [RFC-BPR-0022-issue-plan.md](../plans/RFC-BPR-0022-issue-plan.md) |
| Verification plan / acceptance traceability | [RFC-BPR-0022.md](../../verification/RFC-BPR-0022.md) |

The two source namespaces collide and are mapped independently. Off-hunk
retains RFC-BPR-0022; private threads becomes RFC-BPR-0023 in user-listed order.

| Provisional card (within this pack) | Live card |
|---|---|
| PACK-BPR-0022-01 | [BPR-0561](../../issues/60-done/BPR-0561-plan-import-and-architecture.md) |
| PACK-BPR-0022-02 | [BPR-0562](../../issues/60-done/BPR-0562-core-publication-placement-policy.md) |
| PACK-BPR-0022-03 | [BPR-0563](../../issues/60-done/BPR-0563-exact-anchor-and-placement-observations.md) |
| PACK-BPR-0022-04 | [BPR-0564](../../issues/60-done/BPR-0564-canonical-permalink-contract.md) |
| PACK-BPR-0022-05 | [BPR-0565](../../issues/60-done/BPR-0565-github-wire-and-unsupported-classifier.md) |
| PACK-BPR-0022-06 | [BPR-0566](../../issues/60-done/BPR-0566-publication-capability-evidence.md) |
| PACK-BPR-0022-07 | [BPR-0567](../../issues/60-done/BPR-0567-logical-publication-recovery.md) |
| PACK-BPR-0022-08 | [BPR-0568](../../issues/60-done/BPR-0568-native-fallback-negotiation.md) |
| PACK-BPR-0022-09 | [BPR-0569](../../issues/60-done/BPR-0569-fallback-normalization-thread-map.md) |
| PACK-BPR-0022-10 | [BPR-0570](../../issues/60-done/BPR-0570-qt-comment-anywhere-ux.md) |
| PACK-BPR-0022-11 | [BPR-0571](../../issues/60-done/BPR-0571-compose-public-path.md) |
| PACK-BPR-0022-12 | [BPR-0572](../../issues/60-done/BPR-0572-automated-security-recovery-verification.md) |
| PACK-BPR-0022-13 | [BPR-0573](../../issues/60-done/BPR-0573-designated-live-native-fallback-evidence.md) |
| PACK-BPR-0022-14 | [BPR-0574](../../issues/60-done/BPR-0574-documentation-and-simplification.md) |
| PACK-BPR-0022-15 | [BPR-0575](../../issues/10-backlog/BPR-0575-anchor-rfc-0022-verification.md) |

The allocator claimed BPR-0561 through BPR-0591 in one operation and advanced
`docs/issues/NEXT_ID` from BPR-0561 to BPR-0592. No reservation or schema number
was allocated. Plans have reconciled identities but remain source-only;
`depends_on` on the live cards is the sequencing authority.

The planning cards BPR-0561 and BPR-0576 retain detailed readiness work for a
later implementation request. Their import portions are complete; they are not
marked Done. BPR-0575 and BPR-0591 anchor pending criterion-by-criterion closeout
with this baseline. Future-RFC cards follow the existing `anchor-rfc-...`
convention because the current `close-rfc-...` structure check resolves only the
fourteen root RFC files. No validator or skill was changed for this import.

## Reconciled shared contracts

1. RFC-BPR-0008 retains draft policy, acting identity, admission, and the one
   coordinator. Neither original RFC-BPR-0008 nor its acceptance criteria change.
2. RFC-BPR-0022 owns native/file representation and logical parent/child recovery.
   RFC-BPR-0023 owns private sources, composition, durable draft linkage, and
   local successful-publication lineage. Its BPR-0584 waits on BPR-0568 and
   BPR-0569; BPR-0587 waits on BPR-0571 for the combined public path. Local thread
   and composition slices have no added off-hunk implementation prerequisite.
3. Approved reviewer prose is byte-preserved. Native wire bytes equal that prose;
   file-level wire bytes add only RFC-BPR-0022's visible canonical code permalink.
   Private provenance never reaches GitHub. Lineage binds both body digests,
   chosen representation, logical publication identity, and canonical server
   receipt; child attempts and local lineage repair never create another POST.
4. The off-hunk source decision table permits direct fallback from known
   unsupported evidence, while its original recovery invariant required a
   rejected native child. The live RFC now defines both durable admission paths:
   exact native rejection, or proven off-hunk/fresh scoped unsupported evidence
   before any native attempt. The latter persists evidence without fabricating a
   rejection and cannot override an existing indeterminate native attempt.
   An exact fully received rejection can resolve a transmission witness; response
   loss and GET absence cannot. Planning and recovery cards carry both cases.
5. PR/range/historical private subjects grant no new publication authority.
   Publication still requires an eligible exact current/right line. RFC-BPR-0021
   owns the general historical shared-comment revision join.
6. Private-thread storage permits a 1 MiB assistant body but pages cap body bytes
   at 512 KiB. The live RFC specifies explicit immutable body continuations with
   progress and full-body eligibility, plus total invocation-input admission.
   BPR-0577, BPR-0580, and BPR-0588 own detailed allocation and boundary evidence.

## Live source reconciliation

The relevant seams still match the pack baseline at the recorded Git base:

| Seam | Live observation and implication |
|---|---|
| `review_ledger_port.hpp` | `local_note_record` still contains ID, optional file/comparison attachment, and note; `private_ledger_observation` aggregates notes/defects. No transcript authority is inferred. |
| `review_ledger_database.hpp` / `.cpp` | Ledger version 5; canonical legacy notes and retained agent sessions remain. Choose the next version during implementation after rechecking all supported origins. |
| `review_mutation_recovery_store.cpp` | Separate recovery schema is version 2; logical parent/child migration belongs to RFC-BPR-0022, independently of ledger schema allocation. |
| `review_round_trip.hpp` / `github_application.cpp` | Draft remains ID, semantic line, body, state; `NextReviewDraft` is process-local (now at lines 6048 and 8848). Durable composition linkage remains future work. |
| `agent_profile.hpp` / `agent_session.hpp` | Profile contains executable/protocol/environment-name policy; session contains ID/profile/exact binding. Neither proves runtime model/effort. |
| `agent_launch.hpp` | Decoder retains RemoteSession/RemoteThread/RemoteTurn internally; `agent_run_result` still exposes no requested/reported runtime provenance. |
| `agent_chat_session_host.hpp` / `agent_finding_host.hpp` | Current chat delegates to the existing invocation path; finding promotion retains the current delivery and local artifact path. These are integration seams, not a durable private transcript. |
| `github_mutation_contract.cpp` | Current compiler/parser remains line/side based with no file-subject variant. |
| Qt workbench / draft / thread map | Existing shared drawer, semantic selection, editor, and thread mapping remain the selected presentation seam. This import makes no UI evidence claim. |

The initial relevant open prerequisite closeouts are BPR-0242 (RFC-BPR-0008),
BPR-0245 (RFC-BPR-0011), and BPR-0507 (RFC-BPR-0012); they are encoded on the
applicable new closeout anchors. RFC-BPR-0004, RFC-BPR-0005, and RFC-BPR-0007
are Auto-Verified. Existing open work and the canonical fourteen-RFC order are
preserved. No new implementation frontier exception is inferred.

The source CMake inventory confirms `core_review_round_trip`,
`application_review_mutation`, `application_review_mutation_application_host`,
`github_mutation_contract`, `sqlite_review_mutation_recovery_storage`, and
`qt_review_draft_editor`. The pack's unsuffixed Qt selectors would miss the
registered `qt_github_thread_map_scale_{100,125,200}` and
`qt_composed_github_scale_{100,125,200}` families; the live planned command uses
those scale suffixes. These tests were not run. Future implementation planning
must re-audit source details, exact schemas, protocol capabilities, and selected
CTest inventory at its own start.

## Deliberate exclusions and verification limits

- Neither top-level implementation-order document becomes a live tracker.
- Supporting audits, architecture sketches, prompts, manifests, and the pack
  validator remain inside the immutable retained pack. The live RFC/card
  corrections above outrank their conflicting text.
- The original ZIP containers and external 26 MB source dump are not copied;
  their extracted pack payloads and supplied dump identity provide provenance.
- No editor files, caches, build outputs, transient logs, screenshots,
  credentials, or private repository content are imported.
- No product source, schema, test implementation, architecture contract for
  shipped behavior, or platform configuration is changed.
- External GitHub documentation is retained as supplied design research; this
  import performs no fresh API/capability validation and grants no live mutation.
- No implementation lane, worktree, build, CTest, UI, agent run, or remote
  mutation was started. Every feature acceptance row remains pending.

Import checks: source digest manifests and exact retained copies; live identity,
status, dependency graph and canonical-criterion coverage; Python 3.14.7
`python3 scripts/verify_documentation.py --root .`;
`python3 tests/scripts/verify_documentation_tests.py`; `git diff --check`.
Results: **PASS**. Both source manifests validated; all 57 retained files are
byte-identical to the supplied folders. The import check confirmed 31 unique
backlog mappings, retained source dependencies, both Deferred index entries,
and all 33 canonical criteria in the pending verification and closeout tables.
All 44 explicit source/test paths in the two source-allocation audits still
exist. The documentation gate returned `DOCUMENTATION_OK`; all 54 documentation
regression tests passed (their negative fixtures intentionally print errors).
Diff whitespace checks passed. These are import checks, not feature evidence.
