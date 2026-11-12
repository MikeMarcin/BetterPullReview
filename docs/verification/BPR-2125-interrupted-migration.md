# BPR-2125: retained workspace migration safety

The version1-to-version2 ownership migration now keeps its journal deliberately
invalid until the complete replacement is durable. This corrects the retained
workspace deletion found while qualifying [[BPR-2118]].

## Bound candidates

- Unsafe release successor: `363b2d77d542fd0d7fc8e481d7bd147d52625c3d`.
- Corrective source: private `6b04b17b507868b90cd2badcf21d47b19e00dedc`;
  canonical `63b70be81d6476dee0bbd0b0d6934b1e67b691ae`.
- Final native test consumer: private `67c3e67e53c37b4f3afc8e1d89368195310eeee8`;
  canonical `1cb6469a9b0ce594910c4b2a9809d2a862fff0ab`.
- Both corrective commits were applied independently over canonical main
  `5ee3252b55c8d4c2bce476329b3cd207d4c798d9`. All five touched baseline files
  matched the unsafe source before application. Other main work and its counter
  were preserved; no whole-main merge into the release is implied.

## Original negative control

The original native engine created a private retained phase0,1,2,3,7 workspace
with a local edit. Its valid version1 form occupied2690 bytes; the complete
version2 replacement occupied3350; phase3 ended at2653. A2691-byte replacement
prefix, written through the same inode, extended past old EOF and left complete
version2 phases0,1,2,3 with an incomplete phase7. Normal recovery returned
`recovered=1 live=0` and deleted the disposable workspace and edit. The complete
version2 control returned `recovered=0 live=1` and preserved both.

This is a synthetic interrupted-write state observed through the real native
engine. No real crash, remount, owner workspace or external service was used.
Its original before-images, execution receipts and build provenance are retained
in the external release-run evidence, unchanged.

## Forward protocol and reachability

The existing exclusive descriptor/lock and all authority checks remain in place.
Migration performs these steps, stopping immediately on any failure:

1. Write an invalid `!` byte at offset0, then flush it.
2. Write only the replacement body from offset1.
3. Truncate to the full replacement length, then flush the body and size.
4. Restore the opening `{` byte at offset0, then flush the commit.

No body write or truncation begins unless invalidation and its flush succeeded.
No JSON publication begins unless the complete body, size and flush succeeded.
Before durable invalidation, a crash can leave the original retained version1
journal or an invalid marker. Afterwards every partial body/size state retains
the invalid first byte. After final-byte publication the full retained phase7
body was already flushed. A pending marker returns `record-cleanup-failed`
without trimming the file or acquiring cleanup authority. The inode is unchanged.

The old bare phase3 prefix cannot be distinguished from a legitimate disposable
journal. The fix does not claim to recognize it after the fact: it prevents the
corrected writer from producing it. Ordinary append-tail parsing, phase transitions,
private-object/volume/inode/token/scope/authority checks and cleanup rules are unchanged.

## Native fault evidence

The ordinary macOS Debug ownership target and its dependencies were built from
the private candidate with pinned Qt6.12.0 and offline dependencies. The normal
repository network-isolation runner used zero loopback ports.

Seventeen durable per-case receipts bind input/output hashes, journal inode,
retained-edit hash, and the reached I/O prefix:

| Boundary or injected failure | Next recovery |
|---|---|
| Before marker write; failed marker write | Complete legacy migration; retained Live |
| After marker write/flush, body write, truncate, body flush | Explicit refusal; record and edit unchanged |
| Failed marker flush, body write, truncate, body flush, commit write | Explicit refusal; no later I/O stage reached |
| After commit write/flush; failed commit flush | Complete version2 retained Live |
| Actual short body write at2691, followed by write failure | Invalid marker remains; explicit refusal; edit kept |
| Actual short body write followed by successful remaining write | Complete migration; retained Live |

All17 preserved the original journal inode and retained edit. The short-write
course again measured2690/3350/endPhase3=2653/cut2691. Separate valid version1
and version2 phase3 temporary controls were correctly cleaned, preserving the
original disposable-workspace behavior. Existing tamper/refusal courses passed.

The focused native integration run passed all four rows, each with child exit0:
`agent_workspace_ownership`, `git_process_contract`, `git_cache_contract`, and
`application_agent_invocation_host`. These include retained legacy reattachment
and propagation of the returned durable guard.

## Source controls and limits

On the private corrective candidate, documentation, architecture, conditional-branch
identifiers, clang-format22, focused clang-tidy22 (both changed translation units,
including all five engine compile variants), and the actual composition
source-inventory/adversarial guard pass. No
`src/app` input or its owning composition pin changes. The BPR-2095 generation
helper remains byte-identical. The new fault hook exists only behind the stock
ownership test target's macro and is absent from the normal native engine object.
Canonical documentation also passes. An additional whole-main app guard remains
failed for inherited foreign app content: expected
`2cbe1999453c9e59fefd70d27a3a7ead53b8cae8b9944faabfa80fe38dcda16d`,
found `695e2b6260132c668c3f4dc0d4462a1fad8b04ad4a1300fcdb8eb40f9b0be812`.
Neither owned source commit changes app bytes or that pin; no unrelated reseal
was made. This is separate from the passing frozen release-source guard.

The owned plan validates against locked MASK965. Global checkpoint discovery
still refuses unrelated BPR-2120 metadata fields; no runtime upgrade or
unrelated metadata correction was made.

This evidence is focused macOS source and functional verification. Native
Windows/Linux fault courses, all-nine release compilation, tests-off installed
binary absence, release/security/legal/signing/trust and public download checks
remain release qualification obligations. No cohort allocation, signing,
packaging or publication occurred here.
