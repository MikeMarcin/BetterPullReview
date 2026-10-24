---
schema: "implement-rfc.card.v1"
id: "BPR-1759"
title: "Verify process-wide mimalloc on native macOS"
status: "Blocked"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
blocked_by: "native-macos-candidate"
---

# Verify process-wide mimalloc on native macOS

## Scope and acceptance

Implement and observe the macOS override in the selected desktop, CLI,
update helper and other helpers (AC03). Probe actual C, C++, Qt/loaded-library allocation and
cross-module release on Apple Silicon and Intel. Check the signed installed
candidate, startup and shutdown. Include allocator-boundary stress needed by
AC06; a linked binary or stats report alone is insufficient.

## Candidate prerequisite amendment, 2026-10-24

The coordinator authorized moving only the package prerequisites needed for
AC03 into this milestone. This card therefore owns the macOS runtime
producer/deployment declaration, retained MIT notice and source-inventory entry
needed to prepare a tests-disabled installed candidate through `tools/release.sh`.
The source basis is `packaging/Install.cmake`'s fixed private-library producer
map, `packaging/package_runtime.py`'s closed runtime/notice inventory and the
release script's dependency, signing and relocated-launch checks. Without those
entries the installed allocator cannot load or pass the package audit.
Final AC07 for every platform, standalone CLI/source closure, negative package
qualification and release acceptance remain on [[BPR-1762]]. No criterion or
milestone exit is waived; signed installed and native Intel evidence stay
required. `--prepare-only` is an ad-hoc candidate, never signed-install acceptance.

## Native evidence and current block

Product source candidate `aadf2d91d` on macOS 26.6.2 arm64, AppleClang 21 and
Qt 6.12.0: all five actual tests-enabled executables pass C/C++, loaded-module,
Qt, cross-module, cross-thread, allocation-failure and one-runtime ownership
checks. The provider/missing-override controls also pass (7/7, 15.90 seconds).
The prior 17-row integration passes; unchanged product behavior reuses that
record. Package/runtime/source tests pass (30 with one native-Linux skip, 9,
and 24 respectively). See [the ledger](../../verification/RFC-BPR-0093.md).

`tools/release.sh --version 0.3.2 --prepare-only --skip-tests --output-dir
build/mimalloc-m1/local-candidate` refused at the clean-source guard before a
build, deployment, signing, Keychain lookup or Apple submission. Unrelated
`docs/issues/NEXT_ID` and reservation
`docs/issues/_reservations/BPR-1765--BPR-1765--65cc7e762a78.json` appeared
concurrently; neither belongs to this milestone and both are preserved.
No installed candidate was produced. After that writer reconciles its files,
retry the same local preparation from clean committed source. The resulting
ad-hoc candidate still cannot satisfy signed-installed AC03. Developer ID and
native Intel evidence require their actual authorized course; this arm64 host
supplies neither. Do not mark this card or the milestone complete.

## Clean-worktree result

Source `eb28c0187a49143f5470e4400c2f1b3d0cb03de0` in branch `codex/mimalloc-macos-candidate` at
`/Users/jaeger/.codex/worktrees/mimalloc-macos-candidate/BetterPullReview` produced the credential-free local candidate successfully.
The dirty-source blocker is cleared in this worktree; main's allocation remains
untouched. Full package audit passes (52 files, 19 binaries, 519 first-party
units) using scoped LLVM 22; source-sidecar binding and public missing/extra
allocator controls pass. The relocated offscreen assessment passed and left the
delivered bundle unchanged. See the ledger's clean-worktree continuation for
artifact names, hashes, exact commands, initial tool/harness failures and limits.

This is a tests-disabled, universal, ad-hoc-sealed local bundle. It does not
supply Developer ID, notarization, Gatekeeper, delivered native Cocoa or Intel
execution evidence. This card remains Blocked on those actual native/signed
observations; no criterion is waived. The branch/worktree and candidate are
retained, all own commands are quiescent, and no review server is running.
