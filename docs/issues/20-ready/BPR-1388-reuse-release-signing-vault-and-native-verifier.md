---
schema: "implement-rfc.card.v1"
id: "BPR-1388"
title: "Reuse the release signing vault and repair native Authenticode verification"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win", "linux"]
metadata: {"mandate_revision":"release-next-20261011-vault-verifier-r3","milestone_plan":{"id":"release-next-vault-verifier","path":"docs/rfcs/plans/release-next-vault-verifier/milestone-plan.json"}}
---

# Reuse the release signing vault and native verifier

## Mandate and boundary

The owner requests preparation for the next coordinated Mac/Windows/Ubuntu
26.04 x64 release, with no version selection/change or actual release now.
The 2026-10-11 amendment is: "I dont want to have to enter this GPG passphrase
every time i ask to publish a release, we need to store that in a keychain or
something." This bounded milestone implements one Windows Credential Manager
generic credential and a scoped WSL GPG-agent unlock. The coordinator owns the
owner's direct passphrase entry and actual vault enrollment. No secret enters
agent messages, source, arguments, environment, temporary files or logs.

The prior approved Linux key and isolated Azure signer are producer
prerequisites, not release acceptance. Preserve BPR-1386, original M8/M9
evidence and all RFC0038 native/vault/privacy/legal/security obligations. No
additional Azure signing, access changes, GitHub mutation, tag, push, version
change, real release build or publication is authorized.

## Acceptance

- AC1: A maintained enrollment/unlock helper binds exactly one generic Windows credential to an approved full OpenPGP fingerprint and keygrip in an owner-private WSL GPG home. It validates the destination before reading the credential, sends the secret only to the fixed preset command's standard input and fails closed for missing/mismatched credentials, wrong key/home and invalid inputs. A reusable nonsecret producer configuration integrates unlock before Linux package signing without removing private-key encryption.
- AC2: The native Authenticode verifier explicitly loads the Windows PowerShell built-in security module despite an inherited PowerShell 7 module path. A polluted-module-path regression and read-only verification of the existing signed probe pass; no signing is repeated.
- AC3: Current release documentation and publish-release guides describe producer config/vault reuse, the chosen Linux key and observed Windows signer prerequisites truthfully. Focused security-sensitive contracts and documentation gates pass, the bounded slice is intentionally committed and a durable handoff records exact candidates, pending coordinator enrollment/native obligations and quiescent command state.

## Execution checkpoint

Engineering owner: `/root/release_vault_and_verifier`, nondelegating leaf on
`main`. Entry/baseline: `2b56a8c30f3fbde66a3469dd0778121401ad9e0a`.
Reservation: `docs/issues/_reservations/BPR-1388--BPR-1388--95215331fa8e.json`.
Mandate revision: `release-next-20261011-vault-verifier-r3`.
Durable continuation: `.mask/sessions/release-next-20261011/vault-handoff.json`.
Enrollment is a coordinator-owned human secret entry, separate from helper
engineering acceptance. Wider RFC0038 acceptance remains pending.
