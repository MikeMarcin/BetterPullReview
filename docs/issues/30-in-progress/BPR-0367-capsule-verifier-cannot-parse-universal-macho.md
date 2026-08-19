---
id: BPR-0367
title: The capsule verifier cannot parse universal Mach-O framing
status: In Progress
rfc: RFC-BPR-0008
depends_on: []
---

# The capsule verifier cannot parse universal Mach-O framing

Found immediately behind [[BPR-0366]] in BPR-0256's regeneration. The
currently acquired Qt ships universal `x86_64`+`arm64` binaries, so `otool`
prints one stanza per architecture. `MACH_MAGICS` in
`scripts/designated_live_macos_capsule.py` deliberately includes the fat
magics, but all three `otool` parsers assume thin framing:

- `mach_install_id` demands exactly two lines and fails closed on a fat
  file's four — `Mach-O install-name framing is invalid`, the observed
  regeneration failure.
- `mach_rpaths` and `mach_dependencies` are worse: on a fat file they parse
  without error and silently merge both architectures' entries, so a
  disagreement between slices would be invisible.

## Required final state

- The three parsers are architecture-aware: output is split per architecture
  stanza, every slice must agree on the install name, rpath list, and
  dependency list, and the agreed value is returned. Disagreement between
  slices, or unrecognized framing, fails closed.
- Parsing is separated from `otool` invocation so the framing logic is
  hermetically testable, and the capsule test suite covers thin framing, fat
  agreement, fat disagreement, and hostile framing for each parser.
- The real proof is the one that failed: the capsule `build` subcommand
  completes against the freshly built bundle with the universal deployed Qt,
  which also discharges [[BPR-0366]]'s final bullet.

## Verification

```sh
ctest --preset dev -R '^(designated_live_macos_capsule_contract|macos_supervisor_provenance|macos_capsule_identity)$' --output-on-failure
cmake --build build/bpr0256-release --target designated_live_macos_capsule
```
