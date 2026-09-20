# RFC-BPR-0013 closeout reconciliation — 2026-09-20

## Disposition

The owner-amended `selected-diff-exact-epoch-pair` milestone is complete.
BPR-0794 and BPR-0795 are Done at `4047f072` and `ef6d2d7f`; BPR-0935..0939
and BPR-0941..0943 retain their earlier accepted evidence. The unusual-target
comparison-basis and partial-pane anomaly remains unresolved in BPR-0944.

RFC-BPR-0013 remains **Proposed**, and BPR-0617 remains **Backlog**. The owner
authorized closeout, but amended only the unusual-target requirement. That
does not waive canonical order or supply missing RFC-wide acceptance evidence.
RFC-BPR-0008, RFC-BPR-0010 and RFC-BPR-0012 are still Proposed; BPR-0507 is the
remaining unfinished explicit dependency of BPR-0617. No other RFC status is
changed by this reconciliation.

## Owner amendment, distinct from observed behavior

On 2026-09-20 the owner directed that the remaining UnrealEngine#1773 issue be
filed for later investigation, that the available right file should remain
useful, and that the missing-earlier message should occupy the left pane. The
owner asked to stop spending this RFC round on that unusual target and move on.
BPR-0944 now explicitly owns all three future outcomes: partial-pane rendering,
left-pane explanation, and Whole PR base-tip versus GitHub merge-base semantics.

The new owner screenshot was inspected directly outside the checkout. It shows
the current refusal above two cleared panes. It is not a successful partial-pane
render and no private pixels/content are committed. The original source/anchor
criterion was pending at `a9492592`; that historical state and every attended
rejection remain intact. The amended criterion accepts the retained loading
observation, signed safe source/anchor proof and explicit follow-up, not a fix to
the anomaly. No new target read, owner retest or attended candidate was made.

## Identity and scope

- Engineering contract `implement-rfc.engineering.v1`, coordinator-implementer
  recipe revision 2; leaf `/root/rfc0013_postreview_r3`, no delegation.
- Mandate `r22-owner-closeout-deferral`; entry `143509c3` on local `main`.
- Immutable RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a` remains an
  ancestor of the candidate. The scoped base-to-head inventory spans core,
  application, capability adapters, Qt, composition and their tests. Interleaved
  work from other RFCs is not reclassified as RFC-0013 acceptance.
- This round changes documentation and board state only. Product candidate
  `a9492592` and signed tests-disabled r23 bytes remain unchanged. The previous
  [signed evidence](rfc-bpr-0013-selected-material-20260920.md) and
  [usable-review evidence](rfc-bpr-0013-usable-review-20260920.md) remain relevant.

## Authored milestone reconciliation

| Milestone | Actual evidence and disposition |
|---|---|
| Default cache byte pressure | Accepted BPR-0635/BPR-0751 evidence: exact 70,353,760-byte public pressure, default eviction, stale-safe revisit, ownership release, 32-epoch/929-read projection repair. BPR-0751's isolated historical outlier was explicitly closed as cannot reproduce, not retroactively passed. `4a2be1b1`, `b1743179`, `42bd1ed1`. |
| Durable agent workspace recovery | Accepted BPR-0622 evidence is complete on Windows, Linux and macOS, not still missing as older frontier notes say. Windows `c94e02b1`, integrated `2ba02ddb`; Linux `780d0a1c`, integrated `48b6422b`; macOS preserved at `8a03a5be`. BPR-0825 is a separate nonblocking hardening follow-up. |
| Retained stress PR read | Accepted historical BPR-0643 known-limit alternative at `8285cb4f`; it never claimed a readable comparison. Later target work remains separately attributed. |
| Large PR returned-commit boundary | Accepted BPR-0792 at `3cefa687` / `edc59774`: total versus returned count, exact 250-record boundary, fail-closed negatives and recorded next refusal. |
| Review-thread null anchor | Accepted BPR-0793 at `0d9f3d71` / `17d84c90`: preserved nullable LINE subjects, focused/composed checks and actual next explicit refusal. |
| Selected exact pair and owner amendments | Complete under the explicit owner amendment committed at `a38761b4`, not by inferring private-target success. BPR-0794/0795 Done; BPR-0944 retains the unresolved anomaly. |
| Verify RFC-0013 | Not complete. Canonical predecessors and the original criterion-level gaps below remain. BPR-0617 is the existing owner of this work; no duplicate closeout card is allocated. |

## Original RFC acceptance matrix

These rows supersede the stale current-state claims in BPR-0617's historical
matrix. They retain, rather than silently defer or mark passing, original gaps.

| Criterion | Direct evidence / current result | Remaining acceptance boundary |
|---|---|---|
| Large files, file counts, comments, epochs, binary/generated fixtures | BPR-0623/0624/0631/0632: independently checked 515-file, 20,000-line, 1,000-thread, 32-epoch workload; exact public sources/refs/catalog. `ee1717c2`, `e9da7abc`, `d0047177`, `75d1494e`; later 929-read course retained. | Fixture and recorded Mac public-path claim covered; not a universal performance result. |
| Responsive input, scrolling and navigation during work | BPR-0627..0629 and BPR-0751 repaired measured pauses; Mac refresh maxima 64.617–91.147 ms and source below 0.952 s in the final history course. BPR-0938/0795 and current held-read paint checks show useful Overview before terminal acquisition. | Other-platform measured performance budgets and full RFC-wide active-work coverage are not established by these Mac observations. |
| Bounded memory/cache and recovery after close/prune | BPR-0630..0635 prove public prune/delete, loaded close, 64-entry and default 64-MiB byte pressure, weak-owner release and exact revisit. Recorded Mac sampled drain/destruction growth budgets are 160/96 MiB. | Sampled footprint is not an unsampled peak or a Windows/Linux budget. Preserve those platform/measurement limits. |
| Primary/secondary limits, bounded backoff and useful UI | Current provider/pager checks pass; BPR-0161 and the RFC-0010 ledger prove capped backoff, Retry-After, identity/cancellation and explicit typed statuses. Queue checks are rerun below. | The complete rendered RFC-0013 primary/secondary backoff course is not established by scheduler or provider units alone; BPR-0617 retains it. |
| Clearly labelled cached offline data, never false freshness | BPR-0161 proves typed offline/recovery and stale-session refusal; retained Git/catalog history is independently checked. | A composed cached-review offline/restart and visible freshness-label course is still unproven. No new offline mode is claimed. |
| Process-kill Git/SQLite recovery without corruption/duplicate mutation | BPR-0616/0618/0619/0620/0621 retain actual kill, repeated restart, integrity and no-replay checks. BPR-0622 adds native durable workspace recovery on all three platforms. | Reuse the exact platform/course evidence; native workspace evidence does not silently certify every other kill course on every platform. |
| Fuzz all custom parser/mapping boundaries | Deterministic parser, malformed-input and property tests exist. This audit found no registered fuzz targets/corpora in the current CMake/source inventory. | Complete diff/mapping, Markdown, settings/theme, API, Git-output and AI-output fuzz coverage is not demonstrated. The original requirement remains open on BPR-0617. |
| Metadata-only logging under large/error conditions | Current `application_logging`, `quill_logging` and backpressure checks pass; retained large-target diagnosis writes no private material. BPR-0602/0606 cover the foundation. | Do not infer an RFC-wide pathological-input/privacy audit from those selected courses; the remaining coverage inventory stays on BPR-0617. |
| Active/changed-set scaling, serialized application, incremental Qt and stale/cancellation rejection | Retained scale/cache/history evidence and current core/host/drawer/loading tests exercise those paths with exact identities. Original prepared-index and cancellation proofs remain relevant. | A blanket application-wide structural/benchmark claim is not established; broader Qt layout/contract failures remain visible under BPR-0940. |

## Follow-ups and sequencing

- BPR-0934, Backlog under RFC-0037: Overview folder collapse; never absorbed.
- BPR-0944, Backlog: explicitly owner-deferred unusual-target comparison basis
  and partial-pane presentation. Not a required blocker for the amended selected
  milestone and not a fixed issue.
- BPR-0940, Backlog: pre-existing broader composed GitHub/window-layout contract
  failures. Focused passing courses are not a full-suite pass.
- BPR-0945, Backlog: unchanged shared-header strict-tidy diagnostics.
- BPR-0817, Parked: fresh owner-private Linux roots and Mac profile-management
  checks pass; the pre-existing writable-parent refusal visibility/exit-policy
  decision remains open. No owner decision is invented here.
- BPR-0825, Parked: Windows supervisor/ACE/junction fixes and native evidence are
  integrated; shared descriptor ownership is a separate nonblocking design
  question. BPR-0622 stays Done.
- BPR-0617 retains the original matrix gaps and BPR-0507 dependency. No arbitrary
  new card or allocation is needed to restate that existing closeout obligation.

The next canonical closure frontier is RFC-BPR-0008 / BPR-0242. Its BPR-0143
mixed-client and BPR-0144 acting-identity checks remain parked for designated
live GitHub authority; BPR-0641 separately remains human-pending. This assignment
authorizes neither those mutations nor their acceptance. RFC-0014 follows 0013
but is not the next eligible terminal closeout.

The [final selected-milestone checkpoint](../rfcs/plans/rfc-bpr-0013-hardening/checkpoint-rfc0013-selected-diff-d7-r22.json)
and [closeout handoff](../rfcs/plans/rfc-bpr-0013-hardening/handoff-rfc0013-closeout-d7-r22.json)
bind the amended result, complete card inventory and stopping boundary.

## Final verification and ownership

At unchanged product source, the 17-test focused/public-path pass is green in
52.91 seconds: core/provider read policy, epoch/workbench host, held loading,
drawer/diff, strict endpoint fixture, actual selected-file safety composition,
provider/pager, composed logging/backpressure and profile management. The ignored
log is `build/rfc0013-resume-20260919/r24-closeout-focused.log`. Three additional
queue scheduler/Qt/composition checks pass in 4.92 seconds at `ef6d2d7f` in
`r24-closeout-queue.log` beside it; they do not claim a rendered offline course.
All use safe fixtures. Prior signed r23 native proof, source/anchor matches, 5/5 external CLI
fixture tests and relevant scoped formatting are reused, not rerun against the
private target. Strict tidy and the broader Qt suites remain explicitly non-green.

The documentation gate and its 64 behavior tests pass; the scoped board is valid
with 937 total cards. The first documentation attempt correctly refused a renamed
closeout-checklist heading. The required heading was restored with a current
nine-row matrix, and the old matrix retained separately; no verifier rule changed.
Plan structure/reference checks and `git diff --check` pass.

The current round launches no native app or review server. Owner-launched r23
PID 5964 was discovered on entry and is untouched. Session
`cbd48b91000b00f1e72e1ad62c7c384c` remains at 443; only unrelated BPR-0755 is
protected. Historical review packet bytes/links are not edited. The unrelated
`.mask.lock.json` and ignored nested worktree are preserved.

Documentation impact: RFC, index, README, live cards and plan describe the new
acceptance boundary. Core policy, typed requests/events, authority, concurrency,
persistence, provider/authentication, package and security behavior have **no
runtime change** in this documentation-only closeout round. System architecture
continues to describe the currently implemented both-pane refusal, not the
deferred partial-pane design.
