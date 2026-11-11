# Profile reset binding and the Windows source gate

Mandate `release58-conditional-identifier-r1`, frozen source
`70324127b116e45988b8ffa3a77370209ba7aef4`, correction
`cee8fe23fd5c8d7e71efa1e178b4096c837709eb`.

## Observed failure and cause

Windows58 Release's original `conditional_branch_identifiers` row exited 1 in
5.87 seconds, within its 60-second limit. Its sandbox reported the true child
exit; this was neither a crash nor a timeout. The three findings were the
POSIX-only helper's structured binding `before` at lines 126, 127 and 131.
The map parameter is `aBefore`. The lexical gate recognizes ordinary local
declarations and lambda captures, but not this range-for structured binding,
so it misidentified the local as a stale parameter spelling. Windows polices
the inactive POSIX branch; macOS compiles that branch instead.

Rename only the binding and its two uses to `storeIdentity`. Reversing these
three identifier tokens restores the complete original file bytes. No scanner,
condition, assertion, reset gesture, retained descriptor, library, configuration,
provider, timeout or product source changes.

| Source | SHA-256 |
| --- | --- |
| Original profile reset TU | `d1f2595b30ae522f2b6cf01720ab54a3d6f573b251d029f69cf901264cdad173` |
| Corrected profile reset TU | `0bce35b96123c1e200ee4e5bc97fc417b31508576a273b8f2ac90f7537f6daa1` |

## Focused verification

- Exact original file with the gate's Windows macro set reproduces precisely
  the three findings; the corrected file has none.
- The unchanged gate scans all 2,175 source files successfully under its
  Windows, Darwin and Linux host models. Policed lines/files remain
  16,820/184, 13,037/172 and 16,515/193 respectively. These are modeled host
  checks on macOS, not native Windows or Linux qualification.
- All 26 existing conditional-branch contract tests pass. A separate planted
  `expectedBytes` use beside `aExpectedBytes` is still reported in an inactive
  branch. The detector was not changed or weakened.
- Maintained clang-format 22.1.8 dry-run and `git diff --check` pass.
- The documentation structure gate reports `DOCUMENTATION_OK`; all 67 existing
  documentation contract tests pass, including their expected negative fixtures.

External evidence is retained at
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer58-conditional-identifier-fix-r1`:
`source-seal.json`, `model-controls.json`, the three model logs, original and
candidate copies, and the hash-bound original configured row/failure/receipt.
The original failure block SHA-256 is
`3cca83b73023930b2decac37eab546d007ee1eba6abc8d165c63672f21011d90`.

## Release evidence boundary

The frozen58 producers and running suites remain untouched. No corrected
native course, build or qualification verdict is claimed here. The existing
[execution procedure](../../.codex/skills/publish-release/references/execution.md#compile-every-selected-platform-before-long-suites)
allows an explicit policy-permitted bridge to completed unchanged evidence;
its recovery rule requires matching relevant inputs. The
[qualification profile](../../.codex/skills/publish-release/references/optimized-qualification.md#debug-residual-and-promotion-boundary)
requires both configurations for affected test/fixture cases plus meaningful
negative controls.

This change warrants a bounded bridge review, not an assumed full-matrix pass:
only a test-local name changes, the scanner and production trees are identical,
and the original gate failure plus planted stale-name controls are retained.
For a successor source, bind fresh configured gate/contract rows and affected
profile-reset evidence in both configurations, then account explicitly for
unchanged remaining evidence. New source/cohort metadata and corresponding
source assets must match their delivered artifacts. The release coordinator
owns that disposition; this card supplies no blanket reuse, security, native
trust or publication approval.
