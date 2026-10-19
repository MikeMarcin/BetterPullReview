---
schema: "implement-rfc.card.v1"
id: "BPR-1626"
title: "Supply the approved Linux verification key in v0.3.0 notes"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"04eaf0e1271d4fccdb14a07ba20bef56788f6713","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1626: Supply the approved Linux verification key in v0.3.0 notes

The coordinator selected release-note delivery of the existing approved Linux public verification key through the same private release channel. Native manifest guards intentionally reject arbitrary extra assets; this slice supplies public verification material in notes without changing those guards. The coordinator verified the approved public export and delegated this exact note update. No private key, credential, signing or publication authority is part of this card.

## Acceptance

- AC1: Include the complete approved 404-byte ASCII-armored public key with fingerprint ED85BAA84A3B6D83FCD5B1F107CEC96F2688E4A8 and SHA256 c271732f02b5f3c709ad1a61d7f7816200e96226c490fd77d88f6583437f9798 in the committed release notes, plus concise save/import/detached-signature verification instructions.
- AC2: Verify the notes code block is byte-identical to the approved public export, retain private same-channel distribution and qualification limits, pass documentation/diff checks and commit explicit paths without changing asset or manifest guards.

The [release engineering ledger](../../verification/release-v030-engineering.md) and ignored `.mask/sessions/release-v0.3.0/repair-handoff.json` preserve candidate evidence and coordinator-owned remaining work.
