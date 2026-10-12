---
schema: "implement-rfc.card.v1"
id: "BPR-1402"
title: "Reuse the release signing vault and repair native Authenticode verification"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win", "linux"]
metadata: {"mandate_revision":"release-next-20261011-vault-verifier-r3","milestone_plan":{"id":"release-next-vault-verifier","path":"docs/rfcs/plans/release-next-vault-verifier/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"277c58f26d906d3e69f185632dcf155c7e8f2180","evidence":{"kind":"command","path":".mask/sessions/release-next-20261011/vault-candidate-checks.json","detail":"Native synthetic encrypted-key/Windows credential course passes cold-agent, Unicode encoding, fixed stdin transport, missing/wrong credential/passphrase/metadata, nonprivate home, cache clearing, WSL interop/public-only verify/tamper refusal and exact cleanup. Package composition proves failed unlock before output/build and scoped pinentry-error for all signatures. Coordinator's gpg-vault-real-receipt.json separately confirms owner enrollment and two real cold-agent checks with pinentry disabled; key remains protected."}},{"criterion":"AC2","outcome":"pass","revision":"277c58f26d906d3e69f185632dcf155c7e8f2180","evidence":{"kind":"command","path":".mask/sessions/release-next-20261011/vault-candidate-checks.json","detail":"Actual native Windows pollution control executes poison autoload; maintained verifier explicitly loads the child's built-in module and refuses unsigned fixture without poison execution. Same unchanged signed probe e1d9353dfcc085226813f63b6c595ad2a3bf817359bc83bea239f12439f0753a passes SignTool plus native Authenticode publisher/timestamp checks without parent environment workaround or additional Azure operation."}},{"criterion":"AC3","outcome":"pass","revision":"277c58f26d906d3e69f185632dcf155c7e8f2180","evidence":{"kind":"observation","detail":"Windows and WSL native-release/vault suites each pass 27 checked of 28, with complementary single platform skips; all cases observed on their matching platform. Documentation structure/67 documentation contracts/diff check pass. Current guides bind approved nonsecret configs, automatic vault unlock, public-key distribution, actual chosen Linux key/dedicated Windows signer and native-context config visibility. Product slice intentionally committed; durable handoff preserves native/legal/security/privacy/remote release gaps. No version change/release/build/Azure signing/GitHub mutation. Synthetic credentials/roots/agents removed; task commands complete."}}]
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
prerequisites, not release acceptance. Preserve BPR-1400, original M8/M9
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

The coordinator subsequently completed real enrollment through the owner's
direct visible-console entry. Its two fresh scoped-agent courses passed
maintained WSL-to-Windows automatic unlock, pinentry-disabled signing,
public-only verification and tampered-content rejection. Public receipt:
`.mask/sessions/release-next-20261011/gpg-vault-real-receipt.json`.
The first enrollment attempts failed before secret entry because the public
config was not visible in the native terminal process; the coordinator placed
the same public binding there and retained that failure. No agent received
the passphrase. Native producer-context file visibility must actually be checked.

## Verification and self-review

Product candidate `277c58f26` is bound by the full revision and unchanged HEAD
in `.mask/sessions/release-next-20261011/vault-candidate-checks.json`.
Windows native-release/vault contracts pass 27/28, with only the local Bash
publisher fixture skipped; WSL checks the complementary Bash case and skips
only the actual Windows PowerShell pollution row. Both 28-case runs pass all
applicable checks. The pollution fixture first proves normal autoload executes
its poison module, then proves the maintained verifier does not. No new Azure
operation was used: the final signed probe SHA-256 stays
`e1d9353dfcc085226813f63b6c595ad2a3bf817359bc83bea239f12439f0753a`.
Its SignTool and native Authenticode publisher/chain/timestamp checks pass.

The opt-in native course passes with a disposable encrypted key and synthetic
exact-name credential: Unicode encoding, nonprivate-home refusal, missing/wrong
credential/passphrase/username rejection, failed-unlock cache clearing, cold
agent/pinentry-error unlock, Linux interop, public-only verification and tamper
rejection. Exact dummy credential absence and private root/agent cleanup are
confirmed. Coordinator evidence separately supplies two real cold-agent checks
and completed human enrollment. No private key file or passphrase was read by
agent tools. Documentation structure, all 67 documentation contracts and
`git diff --check` pass.

Self-review confirms one bounded credential target, fixed GPG commands and
stdin-only secret transport. Explicit scoped home and pinentry error prevent
interactive fallback during automatic packaging; unlock failure occurs before
output/build. The standard console enrollment performs no vault enumeration,
and no general command can print a secret or choose a secret-consuming command.
Mutable copies are wiped without promising complete Python memory erasure.
Default/manual non-vault signing behavior and optional RPM mechanics retain their
existing path. Current guides record native-context file visibility, local Windows
account/interop requirements, separate Azure login/MFA and approved public key
distribution. No Qt/core/product UX code changed.

This accepts only reusable producer preparation and the native verifier repair.
Remote station invocation, final packages/native desktops/product vaults,
independent trust/legal/security/privacy and publication remain unqualified.
No version change, BPR release build/signing, GitHub mutation or push occurred.
Task-owned synthetic roots/credentials/agents are removed; coordinator-owned real
credential/keyring and completed wizard receipts remain. No task command runs
past handoff and no review server is handed over.

## Mac/Windows reconciliation

Renumbered from Windows BPR-1388 to BPR-1402 during authorized integration on 2026-10-12 because the Mac had independently allocated BPR-1388. Historical acceptance SHAs and original reservation provenance are retained. The new integration reservation is `docs/issues/_reservations/BPR-1400--BPR-1402--f418763b96dd.json`.
