# Design record

## Scope and provenance

Draft v1 was authored 2026-10-24 against `e82537ad1`, using the owner's
request “pack to integrate mimalloc throughout BPR,” repository build and
package sources named in [the RFC](RFC-DRAFT-0028-process-wide-mimalloc.md),
and the inspected upstream v3.5.3 source archive. No earlier supplied pack
was replaced.

## Decisions

| Decision | Requirement/acceptance | Status and actual answer | Rationale/consequences | Depends on / affected artifacts |
|---|---|---|---|---|
| Scope of “throughout” | Process scope; AC02–AC05 | **Settled by owner in this chat, 2026-10-24:** “Yes, process-wide (recommended)” to standard allocation in every shipped BPR executable, including eligible Qt and other loaded-library calls. | All selected BPR-owned processes, including the macOS update helper, are in scope. External Git, `gh`, agents and OS processes are not BPR executables. | RFC, all native cards, plan. |
| Version and acquisition | Source identity; AC01, AC07 | **Proposed authoring choice:** official v3.5.3 MIT source with the inspected SHA-256 values in the RFC. No owner answer specifically selected a version. | A single pinned source fits BPR's provider and release inventory. Native correctness may require an explicit RFC amendment rather than silently changing versions. | Provider and packaging cards. |
| Build and sanitizer boundary | Runtime topology; AC02, AC06 | **Proposed authoring choice:** mandatory for distribution and default for developer product executables, with a developer baseline switch; incompatible sanitizer/fuzzer override is excluded. | Gives a truthful product scope and a same-workload baseline without corrupting instrumentation. | Provider, native and measurement cards. |
| Performance claim | Measurements; AC08 | **Proposed authoring choice:** measure and report, with no promised speedup or memory saving. | Existing 160 MiB macOS memory-course failures have their own BPR-0967 owner and cannot be relabeled. | Measurement and closeout cards. |

## Open frontier and explicit deferrals

No material product-scope question remains open after the owner's process-wide
answer. The exact platform interception technique, library shape and probe
implementation remain engineering choices; they must pass the native observations
in AC03–AC05. If a platform cannot safely meet them, its card and RFC closeout
stay unaccepted and the implementation must propose an amendment instead of
claiming partial process-wide coverage. Existing RFC-BPR-0038 release/security
qualification and BPR-0967 memory diagnosis retain their own obligations.

## Checks and confirmation

- **Structural validity:** the installed `implement-rfc` offline validator
  reported `Structurally valid; references and semantic coverage unchecked`
  for `milestone-plan.json` on 2026-10-24, and its renderer generated
  `MILESTONES.md`. A pack-local read-only check reported
  `PACK_LINKS_IDS_COVERAGE_OK` for seven unique cards, one closeout,
  AC01–AC09 and local Markdown links. The live selectors and actual RFC/card
  mappings remain unchecked until import.
- **Design readiness:** full scope is defined in the RFC and all proposed cards
  and milestones. No material open choice blocks authoring readiness. Native
  capability is an acceptance condition, not presumed evidence.
- **Confirmation:** the owner explicitly confirmed process-wide scope in this
  chat on 2026-10-24. The complete draft wording and source version have not
  been separately confirmed; blanket confirmation is not required to deliver
  the proposal.
- **Execution eligibility:** not evaluated by authoring. `implement-rfc`
  checks live IDs, dependencies, mandate, platform access and acceptance.
- **Documentation:** `python3 scripts/verify_documentation.py --root .`
  reported `DOCUMENTATION_OK` on 2026-10-24. No C++ build, native allocator
  probe, package, benchmark or release check was run for this draft.

## Continuation

Use [the implementation entrypoint](IMPLEMENTATION_PROMPT.md) for authorized
intake. Reconcile the selected version with current upstream
and repository state before source acquisition; preserve this pack as draft
provenance.
