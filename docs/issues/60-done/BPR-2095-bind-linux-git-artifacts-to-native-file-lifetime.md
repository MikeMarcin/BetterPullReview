---
schema: "implement-rfc.card.v1"
id: "BPR-2095"
title: "Bind persisted Linux Git artifacts to their native file lifetime"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P0"
metadata: {"milestone_plan":{"id":"bpr-2095-git-artifact-lifetime","path":"docs/rfcs/plans/bpr-2095-git-artifact-lifetime/milestone-plan.json"},"release_source":"48bd9b3f2b82128e6b43527812f165eacc4f8b47","owner_scope":"release56-git-lifetime-r1"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"320d33e0996778eb0e4793688026382d201c7799","evidence":{"kind":"artifact","detail":"Native strong/legacy/stale/unavailable/foreign controls; original replacement-deletion negative retained."}},{"criterion":"AC2","outcome":"pass","revision":"320d33e0996778eb0e4793688026382d201c7799","evidence":{"kind":"artifact","detail":"FD lifetime through mutation/publication; raw manifest unchanged; explicit legacy recovery and unavailable-filesystem limits documented."}},{"criterion":"AC3","outcome":"pass","revision":"320d33e0996778eb0e4793688026382d201c7799","evidence":{"kind":"artifact","detail":"Original Release/Debug native contract once each pass; root final critical review accepted exact raw/image/input identities."}},{"criterion":"AC4","outcome":"pass","revision":"320d33e0996778eb0e4793688026382d201c7799","evidence":{"kind":"artifact","detail":"Scoped format22/tidy22/architecture/docs/board/diff checks pass; frozen56 immutable and native lease returned; intentional owned main integration authorized."}}]
---

# BPR-2095: Bind persisted Linux Git artifacts to their native file lifetime

## Problem

The original Linux Debug contract and its single causal diagnostic show that a fresh empty file recycled the committed device/inode and was deleted by abort recovery. This is a product ownership defect. An owned-file primitive probe on the same filesystem confirms GETVERSION is available, stable across writes/rename/separate-process reopen, and changes on all 32 observed same-inode recreations. Those bounded observations are not a universal non-collision guarantee.

## Acceptance

- AC1: Linux opaque artifact facts use native generation plus device/inode; stale, legacy or unavailable proof refuses existing artifacts and preserves every foreign replacement. Already absent recovery remains idempotent.
- AC2: A matched descriptor pins the lifetime through publication and destructive cleanup. Live manifest/lock identities, port width/API, existing size/privacy/quarantine guards and other platform schemes remain unchanged. Document the existing explicit Clear caches recovery path and its retained-history loss.
- AC3: Keep all original ownership assertions and add witnessed-empty/partial/completed close-reopen, stale-generation, legacy/unavailable and actual syscall/lifetime controls. Corrected native Release and Debug owning courses and independent critical review pass before landing.
- AC4: Scoped format22/tidy/source/docs/board checks pass; own commits land main, frozen56 and unrelated work remain unchanged; native qualification remains separate.

## Evidence and frontier

Original/diagnostic failure and capability raw evidence are external under implementer56-linux-history-triage-r1. Capability receipt SHA256 96664d908b648f68ec0dc06fbc2fb88d522b796d79a575f584584956f0d7e795. Implementation, focused native positives and critical review are pending.


## Completed engineering proof

Candidate `320d33e0996778eb0e4793688026382d201c7799` changes only the installed Git adapter, its module-private lifetime helper and the existing Git cache contract test. Linux opaque scheme 3 stores native generation with device/inode. Live cache manifests, lock identities, 32-byte port width and other platform schemes retain their prior formats.

The original Debug failure and single causal diagnostic remain failed evidence: the old dev/inode witness deleted a recycled-inode replacement. The native capability probe and corrected courses are separate evidence, with no retrospective release clearance.

- AC1/AC2: Existing partial/completed and new witnessed-empty recovery pass after closing/reopening the cache. A changed generation with the same actual device/inode and legacy scheme 1 both refuse and preserve the file; absent legacy recovery remains idempotent. The real nonblocking opener and port preserve a FIFO substitution. On actual existing tmpfs, GETVERSION is unavailable, new sink admission produces zero witnesses and cleans its held empty creation, and a matching-device/inode claimed-zero-generation canary is preserved with `repair_required`. No production injection, new mount or dependency was added.
- AC3: Original normal Release and Debug `git_cache_contract`, each at its unchanged 600-second bound, pass once: 4.0471 and 4.3020 seconds. Four private normal-flag compiles and two links pass. Only the two changed objects replace their original objects; all normal libraries remain exact. All 1,253 protected inputs and frozen source remain unchanged, private image hashes match independent after-course checks, and all owned process groups are empty. Root independently accepted the final critical review after verifying the exact raw archive, both JUnit cases and input/image identities.
- AC4: clang-format 22, two-TU LLVM 22 tidy using the normal macOS compile database, architecture boundary, documentation, board and diff checks pass. The Linux-specific branches additionally compile with original GNU 15.2 Release/Debug warnings-as-errors. No sanitizer run or cross-native release qualification is claimed by this focused proof. The common checkpoint helper still refuses the historical parent plan's unknown `amendments` field; the explicit authored issue plan check passes. No shared tooling or parent plan was changed.

### Legacy and unsupported-storage recovery

An existing Linux scheme-1 artifact lacks lifetime evidence and remains untouched with `repair_required`; it is never upgraded from whichever file occupies that name. The existing supported action is **Settings > Storage & privacy > Troubleshooting > Clear caches**, or **Clear caches and try again** at startup, under the stopped-profile reset flow. Clearing retained Git history can lose earlier force-pushed revisions that GitHub no longer serves; it does not migrate an old witness. The existing operator guide already records that warning and action. Generation-unavailable storage still refuses new durable Git artifacts after clearing: this change does not claim support for such a filesystem, manufacture generation zero, or make a cache reset enable an unavailable OS capability.

External evidence: `implementer56-linux-history-triage-r1/git-lifetime-native-proof-r1/`. Raw receipt SHA256 `0fd8061946c3b06ca5f02294a5de676097a0173181376ff5a089f8c591851a00`; archive SHA256 `f00714ff7ea9e1f49441f23d56b94ba660fb0eabc9b088971f8081b7791004c8`; independent review `root-critical-review-final-r2.json`. Published/frozen build 56 remains unchanged and unqualified by these corrections; a production change requires the coordinator's successor qualification.
