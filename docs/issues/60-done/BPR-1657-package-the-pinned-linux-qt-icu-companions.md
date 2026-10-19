---
schema: "implement-rfc.card.v1"
id: "BPR-1657"
title: "Package the pinned Linux Qt ICU companions"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"assigned_implementer": "/root/ubuntu_final_production", "mandate": "release-v030-coordinated-r1 + owner-signing-restored-20261019; packaging-only repair/no full suites", "baseline": "591aabf60cdbd45d9bf50a714a263a2954a39c2e", "reservation": "docs/issues/_reservations/BPR-1657--BPR-1657--52807f7aeaa1.json", "milestone_plan": {"id": "release-v030-repairs", "path": "docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}, "boundary": "Linux packaging/source/notice closure only; no application execution, trust-gate bypass or premature production"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"591aabf60cdbd45d9bf50a714a263a2954a39c2e + BPR1657 working-tree candidate SHA256 eb1ff38a2bbff9735ed977a35302368ce63611513e27f934f85630c6d08c89a0","evidence":{"kind":"observation","detail":"Exact final working-tree helper d3fa2528509fea3e632a1fe978c324c3f6a46277f5afea69f91257fcaed04a3d native r4 PASS: source-built trio;9 refusal controls;31 imports;private loader/collation;3 existing hardening gates;strict cache/source/recipe/compiler/flags/output and prefix containment. Evidence icu-maintained-course-r4-output.json SHA256 1c44d5ad1d9ba0a7b0bd9e3acf73ea55a91d3c18280ac230bee0b8f7c1f34823"}},{"criterion":"AC2","outcome":"pass","revision":"591aabf60cdbd45d9bf50a714a263a2954a39c2e + BPR1657 working-tree candidate SHA256 eb1ff38a2bbff9735ed977a35302368ce63611513e27f934f85630c6d08c89a0","evidence":{"kind":"observation","detail":"Official source/archive/binary/complete mixed notice pins;actual29 entries (21 archives/8 helpers),all entry digests verified. Source assessment SHA256 4f2f01310271ae8ec25a282935e81f83cc13ab04fc99f91b2479c9b199b63b24 is dirty-source engineering evidence,not final release. Mac/Windows components/notices unchanged."}},{"criterion":"AC3","outcome":"pass","revision":"591aabf60cdbd45d9bf50a714a263a2954a39c2e + BPR1657 working-tree candidate SHA256 eb1ff38a2bbff9735ed977a35302368ce63611513e27f934f85630c6d08c89a0","evidence":{"kind":"observation","detail":"Focused package29/source21/package-audit7/security18/native-manifest30 PASS with2 pre-existing native-only skips;package29 rechecked final helper;native r4 positive/negative controls PASS;1963 unchanged execution inputs. No full application suite rerun."}},{"criterion":"AC4","outcome":"pass","revision":"591aabf60cdbd45d9bf50a714a263a2954a39c2e + BPR1657 working-tree candidate SHA256 eb1ff38a2bbff9735ed977a35302368ce63611513e27f934f85630c6d08c89a0","evidence":{"kind":"observation","detail":"Final docs gate DOCUMENTATION_OK;67 documentation controls PASS;self-review/diffcheck PASS;explicit-path finish commits this completed bounded slice. BPR1657-candidate.json binds exact precommit paths. Final signed three-platform reproduction,independent Linux/material intake and publication are subsequent exact-candidate obligations,not this engineering acceptance."}}]
---

# BPR-1657: Package the pinned Linux Qt ICU companions

The actual final591 Ubuntu DEB attempt compiled the app, then refused its
pinned Qt6.11.2 provider's ICU73.2 runtime dependencies as untrusted. The
approved provider includes the three required libraries; QtBase's SPDX names
ICU73.2 but supplies no binary checksums, source URL or licence conclusion.
The failed attempt and original gate remain retained. No final signed Linux
manifest was emitted.

The coordinator authorizes this packaging repair under the coordinated release
mandate. The owner's subsequent amendment explicitly forbids full test-suite
reruns: use focused packaging contracts, native closure checks and unchanged
application-input proof. All historical591 assets and normal-suite evidence
remain bound to their actual revision.

## Acceptance

- AC1: Admit only the exact Linux Qt6.11.2 ICU73.2 data, uc and i18n provider ABI inputs, with official archive/binary pins, canonical origins and exact names. Ship the exact ICU73.2 source-built hardened trio, with source/recipe/compiler/make/flags/source-tree/output cache binding and whole-file relocation checks. Preserve the existing full RELRO/NOW audit and refusal of every undeclared runtime.
- AC2: Include the exact ICU73.2 source archive, complete mixed upstream notices, source-built provenance, standalone recipient helper/README and declared package identity in Linux source/provenance/SPDX. Bind the actual21 archives/eight helpers (29 source-ZIP entries) to official source/pinned inputs. macOS/Windows selected source-component and notice sets remain unchanged.
- AC3: Focused package/source/security contracts and native real-provider positive/negative controls pass, including wrong origin, bytes and name, missing companion and altered deployed bytes. Record Qt's actual ICU import surface and preserve upstream qualification unknowns. No full-suite rerun or product behavior change.
- AC4: Complete self-review and documentation checks, commit explicit paths, bind changed and unchanged inputs and return the common-source candidate to the coordinator. Actual signed final-artifact reproduction, independent security/material acceptance and publication remain subsequent exact-candidate obligations.

## Current claim

Claimed by the sole Ubuntu implementer on `codex/ubuntu-icu-release` from591;
root retains all producer/source-freeze reconciliation and publication intake.
No producer source or historical artifact has been changed for this repair.

## Bounded source-build design

The exact official Qt ICU prebuilt inputs have RELRO and a nonexecutable stack,
but lack NOW. An isolated source73.2 assessment with the existing GCC15/make
passes configure5.95s/build41.47s/install1.00s and the unchanged ELF hardening
gate for all three delivery candidates. No tests or application run in that
build. The maintained helper records source-built bytes honestly and validates
its cache before reuse; the shared Qt cache remains unchanged.

The owner explicitly prohibits full test-suite reruns for this packaging-only
repair. No `src/`, CMake, app resource, fixture, benchmark or execution policy
changes are included. Final package reproduction waits the coordinator's common
source adoption. Original RSS failure/exception and all broader qualification
limits remain unchanged.

## Focused engineering result

The maintained native source producer, all nine refusal controls, exact31-symbol
QtCore import/export coverage, actual private-loader/collation and the unchanged
three-image ELF hardening gate pass. Its cache is checked against the pinned
archive tree, helper, compiler/make/flags and complete output bytes. No shared
Qt file was changed. The explicit29-entry source assessment verifies every
archive/helper digest; its dirty-source status is retained, with final signed
artifact acceptance pending common-source production.

Focused Python contracts cover29 package,21 source,7 package-audit,18 security
and30 native-manifest cases; two pre-existing native-only skips remain. The
documentation gate and67 documentation controls pass. Input proof preserves1963
execution/build/fixture files and Mac/Windows component/notice sets from591.
The sole new source-built runtime is same-version Linux ICU; no full normal
suite is run and the narrow loader/collation course is not whole-app or broad
native qualification. Independent final Linux security/material acceptance,
exact native artifacts and publication remain coordinator obligations.
