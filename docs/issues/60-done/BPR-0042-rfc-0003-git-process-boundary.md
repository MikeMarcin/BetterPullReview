---
schema: "implement-rfc.card.v1"
id: "BPR-0042"
title: "Implement hardened installed-Git process boundary"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0040"]
completed: "2026-07-30"
---

# Implement hardened installed-Git process boundary

Replace the Git adapter placeholder with a concrete narrow application port
and one designated `QProcess` implementation. Resolve and validate the
installed Git image and exec path, construct the child environment from
nothing, isolate system/global/local configuration and templates, use only
fixed verb/argument shapes against validated app-owned bare repositories, and
bound stdin, stdout, stderr, duration, cancellation, descendants, and
shutdown.

Keep credentials, URLs containing credentials, GitHub responses, proxy
authority, and network trust out of the Git child. This card ends at securely
consuming an already completed request-owned `.pack.part` spool through fixed
local Git operations. BPR-0043 owns the separately bounded
protocol-v2/native-HTTPS acquisition and cache ingestion. BPR-0044 owns the
durable pack identity, pre-import inventory, and preexisting-pack facts.
BPR-0045 owns journal-before-import and ref/catalog sequencing. Installed Git
never runs `fetch-pack`, parses a remote advertisement, or becomes another
production authentication transport, and none of these boundaries weaken the
RFC-BPR-0002 query-bearing redirect rejection.

Direct canaries must prove that user/system configuration, credential helpers,
askpass, hooks, filters, LFS, submodules, textconv, external diff, alternates,
replace objects, SSH/file/ext protocols, editors, pagers, traces, and
repository-provided executables are not invoked.

## Result

- Added a narrow application-facing Git process port with algorithm-tagged
  commit, tree, and pack identities; typed operation and private-ref requests;
  fixed failures; and installation metadata that exposes only the verified
  Git version.
- Added the designated `QProcess` adapter. It binds a validated absolute Git
  executable and matching exec path, probes the required Git 2.39-or-newer
  pack/object/ref capabilities, constructs the child environment from
  nothing, and admits only fixed local repository commands and argument
  shapes.
- Isolated home, configuration, attributes, templates, hooks, temporary
  storage, working directories, and repositories. Repository and policy drift
  fail closed before launch, including symlink/reparse and nonempty policy
  directory cases.
- Derived the exact
  `incoming/v1/<64-lowercase-hex-operation-id>.pack.part` path, retained a
  no-follow verified spool handle through import, and validated strict
  `index-pack` output plus the resulting pack and index before success.
- Serialized process work with bounded input, retained output, deadlines,
  cancellation, descendant termination, and two-stage reap. A catastrophic
  unreaped child permanently quarantines the lane and retains its process
  wrapper instead of entering an unbounded destructor wait.
- Kept protocol-v2 and native HTTPS transport/cache acquisition in BPR-0043,
  durable operation journaling and recovery facts in BPR-0044, and
  journal-before-import sequencing and lifecycle orchestration in BPR-0045.
  Target-native Windows execution remains BPR-0053.

## Evidence

- `cmake --preset dev` and `cmake --build --preset dev -j 4` pass.
- The focused `git_process_contract`, `architecture_boundaries`, and
  `architecture_boundary_contract` matrix passes 3/3.
- `python3 tests/scripts/architecture_boundary_tests.py` passes all 104
  mutation cases.
- Implementation and remediation are recorded in `0059fbc`, `6e2e7fc`,
  `a0a92cf`, and `251be4b`.
- Real installed-Git fixtures cover exact initialization, pack/index
  ingestion, object and ancestry inspection, atomic refs, hostile
  environment/configuration/repository canaries, spool replacement, bounded
  process terminals, and permanent quarantine with zero forbidden
  invocation.
- Three adversarial review rounds found and re-reviewed spool-handle,
  nonempty-policy, post-wait quarantine, and bounded-reap repairs; the final
  reviews report no remaining BPR-0042 blocker.
- The documentation validator and `git diff --check` pass. Native Windows
  process, Job Object, pipe, ACL, junction, and reparse execution is
  intentionally unverified here and retained in BPR-0053.
