# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **rfc-bpr-0014-release-readiness** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## m1-pack-import-planning

Reconcile paths/IDs, retain the normative architecture, create the live task set, and commit the planning slice.

**Work:** `BPR-0951`

**Prerequisites:** None

**Completion:**

- The immutable source, RFC base, historical associations and owner amendments are reconciled.
- One discoverable adopted plan and live queue cover all authored stages and RFC acceptance obligations without hidden scope reduction.
- Documentation and board checks pass; the bound milestone checkpoint and intentional planning commit identify the next eligible stage.

**Boundary:** Stop after planning acceptance; the coordinator dispatches a fresh implementer for architecture allocation.

## m2-architecture-allocation

Identify authoritative state, compact core state, bounded observations, typed transitions, host ports/effects, Qt projections, async identity and evidence-backed no-impact regions. Inventory and reuse the existing publish-release skill, macos-release preset and release/publish scripts, separating available mechanisms from observed qualification.

**Work:** `BPR-0952`

**Prerequisites:** m1-pack-import-planning

**Completion:**

- The release/maintenance architecture and existing implementation gaps are mapped to exact source paths and typed ownership.
- Each acceptance row has a concrete public check, native/environment requirement and owning work item.

**Boundary:** Finish allocation and any necessary evidence-backed future plan refinement before feature engineering. Owner amendment r3 permits source-backed revision of obsolete implementation prescriptions while preserving intended outcomes, all acceptance exits and explicit native/human gaps.

## m3-deterministic-functional-core

Implement or directly qualify deterministic release-maintenance semantic policy, identity, transitions and tests without effects.

**Work:** `BPR-0953`

**Prerequisites:** m2-architecture-allocation

**Completion:**

- Every required core region is implemented or has a direct evidence-backed no-impact disposition.
- Deterministic policy and refusal tests pass in a Qt/provider-free graph.

**Boundary:** Hand off accepted core policy and explicit remaining host obligations.

## m4-imperative-application-host

Implement or qualify bounded maintenance, migration, diagnostic and lifecycle effects through host-owned capabilities.

**Work:** `BPR-0954`

**Prerequisites:** m3-deterministic-functional-core

**Completion:**

- Required retention/purge/migration/diagnostic effects enforce exact authority and private-root boundaries.
- Failure, cancellation, stale completion, restart recovery and shutdown checks pass without duplicating core policy.

**Boundary:** Hand off the verified host path and remaining presentation/composition obligations.

## m5-qt-presentation-adapter

Implement or qualify diagnostic preview and maintenance controls, typed event translation and incremental projections.

**Work:** `BPR-0955`

**Prerequisites:** m4-imperative-application-host

**Completion:**

- Required user actions reach typed host policy and render truthful terminal results without Qt performing effects.
- Focused rendered keyboard/accessibility/confirmation and incremental-projection checks pass, with direct no-impact evidence where applicable.

**Boundary:** Hand off presentation and composition requirements; platform acceptance remains separate.

## m6-composition-public-path

Reuse the existing authoritative macOS release pipeline, complete demonstrated cross-platform install/package and inventory gaps, and prove the tests-disabled public application selects the required path.

**Work:** `BPR-0956`

**Prerequisites:** m5-qt-presentation-adapter

**Completion:**

- Windows, universal macOS and baseline Linux package definitions, dependencies, SBOM/notices/source offers and checksum generation are implemented.
- Available packaged public paths reach an exact fixture comparison through one alignment controller and DiffLinkMap; unavailable native evidence stays pending.

**Boundary:** Do not turn available-host package mechanics into full delivered-matrix acceptance. Deliver application packages and any install rules they need; independently verify core isolation without inventing a standalone core SDK or general project-embedding support.

## m7-focused-tests-fixtures

Verify core/host/Qt/package contracts through focused hostile fixtures and dependency-breaking public negative controls.

**Work:** `BPR-0957`

**Prerequisites:** m6-composition-public-path

**Completion:**

- Direct checks fail when an essential packaged capability or boundary is broken.
- Exact-candidate focused integration checks pass and tests-disabled package audits exclude fixture seams.

**Boundary:** Retain the exact candidate and useful failures for native and security qualification.

## m8-ux-ui-native-verification

Verify clean-target installation, real rendered interaction, authentication, fixture comparison, uninstall and explicit purge on every supported platform.

**Work:** `BPR-0958`, `BPR-0959`, `BPR-0960`, `BPR-0961`

**Prerequisites:** m7-focused-tests-fixtures

**Completion:**

- Clean Windows 10/11 x64, macOS Intel/Apple Silicon and declared Linux x64 DEB/RPM environments satisfy their complete native card criteria.
- Human decisions, where required by an actual interaction criterion, bind the exact candidate and are supplied through attended review rather than self-certified.

**Boundary:** Preserve partial platform evidence and exact pending candidates; after safe release, independent M9/M10 work may continue.

## m9-security-boundary-verification

Verify signatures/notarization, provenance, hardening, credentials, process/network limits, diagnostic privacy and packaged architecture.

**Work:** `BPR-0962`

**Prerequisites:** m7-focused-tests-fixtures

**Completion:**

- Each exact delivered artifact independently verifies trust, checksums, SBOM, notices and source-offer obligations.
- Release-policy hardening/analysis/sanitizer/fuzz/secret/vulnerability and native runtime/credential/privacy boundary checks pass across the supported matrix.

**Boundary:** Required external signing authority and native evidence stay pending; no live publication is implied. Reuse existing macOS signing/notarization and package-verification mechanisms; inspect or repair demonstrated gaps and obtain exact-artifact evidence instead of rebuilding equivalent tooling.

## m10-documentation-impact

Complete the corporate security packet and affected architecture, UI, storage, build, installation and operator documentation.

**Work:** `BPR-0963`

**Prerequisites:** m7-focused-tests-fixtures

**Completion:**

- Every affected semantic document is updated or has source-backed no-impact evidence.
- The corporate security packet states actual network, storage, subprocess, credential, AI, diagnostic and removal behavior and explicit pending qualification.

**Boundary:** Documentation may proceed independently of unavailable native decisions while keeping claims bounded.

## m11-simplification

Remove avoidable duplication and broad capabilities in the changed implementation and re-verify.

**Work:** `BPR-0964`

**Prerequisites:** m10-documentation-impact

**Completion:**

- A concise simplification/self-review pass is recorded against the changed scope.
- Any corrections pass focused affected checks plus coherent integration evidence.

**Boundary:** Preserve all native, security and predecessor obligations for final closeout.

## m12-verification-rfc-closeout

Accept the owner-amended RFC-BPR-0014 packaging and maintenance foundation, transfer every incomplete original release requirement to RFC-BPR-0038, and close RFC0014 in canonical order.

**Work:** `BPR-0965`

**Prerequisites:** m11-simplification

**Completion:**

- Every amended RFC0014 foundation criterion has direct producing-revision evidence; all original release criteria and BPR0958–0962 obligations remain explicitly pending under RFC0038 without invented acceptance.
- RFC-BPR-0013 and canonical predecessors are Auto-Verified, all RFC0014-scoped cards are Done, and the amended RFC/index/documentation gates agree.
- The final diff against immutable RFC_BASE_SHA, integrated revision, exact protected candidates, command quiescence and cleanup are recorded.

**Boundary:** Owner amendment r4 accepts only RFC0014's implemented foundation. M1–M11 definitions remain unchanged, including pending M8/M9. RFC0038 M13, not M12, now owns complete native and release-security qualification. No publication or independent approval is implied.

## m13-release-qualification-closeout

Complete RFC-BPR-0038's original full native package and release-security acceptance against exact final artifacts.

**Work:** `BPR-0973`

**Prerequisites:** m8-ux-ui-native-verification, m9-security-boundary-verification, m12-verification-rfc-closeout

**Completion:**

- Every original release criterion retained in RFC0038 has complete exact-artifact native, human, independent trust, license/source-access/retention and security-policy evidence.
- BPR0958–0962 and BPR0973 are Done, canonical predecessors are Auto-Verified, and RFC0038/index/ledger/documentation agree without waiver by RFC0014 closure.
- Final candidate and integrated identities, baseline diff, protected-review state, command ownership and cleanup are recorded; publication requires separate authority.

**Boundary:** Separate successor acceptance. Do not claim any native, security, legal or trust pass from the r4 scope transfer or earlier mechanical evidence.
