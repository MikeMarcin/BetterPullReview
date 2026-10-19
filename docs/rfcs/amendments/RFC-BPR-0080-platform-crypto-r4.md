# Adopted amendment: deterministic platform SHA-256 in core

Owner instructions, 2026-10-18, r4:

> Let's amend BPR's architecture to allow deterministic platform crypto in core,
> then use CommonCrypto on Mac and CNG on Windows with HACL as the portable fallback.

Expanded instruction, r5:

> I'm still fine keeping RE2 out of core directly if abseil is launching its own
> threads for regex. Chrono and random bans are dumb. Although I also don't think
> stdlib random generators are that great if we need a lot of random generation.

The coordinator supplied these instructions to the exclusive engineering leaf.
This record adopts them under authorized implement-rfc execution; it is not an
unanswered proposal. Original RFC_BASE_SHA stays unchanged.

## Sources before amendment

- Live RFC: `docs/rfcs/RFC-BPR-0080-hashing-replacement.md`, r3.
- Adopted plan: `docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json`.
- Source checkout revision: `296b17d9ddfbc395005a1ee19599dc4a3cb679e1`.
- `docs/rfcs/RFC-BPR-0080-hashing-replacement.md` SHA-256 `bff0ecb322c81f99cc9b268e58f6810a936b39b4733aa0cb1cbc3ac75369d57d`.
- `docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json` SHA-256 `77f22af6fd44f82bc73921eee8cdab55efaa159aa59fd9cb750c7e95cefdc3a7`.

## Reconciliation

The original RFC's rejection of OS CPU dispatch as inherently impure is
superseded. Semantic determinism means byte-identical SHA-256 for identical
input; system dispatch and implementation state may vary. Core admits only
this scoped unkeyed SHA-256 capability, not general OS APIs. CommonCrypto is
part of macOS libSystem; CNG is Windows' system bcrypt API. HACL retains its
existing approved pinned MIT source and portable role. No new vendored library,
licence or algorithm enters the product. SHA-1 collision detection is unchanged.

The owner directly selects the backends; the old host-port per-site throughput
adoption rule does not gate this new core adoption. Keep existing measurements
and all original station correctness/performance, host window-thread and package
obligations. Native Windows evidence for this amendment is narrower than the
full BPR-1448 evaluation. Linux libcrypto and Q3 remain independent and pending.

RFC behavior/recommendation, platform-fit rationale and acceptance AC06 are
updated. The original complete plan and historical accepted cards retain their
bytes and meaning. The additive [milestone plan](../plans/deterministic-platform-sha256-v1/milestone-plan.json)
and [[BPR-1617]] bind the new architecture/backend outcome; no old obligation is
removed or treated as observed. No active or protected attended candidate is
changed; no other writer owns these paths.

The SHA-256 signature, noexcept behavior, digest identity and independent copies
remain. CommonCrypto/HACL stay allocation-free. CNG may allocate on stream
construction/copy; updates do not allocate first-party memory and moves transfer
ownership. Initialization refusal selects HACL. Failure after native state is
established terminates, because the API cannot report failure and the original
bytes are not retained for replay. It must never emit a silently wrong digest.

The expanded utility rule permits chrono arithmetic and explicitly seeded,
caller-owned PRNG state; it excludes clock reads, random_device and global C
random state. Reproducible values across standard libraries need specified
algorithm/mapping. No generator choice, generator implementation or throughput
claim follows. RE2's adapter placement is unchanged.

Evidence and remaining obligations live in the [RFC ledger](../../verification/RFC-BPR-0080.md)
and BPR-1617's checkpoint.
