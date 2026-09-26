# RFC-BPR-0019 Verification Plan and Ledger

RFC base: `58b5116f8706ac336664cc3fc39dabc66745a70d`

## Owner amendment to AC1 — 2026-09-25

**The AC1 provenance blocker recorded below is resolved by a scoped owner
amendment; RFC-BPR-0019 stays Deferred until BPR-0295's terminal
reconciliation.** In attended session `06ad5734ed9f7c2fd6d0a62c8328fa0a`
([packet](reviews/owner-review-20260925-fresh-v001/operator-packet.md)),
candidate `BPR-0295-ac1-provenance-v001` at `3759b0d8`, decision `d7af6bd8`
(sequence 35, `actor: human`, fingerprint equal to the prepared one), the
owner amended AC1 for this exact v008 asset set: the recorded unknowns
(creator identity; generator provider, model, seed and version; full prompt
and input-role history; service terms) are accepted as disclosed gaps. The
bound bytes are the provenance, manifest and v008 verdict hashes listed under
[Commands and artifact identities](#commands-and-artifact-identities), all
unchanged. The owner's note names the generator as "ChatGPT 5.6 Sol"; that
statement is recorded on BPR-0295 and the RFC and is not written into
`provenance.json`. The exact wording is on
[RFC-BPR-0019](../rfcs/RFC-BPR-0019-dif-visual-identity-sprite-assets.md#status-and-scope)
and [BPR-0295](../issues/30-in-progress/BPR-0295-anchor-rfc-0019-verification.md#owner-amendment-to-ac1--2026-09-25-attended-session-06ad5734).
The audit below is kept as it was written.

## Ordinary closeout audit — 2026-09-25

**RFC-BPR-0019 remains Deferred; BPR-0295 is blocked on AC1 provenance
completeness after independent review and current graph verification.** The
`rfc0019-20260925-parallel` run, mandate `r1`, selected only its authored final
stage. The entry/product candidate is
`fc4b39e8c3a54ecf931d74071d7793e5e4112496`; the claimed audit revision is
`07f41e25d4bd0793839f15c084e5d2d1e53ee251`. No product, generated asset,
protected historical review or RFC-0052 path changes in this run.

The predecessor blockers in the historical section have cleared: RFC-BPR-0006
and RFC-BPR-0013 are Auto-Verified with terminal BPR-0115 and BPR-0617.
Every BPR-0287–0294 prerequisite is Done. The original Markdown issue plan and
its immutable RFC base remain unchanged; the
[bounded current plan](../rfcs/plans/rfc-bpr-0019-closeout/milestone-plan.json)
adopts only BPR-0295, retaining all seven criteria and the independent gate.

### Existing successor authority

[RFC-BPR-0030](../rfcs/RFC-BPR-0030-dif-production-animation-revision.md)
explicitly partially supersedes RFC-0019's three clips, original resource
limits, source-size and regeneration requirements. The operator's
[BPR-0378 decision](../issues/60-done/BPR-0378-dif-canon-depends-on-the-local-zlib.md#operator-verdict--option-1--2026-08-19)
and [BPR-0408 implementation](../issues/60-done/BPR-0408-the-generated-tree-comparison-is-zlib-pinned-too.md)
compare canonical decoded PNG pixels and deterministic text, while validating
committed compressed-payload hashes against the actual committed files.
[RFC-BPR-0031](../rfcs/RFC-BPR-0031-dif-workbench-animation-integration.md)
owns current runtime composition. These are existing accepted successor facts;
this audit grants no new amendment and leaves original RFC-0019 wording intact.

### Exact original criterion reconciliation

The row numbers below identify the unchanged canonical criteria and matching
rows on BPR-0295. They are not seven new acceptance criteria.

| Original row | Current direct result and identity | Remaining limitation |
|---|---|---|
| 1 — Original source, rights and provenance | All 72 current source files match v008 intake canonical hashes; current manifest matches its reviewed candidate. Provenance binds the exact v008 human verdict hash. Asset hygiene checks 87 files, including 82 PNGs, without a forbidden marker. | BPR-0291 rights/visual receipt belongs to the old pack. The later owner accepted the exact d10 derivative scope; creator identity, provider/model/seed/version, full prompt/input-role history and service terms remain recorded as unknown. No missing fact is invented. |
| 2 — Reproducible outputs | Public validate and verify pass. Both clean regenerations agree; the current freshly generated products also match every committed byte on this Python 3.14.7/zlib 1.2.12 host. | Portable acceptance remains the existing BPR-0378/0408 canonical-pixel/text/payload-hash policy. This one byte-equal observation does not promise identical compression on every interpreter. |
| 3 — Exactly three clips | Current 63-case contract passes, including generated C++ descriptor consumers and hostile schema/identity/return/digest cases. | Actual schema 2 admits the ten clips authorized by RFC-0030. The literal original three-clip assertion is false for the successor pack and is not marked passing. |
| 4 — Original bounds | Current validator accepts registered 256-square RGBA cells, geometry, alpha/gutters, timing and revised bounds. Measured content: 71 cells, 18,612,224 bytes; 20 gutters; complete strips 23,855,104 bytes; maximum 10 fps. | The original 48-cell, six-gutter, 12/13.5-MiB limits are superseded. Current 17.75/22.75-MiB use satisfies RFC-0030's 19/24-MiB ceilings, not the old numeric assertion. |
| 5 — Mechanical and attended motion | Current mechanical validation passes. v008 session `7e489c0dd362c256bc28715a16892b65`, hand-back 123, has twelve accepts and no pending decision; the reviewed manifest and all source bytes match. | Historical BPR-0291's fifteen bound source/generated files, manifest, provenance and tool all differ now. Its unchanged receipt is historical proof only; the successor verdict supplies current scoped art/derivative acceptance. No new human verdict is inferred. |
| 6 — Build/tool isolation | Current offline-tool import/capability audit passes, and current source/resource selection scan checks 565 files. The 63-case suite includes graph-registration, art-tool-dependency, core asset, runtime-resource and missing-selection negatives. | Current Mac dev and tests-disabled application builds pass; core-only builds and passes 163/163 tests. The selected graph/binary verifier passes on those exact profiles. The original no-runtime clause is superseded by RFC-0031, whose ten strips are intentionally compiled into the application. |
| 7 — Honest documentation | The current documentation structure gate, all 64 documentation tests and whitespace check pass. This ledger separates old claims from the accepted current implementation. | Current architecture/asset docs truthfully describe runtime composition. Independent reviews confirmed the explicit successor reconciliation; the original no-runtime statement applies to RFC-0019's authored stage. Terminal status still depends on AC1. |

### Commands and artifact identities

Commands ran from the isolated worktree; product input is unchanged from the
entry revision. `python3` is Python 3.14.7. The public maintainer entry point is
`scripts/dif_assets.py`; its terminal outputs were `DIF_ASSETS_VALID` and
`DIF_ASSETS_REPRODUCIBLE`.

| Command | Result |
|---|---|
| `python3 scripts/dif_assets.py validate --assets src/adapters/qt/assets/dif` | Pass, exit 0. |
| `python3 scripts/dif_assets.py verify --assets src/adapters/qt/assets/dif` | Pass, exit 0; two temporary empty output trees removed by the tool. |
| `python3 tests/scripts/dif_asset_tests.py --cxx /usr/bin/clang++` | 63/63 pass, 92.602 s, no skip. Direct offline maintainer suite, not a CTest/native-application run. |
| Current artifact/hash/authority audit | Pass for v008 identity, hygiene, generator capability, current source/resource boundary and local byte equality. Exact script and observations are retained in the [audit record](../rfcs/plans/rfc-bpr-0019-closeout/asset-audit-r1.json). |
| `python3 scripts/verify_documentation.py --root .` | Initial unconfigured worktree invocation failed to import `board_model`. Setting `PYTHONPATH` to the assigned installed implement-rfc `scripts` directory passed with `DOCUMENTATION_OK`; no product change was needed. |
| `PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=<installed-implement-rfc>/scripts python3 tests/scripts/verify_documentation_tests.py` | 64/64 pass, 2.467 s; negative fixture diagnostics are expected. |
| `git diff --check` | Pass after the audit edits. |

The installed helper root is
`/Users/jaeger/.mask/store/v1/6ad19f0fc539b9b393a02dbeff6bf0745f83ac499d7c953cba4d9236bf667832/payload/implement-rfc`.
The new worktree has no ignored `.agents/skills/implement-rfc` deployment;
`PYTHONPATH` supplies those already installed read-only helper modules.

Current manifest SHA-256:
`c9f3f38e303caea3bb2c1b28edf947a717c2c1e5f484945a1d6814d8aee7f40d`.
Current provenance SHA-256:
`f77354d6e55fd8631b5741d5a055c1dcb42a52e3a5ebf515d98b6ad1fed76f1c`.
Current asset-tool SHA-256:
`c0361ed24130ff81a8a1b32dab4cca26123ce08e35205304765c3fd09de86abf`.
Current v008 verdict SHA-256:
`57a2a6d8fa6c297173b9f7a93e6411faa3b1cd0c0262ba059b1da36021f4e116`.
The audit record includes additional exact test, boundary and resource-selector
hashes. Both historical manifest/verdict pairs still match their durable
validation receipts; the removed historical validator was not rerun.

### Current graph replay and stop boundary — 2026-09-25

After this audit was integrated, the coordinator built the current Mac
developer and tests-disabled application graphs, built core-only and passed
all 163 core-only tests. The selected graph and binary verifier returned
`DIF_ASSET_BOUNDARIES_OK`; [graph-verification-r2.json](../rfcs/plans/rfc-bpr-0019-closeout/graph-verification-r2.json)
records the commands, binary digests and exact profile selections. An
independent ordinary review reproduced the asset audit and found the premature
`exit:1` pass, since corrected to pending. A separate critical review
confirmed the current v008 assets and successor authority.

The surviving blocker is AC1's requirement for complete creator and tool
provenance. Current `provenance.json` explicitly records unknown creator and
generator details. The v008 verdict authorized distribution with those gaps
disclosed, but did not amend AC1's completeness requirement. The owner must
supply the missing facts or a scoped amendment accepting those recorded
unknowns for AC1. No new visual or rights verdict is required for unchanged
v008 bytes. Keep BPR-0295 Blocked and RFC-0019 Deferred until that criterion
and terminal reconciliation are resolved. The original
[durable handoff](../rfcs/plans/rfc-bpr-0019-closeout/handoff-r1.json) remains
the audit history; the r2 graph record is the subsequent current evidence.

## Historical implementation evidence

Historical status: **the original Dif source pack and deterministic generated sprite
products are implemented; final source rights and all five attended visual
subjects are accepted for the exact reviewed assets.** BPR-0288 is done at `1a5c9b8`: its initial 18-case contract closed the
catalog-portability, neutral-body-box, and symlink-containment findings.
BPR-0290 extends that contract to 21 cases, including narrow compilation of
both synthetic and production catalogs plus stale manifest and digest-inventory
negatives, in developer and core-only selections. The tooling consumes no
supplied art and grants no reference or distribution authority. Historical or
delegated observations remain provenance until replayed at the final closeout
head. BPR-0294's behavior-preserving simplification is complete at `b5c183f`:
its exact clean archive reproduces every generated and attended-preview byte
and passes all 33 focused cases.

## Architecture allocation

| Region | Selected responsibility | No-impact or prohibition |
|---|---|---|
| Qt adapter assets | Original owned sources, manifest/provenance, generated strips and fixed catalog | No runtime widget, timer, preference, application composition, or JSON parser |
| Offline scripts | Bounded deterministic PNG/schema/generation/preview contracts | No network, model call, prompt replay, ordinary-build invocation, or private-path diagnostics |
| Tests | Reproducibility, hostile input, geometry, alpha, limits, compilation, build isolation | No visual or legal self-approval |
| Core/application/app | None | No Dif types, bytes, resource selection, timing, effects, or policy |

## Direct claim matrix

| Acceptance claim | Planned direct evidence | Current disposition |
|---|---|---|
| Owned original source pack and exact rights | BPR-0287 pre-creation decision, BPR-0289 source hashes, BPR-0291 final rights subject and scoped foreign-material audit | Both exact attended authority decisions validate; final production-source rights and the scoped foreign-material audit are accepted for the bound asset bytes |
| Byte-identical offline regeneration | BPR-0288 deterministic tool contract and BPR-0290 two-empty-tree/committed-tree comparison | Production strips, catalog, and digest inventory reproduce byte for byte from two isolated empty trees; BPR-0294 preserves generated tree digest `da65cd5231cf70c1964de3dc11226ad613506f94a452ebe4e31e207c40138ed6` and attended-preview tree digest `c29e92783869b4acce69cc5024fcc8603140b81b17747ef6f61cf446add0fa56` at `b5c183f`; final closeout replay remains required |
| Exact three-clip manifest and fixed descriptor | Schema validator, hostile graph cases, and narrow generated-catalog compile consumer | Production and synthetic catalogs compile in a C++20 consumer; stale manifest/catalog and digest inventory fail non-vacuously |
| Transparent bounded registered runtime cells | Full cell/gutter/edge/body/baseline/travel/rate/frame/pixel/file validation and negative corpus | All three committed strips validate with six transparent gutters, nine declared cells, shared registration, and bounded budgets |
| Mechanical motion plus attended art direction | Seam/handoff mechanics and BPR-0291 five-subject immutable offline visual package | Mechanical calm seam and greet handoff pass; all five exact owner-attended visual subjects are accepted, with the plus/minus-card note preserved for downstream composition |
| Developer/core-only/tests-disabled isolation | Clean selected-graph builds, dependency/resource scans, and cut registration/tool-dependency negatives | BPR-0292 passes at `d402338`: 182 dev and 83 core-only tests, tests-disabled app build, source/binary/resource scans, and 33 nonvacuous asset/boundary cases are green |
| Complete honest documentation | BPR-0293 RFC-base path ledger and documentation gate | BPR-0293 passes at `3259cc2`: the exact clean archive validates the Dif assets and repository documentation, every Markdown/agent file has a final newline, Git reports no whitespace error, and every RFC-owned path family has an owner or exact no-impact disposition without a runtime/package claim |

Final replay records use the exact normalized acceptance-criterion text as
their durable key and add the command, result, proving commit, artifact digest,
and delegated review disposition before any closeout checkbox changes.

## Documentation impact ledger

| Surface | Disposition | Evidence |
|---|---|---|
| Asset-local README and provenance | Documented | Source/generated authority, creation method, schema, clips, limits, commands, invalidation, accepted verdicts, and downstream prop split |
| UX appearance and accessibility | Documented | Identity, backgrounds, contexts, scales, nonsemantic motion, rest fallback, and no-runtime boundary |
| Build and test guidance | Documented | Offline commands, dev/core contracts, clean three-graph boundary verifier, and attended invalidation |
| Foundation security | Documented | Closed inventory, symlink containment, PNG/input bounds, prompt disposition, capability isolation, and attended limits |
| Packaging | Documented no-runtime-package change | `notice_required: false`; no resource binding, manifest, install, SBOM generation, signing, notarization, or release claim |
| System architecture | Documented | Qt-adapter source/product ownership plus explicit core/host/current-app no-impact and RFC-BPR-0020 handoff |
| Agent instructions | Updated | Stable edit/regenerate/validate/review obligations are in `src/adapters/qt/AGENTS.md` |

## RFC-base changed-path reconciliation

The immutable RFC base is
`58b5116f8706ac336664cc3fc39dabc66745a70d`. The rows below partition the
RFC-BPR-0019-owned changes from that base. Large review-media trees are grouped
by their closed directory roots; the strict package manifests and asset
inventory supply the per-file identities inside each root.

| Changed path or closed path family | Owning documentation or disposition |
|---|---|
| `docs/rfcs/RFC-BPR-0019-dif-visual-identity-sprite-assets.md`, `docs/rfcs/RFC-BPR-0020-animated-dif-mascot-qt-composition.md`, `docs/rfcs/plans/RFC-BPR-0019-issue-plan.md`, `docs/rfcs/README.md` | Governing asset RFC, explicit downstream composition boundary, issue plan, and canonical index. RFC-BPR-0020 remains deferred and gains the accepted prop-separation constraint; no runtime work is claimed. |
| `docs/issues/{backlog,in-progress,ready,done}/BPR-0286` through `BPR-0295`, `docs/issues/README.md`, `docs/issues/NEXT_ID` | Tracker history and live issue graph. Folder moves are status transitions rather than duplicate product surfaces. BPR-0295 retains the non-waived RFC-BPR-0006/RFC-BPR-0013 prerequisites. |
| `.agents/skills/bpr-attended-review/` changed skill, references, builder, and tests | Generic document-only attended-gate support used by BPR-0287. It changes review tooling, not product runtime. Its official suite and both Dif manifest-bound verdict validations are recorded. |
| `scripts/dif_assets.py`, `scripts/verify_dif_asset_boundaries.py`, `tests/scripts/dif_asset_tests.py`, and the scoped `tests/CMakeLists.txt` registration/selection changes | Owned by the asset README, build/test guide, security baseline, BPR-0290, and BPR-0292. The scripts are offline maintainer/test tools; no ordinary product graph invokes them. |
| All 18 files under `src/adapters/qt/assets/dif/` | The asset-local README defines source/generated authority, schema, commands, limits, invalidation, and the RFC-BPR-0020 boundary. `provenance.json` owns rights/tool/notice facts; the manifest and digest inventory own mechanical identity. No resource target selects the generated products in RFC-BPR-0019. |
| All 7 files under `docs/verification/reviews/dif-provenance-v001/` | Immutable pre-creation authority batch, returned verdict, and validation. It authorizes creation only and contains no final/runtime/package approval. |
| All 81 files under `docs/verification/reviews/dif-final-v001/`, including the 51-file offline package and 24-file preview tree | Immutable final-rights and five-subject visual evidence. Review media is not product art or runtime proof. The validated verdict accepts the exact subjects while keeping package, RFC closeout, RFC-BPR-0020, runtime composition, and runtime mascot claims false. |
| `docs/verification/RFC-BPR-0019.md` and the BPR-0287 through BPR-0295 issue evidence | Verification ownership, direct-claim rows, delegated findings, limitations, and closeout dependencies. Historical observations remain provenance until BPR-0295 replays them at its final head. |

The owning product documentation is now the asset README,
`docs/ux/appearance-and-accessibility.md`,
`docs/architecture/system-architecture.md`,
`docs/security/foundation-security.md`,
`docs/development/build-and-test.md`, `packaging/README.md`, and
`src/adapters/qt/AGENTS.md`. Core, application-host, provider, Git, SQLite,
agent, syntax, icon, installer, signing, and release documents have no
RFC-BPR-0019 implementation change beyond the explicit no-impact boundaries
above.

## Historical closeout blockers

- BPR-0287 is complete: the accepted identity-only page export validated
  against batch `bpr:/attended-review/betterpullreview/844543906f4f8df462676ab0`.
  That decision authorizes creation only; BPR-0291 retains final-source rights
  and visual acceptance authority.
- BPR-0291 is complete. The final rights and five visual decisions validate in
  immutable batch
  `bpr:/attended-review/betterpullreview/c93e59ae499277a8d2e854f6`; the verdict
  leaves every runtime, package, downstream-RFC, and closeout claim false.
- The accepted identity subject notes that the plus/minus card should not be
  part of the core base sprite. RFC-BPR-0020 must preserve that downstream
  composition constraint or reopen the affected digest-bound asset decision.
- BPR-0294 is complete at `b5c183f`; the simplification changes no generated,
  preview, attended-review, schema, diagnostic, or rejection identity.
- RFC-BPR-0006 is not Auto-Verified and its live closeout is BPR-0115.
- RFC-BPR-0013 is not Auto-Verified and has no live terminal closeout card at
  the planning head; individual resilience fixes do not substitute for one.
- RFC-BPR-0020 remains strictly downstream and cannot begin from generated
  assets whose RFC has not become Auto-Verified.
