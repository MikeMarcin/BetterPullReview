# BPR-1199 diagnostic candidate evidence

Assignment `bpr1199-comparison`, mandate r1, milestone `private-comparison`.
Product source: `c982586fbfc774614c6f4993c4e6c81d450d15be`.
This completes the unattended diagnostic preparation, not the private comparison
repair. Original AC1–AC4 remain pending.

## Established observation

The existing owner log has a successful pull request load at window session 43,
request 3. Temporally adjacent epoch records reach `protocol_acquire`, then
refuse with domain `transport`, reason `unavailable`, epoch request 1 and window
session 0. A subsequent `revision_prepare` record reports domain `epoch`, reason
`unavailable`. The prior catalogue/cache/pack-sink stages succeeded. These
records fall within the original attended candidate's preparation/decision
interval. No explicit session join identifies that epoch as the accepted PR
load, and no old field identifies the refusing acquisition branch. Therefore
this is bounded evidence of correlation, not proof of the underlying cause.

Only allowlisted fixed codes and numeric identities were extracted locally.
No raw private log, source, path, title, author, body or credential was copied
into this checkout. There was no live private API/Git access or remote mutation.
A closed PR's mutation admission is independent of its read comparison, so the
closed state does not establish this failure's cause.

## Implemented behavior

`233167b9` captures the originating application session at all three epoch
host diagnostic producer sites. Records before epoch request allocation still
use request 0. `c982586f` preserves a closed acquisition-step enum from the CLI
bridge through protocol/host logging to the real sink. Unsupported tool
qualification, identity probing, clone and later local processing can now be
distinguished without retaining command output. Invalid enum values are
rejected by the sink. Existing failure enums, admission, tool qualification,
arguments, clone bounds, authority, recovery and public wording are unchanged.
`clone` plus `unavailable` still does not establish authentication or size as
the cause. Diagnostic preview/export retains its existing identity/prose policy.

Changed product paths are the application Git/diagnostic ports, epoch host,
GitHub CLI Git transport and Quill sink. Tests cover the application host,
local fake-CLI bridge, real logger and composed application logging. The only
other changed source documentation explains these fixed diagnostic fields.
No functional-core or Qt product source changed.

## Verification on the candidate

Runtime evidence is outside Git at
`/Users/jaeger/.codex/tmp/bpr1199-jeozogwl`.
All builds used `CMAKE_BUILD_PARALLEL_LEVEL=2` and `-j2`; Qt tests ran serially
with `QT_QPA_PLATFORM=offscreen`.

- `red.log`: the two new correlation assertions failed against the unchanged
  host: missing originating session in host events and zero session in the
  actual composed log. `correlation-green.log`: both passed after the repair.
- `focused.log`: `architecture_boundaries`, `application_epoch_host`,
  `gh_cli_git_smart_transport`, `application_logging`, and `quill_logging`
  passed, 5/5. The Git bridge uses an isolated synthetic local repository and
  fake CLI; it does not contact a private service.
- `negative-step.log`: temporarily dropping transport-step propagation makes
  the bridge test fail at the unsupported qualification assertion. The source
  was restored in `finally`. `build-restored.log` and `restored.log`: final
  bridge, composed logger and app relinked; bridge/logging passed 2/2.
- `format.log`, `format-adapter.log`: explicit changed C++ paths passed format.
  `tidy.log`: seven changed translation units passed; existing warnings in
  unchanged helper lines remain (string-view/naming/static access).
- `docs.log`: `verify_documentation.py --root .` passed. `docs-tests.log`:
  all 64 documentation tests passed. Fixture tests deliberately print expected
  validation errors. `git diff --check` passed.
- Owner capture wrapper `--self-test`: synthetic private-content canary is
  excluded; fixed code and numeric identity retained; unknown code excluded.
  The actual owner wrapper was not run.

The configure used the dev preset, `BPR_DEPENDENCY_OFFLINE=ON`, and the existing
pinned `/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache`.
The focused build targets were `test_better_pull_review_application_epoch_host_tests`,
`test_application_logging`, `test_better_pull_review_gh_cli_git_smart_transport_tests`,
`test_quill_logging`, and `app_better_pull_review`.
Documentation uses `/opt/homebrew/bin/python3` with `PYTHONPATH` set to the
installed MASK implement-rfc `scripts` directory, because this worktree has no
ignored skill link. No new core-only, release or native acceptance is claimed.

## Exact owner candidate

Frozen app:
`/Users/jaeger/.mask/attended-review/bpr1199-comparison-v001-runtime/better_pull_review.app`.
The copied executable matches the restored final build, SHA-256
`4c4adf38b82511e9d2b77e7fc85f6f8fd3a4a327e15341ca65e23aecdd23bf58`.
It is Debug with tests enabled and uses pinned local Qt dependencies; it is not
the prior tests-disabled release. Runtime `inventory.json` records copied file
hashes, SHA-256 `7a091d0a5781f35a77361fa130ce9aca0d51a10980fb5939328a48749606232c`.
The owner launcher SHA-256 is
`41b67668726bdefcdf90737dacbd320e2fadce170884a9d100a7cb334f602455`.

The owner alone runs the absolute launcher command in the review page. It
refuses if a BetterPullReview process is running or the executable differs,
then launches the frozen app. After the owner quits, it reads only the new
bounded part of the default log, refuses rotation/replacement, and saves only
allowlisted fields to runtime `observations.json`. It never copies raw logs.
One AC4 Accept/Reject question asks whether the selected file shows base and
head. No separate acceptance is inferred from the wrapper's exit status.
