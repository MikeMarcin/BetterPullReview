# Proposed amendments: strict CLI credentials and ordinary publication

This accompanies [strict account credentials](RFC-DRAFT-0016-strict-cli-account-credentials.md)
and [removing the extra publication warning](RFC-DRAFT-0017-remove-cli-publication-warning.md).
It is a proposal, not an amendment published into the live contracts.
The owner permits transient token handling without BPR persistence and
rejects a maintained gh fork. Their latest instruction authorizes only
investigation and RFCs; no implementation, issue allocation, live token
retrieval, publication or installed-app change is performed here.

## Sources inspected

Repository inspection used HEAD `aa3d71fe470627e1e78488eb51a9114a788f33bd`.
These content identities bind the live requirements inspected during
authoring, not an implementation baseline. Reconcile drift before adoption.

| Source | SHA-256 |
|---|---|
| [RFC-BPR-0015](../../RFC-BPR-0015-github-cli-production-enablement.md) | `467720a0771e4ff986769aae3e365da7c5e8728bcc42e7836029bcbb74cf2644` |
| [BPR-1373](../../../issues/40-parked/BPR-1373-pin-each-gh-call-to-a-chosen-account.md) | `f6fad66c7b7f35e6038dc75b7085dbbd65dd89f513b7da81c62c1a1e6bbeba03` |
| [Adopted CLI-profile plan](../../plans/rfc-bpr-0015-cli-profile/milestone-plan.json) | `eeb2c94607c72f91e10617dab0b01f8d03e5660854283b614a39c151593af831` |
| [GitHub adapter instructions](../../../../src/adapters/github/AGENTS.md) | `84e6c5caf146a158a808fd0173781dcfb7ff13d8f858bbac67158caaca98f6fb` |
| [Product contract](../../../architecture/product-contract.md) | `10fe0f7d1e10e5e9147f8174cd5092812d4cea9af4d4548c715c855e3767fef6` |
| [System architecture](../../../architecture/system-architecture.md) | `147c741f2f06ff6e7af67f436bf671858f147559d3d4639cfe8abda21eba3982` |
| [GitHub host implementation](../../../architecture/github-host-implementation.md) | `73422601d64d09e52354b39593ebd6fe6f0de1ec137682988a7a4101b6f7a051` |
| [Corporate security overview](../../../security/corporate-security-overview.md) | `a830d25473c52f74adfd49aade4b2a3905669a8733dd381ad386dc2de97a73ac` |

## Requirements to reconcile on later adoption

| Existing requirement or decision | Proposed amendment |
|---|---|
| RFC-BPR-0015, 2026-09-13 amendment and 2026-10-09 token rule: no extraction/passing of CLI tokens, no `auth token`, no token environment variables | Permit only the bounded exact-host/login retrieval and operation-scoped child token supply described in the strict-credential draft. Keep the prohibition on BPR persistence, logging, arguments, global account mutation, ambient credentials and native fallback. |
| RFC-BPR-0015, per-account configuration amendment: missing named credentials can fall back inside gh; viewer observations detect this | Retain private token-free configuration, require an explicit account for credentialed use and prevent the unnamed lookup from supplying the request credential. Validate the acquired credential before publication using the same bytes. |
| RFC-BPR-0015, read trust and separate per-write confirmation; original unnumbered acceptance criteria for account opt-in, fixture publication and qualification | Retain deliberate connection trust and ordinary exact-operation publication gestures/grants. Remove the extra CLI publication warning only on qualified strict paths, and place accurate remaining trust details in the connection disclosure. Add the two drafts' fresh acceptance scope. |
| GitHub adapter instructions and architecture credential/CLI boundaries | Add a narrow sensitive-result and transient-child-environment exception at the designated adapter boundary. Keep core/application/Qt records token-free. Revise architecture enforcement and its tests for this exact exception rather than permitting generic token handling. |
| Product, GitHub-host and corporate-security descriptions of tokenless execution and account-switch races | Describe the actual transient credential lifetime, no BPR-created durable copy, strict acquisition/refusal and remaining memory/descendant/transport limits. Distinguish global account switching from credential deletion or revocation after acquisition. |
| RFC-BPR-0071 remembered-connection contract | Preserve token-free metadata and exact numeric-account revalidation. Restart reacquires only the saved chosen account's credential; missing strict capability/credential retains the choice but grants no session or write. |

## Work, dependencies and pending evidence

The warning-removal draft depends on strict credential behavior and its
supported-tool/native evidence. Both use existing selected-account metadata,
process supervision, mutation admission and reconciliation. They do not
expand Windows/Linux CLI eligibility or establish universal native, vault,
release or process-containment qualification.

BPR-1373 is Parked for an Accounts & hosts owner look. Its pending judgement
remains owed; neither draft supplies it or marks the card Done. Its
tokenless canary expectation would conflict with the proposed transient
environment. Preserve that historical evidence with its original candidate,
and bind new sensitive-buffer and no-persistence observations to the new
scope. Reconcile affected writers and any protected attended candidates
before changing their inputs in a later implementation session.

The adopted CLI-profile plan's `opt-in-cli-read` tokenless exit,
`guarded-cli-publishing` per-write-warning exit and
`native-qualification-and-closeout` warning evidence describe their old
scope. Do not overwrite those completed exits or replay adoption over the
historical plan. A later authorized implementation registers and binds the
new scope, publishes the narrow contract amendments, implements strict
credentials first, and removes the warning only after that dependency is
satisfied. Run focused public-path, sensitive-data, native and documentation
checks appropriate to those new claims.

The proposed mechanism is selected for these drafts: stock gh retrieves its
own named Keychain credential; BPR keeps it only for the operation. Direct
native Keychain access remains a later alternative requiring separate
qualification. No additional product choice is currently required to finish
these drafts. Command support, child inheritance, process-buffer handling
and exact native behavior remain implementation evidence obligations.
