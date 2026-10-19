# Adopted amendment: external determinism as the general core rule

Owner mandate, 2026-10-18:

> Let's clean up the architecture docs so external determinism is the general
> rule, rather than accumulating special exceptions for each library.

Mandate `external-determinism-r1` authorizes this documentation reconciliation
through [[BPR-1620]] and its [additive milestone plan](../plans/external-determinism-v1/milestone-plan.json).
It adopts the [general semantic contract](../../architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md#21-external-determinism).

## Sources and affected requirements

Baseline checkout: `80f280ee8c9946b98c6527a79c65c878d0a6a4d1`. SHA-256 identities before amendment:

- `docs/rfcs/RFC-BPR-0001-product-contract-native-architecture.md`: `94cb1627eb69c6fbd4e3def7f43cf9fa60d2057645d2e25e94c4fae017696f84`.
- `docs/rfcs/RFC-BPR-0077-find-and-find-in-files.md`: `81bfd171a461daf744f2d20310a033abf3533663e27e57169558a9c02aa29ab4`.
- `docs/rfcs/RFC-BPR-0080-hashing-replacement.md`: `083b73a34a81020a89644f945312079ab848cdd729208c01da8be53a045733f9`.
- `docs/rfcs/RFC-BPR-0082-diff-engine-evaluation.md`: `50fac8b5967e647edbfade88b45d9471bb5138285e76581124f3af0cd06cdba5`.

RFC-BPR-0001's functional-core allocation and the SHA-256/RE2/xdiff placement
rationale in RFC-BPR-0080, RFC-BPR-0077 and RFC-BPR-0082 are reconciled.
Library-specific exceptions no longer define determinism. The current
[implementation inventory](../../architecture/system-architecture.md#core-implementation-inventory)
separately preserves dependency approval, pins, licensing and exact private
include/link/use ownership. The original owner answers and historical plans,
cards, audits and verification evidence retain their meaning and bytes.

## Reconciliation and preserved obligations

External determinism applies to first-party and library-backed computation.
Internal dispatch, caches, allocation, synchronization, contention clocks and
hash seeds are compatible when they do not alter the promised semantics.
Effects, explicit observations and host scheduling/lifecycle/cancellation
ownership remain. Qt independence is a separate dependency and reuse choice.
The semantic contract explicitly qualifies resource failure, cancellation and
cross-platform identity; it does not promise identical ambient failures.

No dependency, backend, algorithm, effect capability or worker is added. Keep
dtl's two bounded uses, SHA-1 collision detection, xdiff's separate replaceable
dynamic library and licensing obligations, RE2's no-worker policy and upstream
fatal-diagnostic limitation, and the platform SHA-256 wrappers/failure contract.
Source/build guards retain their enforcement behavior; their inventory is not a
semantic-purity definition or proof of upstream effect absence.

The new card's three criteria cover policy consistency, preserved constraints
and focused verification. Original RFC acceptance, base revisions, native
Windows/Linux evaluation, package/vault/crash/vulnerability and release obligations
are unchanged; no broader RFC closeout follows. No active plan or protected
attended candidate is changed. Unrelated BPR-1378 work in this shared checkout
retains its writer and is excluded from this slice.

Authorized execution publishes the live rule, links current guidance to it,
checks documentation and existing boundary contracts, then finishes BPR-1620.
No unresolved design choice or required human/native acceptance is introduced.
