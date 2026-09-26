# BPR-0971 capability-error classification investigation

## Candidate and result

Assignment `classifier-20260925`, baseline `daaa2023`, isolated branch
`codex/bpr0971-classifier`. Test candidate `161b988b` extends the
[initial audit](bpr0971-diagnostics.md) with identifier-shaped content canaries.
Product source is unchanged. The classification repair remains unimplemented;
BPR-0971 AC2 still fails. This is an exact contract conflict and executable
counterexample, not a privacy qualification or a completed milestone.

Eight synthetic cases pass through the real sanitizer, provider facade, and
private Quill file. All eight content markers survive each of those boundaries;
none survives the metadata-only export. The active credential is removed.
Existing exact unknown-error usefulness tests remain passing. No live provider,
account, private source or credential was used.

## What the existing contract requires

- [BPR-0602](../issues/60-done/BPR-0602-basic-quill-logging-foundation.md)
  requires unknown GraphQL code/message prose to survive as useful bounded
  detail, and source/review bodies to stay out of the queue and file.
- [RFC0027](../rfcs/RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md)
  authorizes the bounded static-query error fields, excluding source/review
  content. Its full design also requires omission of unclassifiable spans and
  forbids treating remote strings as operational facts merely because they
  look like identifiers. Generic fallback alone does not satisfy usefulness.
- Existing sanitizer, provider, native HTTPS and file tests require unfamiliar
  `newField` and `UNFAMILIAR_SCHEMA_CODE` / `NEW_SCHEMA_CODE` bytes to survive.
  This round preserves those tests and strengthens the provider test to check
  both the field and code.
- [BPR-0971](../issues/60-done/BPR-0971-audit-pathological-diagnostic-privacy-and-backpressure.md)
  AC2 asks for no private source or uncontrolled provider text at diagnostic
  storage and exports, including synthetic content canaries.

## Why a shape filter is insufficient

`sanitize_capability_error` receives response bytes and the active credential.
The JSON parser establishes syntax and selected field identity. It supplies no
provenance proving that a field's text is public schema or safe error content.
Neither `newField` nor `UNFAMILIAR_SCHEMA_CODE` comes from the nine locally
compiled capability queries. The static request's public vocabulary can supply
trusted symbols; it cannot establish arbitrary newly returned symbols as public.

The extended canaries substitute a short identifier into the existing useful
schema-error sentence, and uppercase identifiers into all three admitted type/
code locations. A rule accepting unfamiliar schema names or uppercase error
codes accepts these shapes too. A private source identifier or short secret has
no distinguishing syntax. Adding the words `private` or `canary` to a denylist
would only memorize this fixture. An in-band provider assertion that a value is
public supplies no independent provenance.

This demonstrates a conflict with **exact arbitrary unknown-value retention**.
It does not prove that useful safe diagnostics are impossible. A closed grammar
could produce local explanation text, preserve independently known public query
symbols, and explicitly omit unclassified operands and codes. That would change
the current exact unknown-value expectations. Conversely, retaining arbitrary
bounded unknown strings preserves today's usefulness but cannot establish AC2
from their shape or field name alone.

### Decision needed before changing the classifier

Confirm whether useful unknown-error acceptance may omit previously unknown
code/symbol bytes while retaining safe cause descriptions, request-bound public
symbols, request/status correlation and an explicit content-policy omission
reason. If so, authorize the corresponding changes to the exact historical
fixture expectations and define representative useful unknown explanations
before implementation. Do not mark the current raw-string behavior safe or
silently weaken either privacy or usefulness.

This is a bounded contract interpretation for the coordinator/owner. The rest
of BPR-0971 is not blocked by it: direct terminal-output bounds and failure
isolation remain independently implementable. No completed card or RFC
acceptance has been rewritten by this investigation.

## Executable evidence

Shared synthetic cases live in
`tests/support/capability_error_content_canaries.hpp`. Each has a distinct marker
so a file observation cannot attribute one retained field to another.

| Input field / shape | Sanitizer retained | Provider retained | File retained | Export retained |
|---|---:|---:|---:|---:|
| message / prose | 1 | 1 | 1 | 0 |
| type / prose | 1 | 1 | 1 | 0 |
| extensions.type / prose | 1 | 1 | 1 | 0 |
| extensions.code / prose | 1 | 1 | 1 | 0 |
| message / quoted schema symbol | 1 | 1 | 1 | 0 |
| type / identifier | 1 | 1 | 1 | 0 |
| extensions.type / identifier | 1 | 1 | 1 | 0 |
| extensions.code / identifier | 1 | 1 | 1 | 0 |

The sanitizer and file probes report `credential_retained=0`. The provider
checks credential absence and its failed capability terminal in each case.
Every file event is admitted, the logger closes, and the export descriptor
contains all eight records. The audit exits 1 when a canary survives. It is
opt-in, not registered as an expected-passing CTest or an expected-failure test.

The ordinary test path still requires exact useful unfamiliar names and codes.
The sanitizer/provider audit modes first run that useful-error control as well.
The ordinary real-file test supplies the corresponding file usefulness control.
The tests neither assert that content retention is acceptable nor erase the
finding when the normal regression suite passes.

## Commands and outcomes

Working directory for the following commands:
`/Users/jaeger/.codex/worktrees/bpr0971-classifier/BetterPullReview`.
macOS arm64; the same M4 Pro / macOS 26.6.2 / Qt 6.11.2 / AppleClang 21 station
as the initial audit. Dependency source cache reuse does not reuse build outputs.

```sh
cmake --preset dev -DCPM_SOURCE_CACHE=/Users/jaeger/.codex/worktrees/bpr0971-diagnostics/BetterPullReview/build/.cpm-cache
cmake --build --preset dev --target test_better_pull_review_github_capability_probe_tests test_quill_logging test_better_pull_review_github_provider_tests -j 8
ctest --preset dev --output-on-failure -R '^(github_capability_probe|github_provider|quill_logging)$'
build/dev/tests/github_contract/better_pull_review_github_capability_probe_tests --audit-content-policy
build/dev/tests/github_contract/better_pull_review_github_provider_tests --audit-content-policy
build/dev/tests/logging/quill_logging --audit-content-policy
python3 _coding-standard/scripts/format_check.py --root . tests/support/capability_error_content_canaries.hpp tests/github_contract/github_capability_probe_tests.cpp tests/github_contract/github_provider_tests.cpp tests/logging/quill_logging_tests.cpp
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths tests/github_contract/github_capability_probe_tests.cpp tests/github_contract/github_provider_tests.cpp tests/logging/quill_logging_tests.cpp
```

Configure/build pass. Baseline focused CTest: 3/3, 16.98 s. Candidate focused
CTest: 3/3, 15.61 s. All three content audits exit 1 with the table above. Scoped
format and diff hygiene pass.

The documentation gate passes using the installed MASK scripts in `PYTHONPATH`
(the worktree-local ignored skill deployment is absent); scoped board validation
passes with 1,182 cards. No documentation verifier implementation changed.

Strict tidy passes provider/logging units but the capability-test unit has ten
inherited findings: two `pragma once` headers, three Glaze option member names,
and five `push_back` calls. The unchanged baseline unit, analyzed in the prior
audit worktree, reports the same ten findings. Its SHA-256 equals the baseline
`daaa2023` source: `2600009a3a5dae765857ee276e6fe5452a07c2da4f249706253aaf916dc0f883`.
No suppressions or unrelated helper edits were introduced; the aggregate strict
tidy gate remains failed and is not reported as a pass.

Ignored `build/bpr0971-classifier-*.log` files contain command output; committed
prose preserves the outcomes. Synthetic actual diagnostic files exist only in
owned automatic temporary directories and are removed by the course. Product
source, public error policy and authority remain unchanged. Native TLS was not
rerun because it was not changed; Windows/Linux verification is owner-deferred.
Broader non-Quill/native crash limitations from the initial audit remain open.
