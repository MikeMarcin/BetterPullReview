# Source audit and extension boundary

## Basis

This pack is based on the **original review-tour v1 archive already imported by the user**, the supplied **September 22, 2026 root BPR source snapshot**, and the subsequent decisions in this conversation. The original archive and selected source sections were actually read. [SOURCE-BASELINE](../references/SOURCE-BASELINE.json) records full input hashes, exact section locations, and verified predecessor payload checksums. Neither the live checkout nor its imported RFC/card remapping was fetched or changed. The source snapshot predates that import; new tour service symbols must be located at intake rather than invented as observed code.

The source contains embedded `.claude/worktrees/...` copies and historical packs. Only the exact top-level sections named in the baseline are observations of the supplied root snapshot. A repeated filename within an embedded document is not a later root implementation. No private 177 MiB source dump or complete parent pack is redistributed inside this extension.

## Observed seams and authored change

| Observed source | What it establishes | New work, not an existing capability |
|---|---|---|
| `AGENTS.md`; `docs/architecture/system-architecture.md` | Core decides; host performs; Qt presents. Core has no effect/Qt dependencies. Shared profile services and serialized lanes exist. | Another presentation using the same tour semantics, not a new business layer. |
| `src/application/CMakeLists.txt` | Separate static application target linked to Core. | Expose a bounded tour facade and headless composition; not all source is already CLI-ready. |
| `src/adapters/github/CMakeLists.txt` | Concrete GitHub target is gated by BPR_BUILD_QT and links Qt Core/Network; the disabled branch is interface-only. | Split presentation/service gating without replacing qualified transports. |
| `src/app/CMakeLists.txt` | Existing composition links the Qt adapter and other desktop services. | A minimal QCoreApplication CLI target with no GUI dependencies. |
| `src/adapters/sqlite/.../profile_runtime_lease.hpp`; `profile_activation.hpp` | Profile runtime ownership and authenticated fixed activation messages are existing concepts. | Versioned, narrow tour forwarding with request dedupe; current activation is not generic RPC. |
| `src/core/.../revisioned_review_draft.hpp` | Revisioned draft semantics have an existing seam. | Shared committed tour undo/history, multi-interface flush/conflict and semantic session handling. |
| `.mask.json` | Shared AgentSkills source selected for skills, including implement-rfc. | Add pr-tour to the canonical upstream source after CLI implementation; no second repository-local skill system. |
| Original RFCs 0039–0043 | Local drafts, normal GitHub tour comments, cold import, staged publication, maintenance and exact scoped assistance are already specified. | Reuse these through CLI/skill and shared WIP, without re-importing or rewriting their obligations. |

## Corrections to avoid

Do not equate BPR_BUILD_QT off with a functioning headless provider. Do not equate a same-user socket with human or provider attestation. Do not equate a GitHub pending review with the whole local draft. Do not equate a read/ready flag with publication authority. Do not equate cloud recovery with private provenance/undo recovery. Do not call a generated SKILL.md an installed/verified skill or a fixture a live agent observation.

## Live intake

Resolve actual HEAD, parent import receipt/RFC mapping, installed implement-rfc/MASK interfaces, current tour implementation and profile/IPC/store versions once. Allocate proposed RFC0044–0046 only if free; otherwise remap all extension references. Preserve parent history/status and existing work ownership. Record exact missing prerequisites without reopening settled parent RFCs, cloning their cards or broadening this pack into unfinished unrelated work.
