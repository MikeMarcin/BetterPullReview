# BPR-0256 v004 operator response and attended result

Date: 2026-08-21

Packet SHA-256: `08cc6f277cb92c89760c94f263f8c4813e6e1eca1e5bd55482d03b991bddd4e2`

## Exact operator decision

The product owner returned this exact approval:

> I approve the exact one-use approval subject
> sha256:7daa46aa18dfc0cd804c9060f1b790089e17a1db972d074b38e61a78769dcb39
> for BetterPullReview binary
> sha256:2a1c65bf1249ba25333f949deac674ded204cd9ce0653d7fdbb165fcbb4a2216
> and signed-capsule supervisor
> sha256:6ff0d7c7949dc446e8d4e63d04d2eda762d1d6155aab84fdbb6beaae97f35a6f
> and capsule inventory
> sha256:ca4fbe45e2a6ed6198a90b1b114b4617c1008bc8739c3662c2f4e7f1665a25ee
> and source manifest
> sha256:c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79
> to publish one source-line review comment to MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

The response matches the packet's approval alternative exactly and binds the
reviewed schema-3 subject and all four executable/source identities.

## Materialization and static manifest

The approval was materialized as the exact 1,496-byte subject followed by only
its required `subject_sha256` line. The resulting file was 1,576 bytes with
SHA-256 `fa50a72b50059a8d4e3e876bbaa12bce05ebdc58a6419e8cc5b73a9363fdcced`,
mode `0600`, one hard link, and an owner-private `0700` immediate parent. Its
absolute path resolved without a symlink component.

The first static configure reused a stale August 11 CMake cache and stopped
before generating a manifest because that cache mixed Homebrew QtGui/QtNetwork
with the pinned QtCore while rebuilding KSyntaxHighlighting. No test ran, the
approval remained present, and the replay namespace still contained zero spent
records. A targeted `cmake --fresh` configure of the same build directory and
the same reviewed arguments selected the pinned Qt consistently and passed.

The resulting static manifest contained 195 tests and exactly one live entry:
`live_review_mutation_designated_target`. Its only label was
`bpr_live_mutation`, its timeout was 1,800 seconds, and its command was the
exact reviewed supervisor followed only by `--attended-launch`, `--inventory`,
the reviewed inventory, `--approval`, and the private approval path. Its
credential environment entries were unsets; there was no verdict override.

## Attended execution result

At 08:24 PDT the exact CTest lane stopped after 1.84 seconds with a failed test
and no supervisor output. No BetterPullReview GUI or credential prompt became
available, no credential was entered, no Publish action was enabled, and no
GitHub mutation was submitted. The approval file remained present and the
owner-private replay namespace still contained only its namespace identity:
the approval was not consumed and no spent record was created.

The supervisor intentionally emits no stage-specific text for its early
refusals, so the CTest result alone does not prove which internal check failed.
A separate redirect-free, credential-free diagnostic request to the same
GitHub REST pull-request endpoint immediately afterward returned HTTP 404
(`curl` exit 56). That directly proves the designated target was not publicly
observable at diagnostic time and is consistent with the supervisor stopping
at its mandatory unauthenticated public-observation phase. It does not justify
claiming a more specific internal branch than the supervisor reported.

The packet requires an abort on startup or public-observation failure and
forbids a retry. The unconsumed approval file and its private directory were
therefore deleted. v004 must not be run again. A future live attempt requires a
publicly observable designated target plus a complete successor packet,
capability-critical review, and new exact operator decision.

## Verdict disposition

The exact operator approval is accepted and frozen. The live publication
criterion is **not satisfied**: the authority was never consumed, no attended
GUI course occurred, no credential was acquired, and no review comment or UI
observation exists. BPR-0256 remains parked on a live-GitHub prerequisite rather
than on an owner verdict.
