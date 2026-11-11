---
schema: "implement-rfc.card.v1"
id: "BPR-2095"
title: "Bind persisted Linux Git artifacts to their native file lifetime"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P0"
metadata: {"milestone_plan":{"id":"bpr-2095-git-artifact-lifetime","path":"docs/rfcs/plans/bpr-2095-git-artifact-lifetime/milestone-plan.json"},"release_source":"48bd9b3f2b82128e6b43527812f165eacc4f8b47","owner_scope":"release56-git-lifetime-r1"}
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
