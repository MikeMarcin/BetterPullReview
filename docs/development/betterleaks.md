# Local secret scanning with BetterLeaks

This is opt-in developer tooling. No setup, scan or integration course is
registered with GitHub Actions, CMake, CTest, the normal Python tests or
`scripts/run_test_suite.py`. Existing workflows are unchanged; this adds no
hosted CI usage. The commands also refuse `GITHUB_ACTIONS` environments.

## Setup

From this checkout, with Git and Python 3.10 or newer on `PATH`:

```sh
python3 scripts/betterleaks.py setup
python3 scripts/betterleaks.py hook-install
python3 scripts/betterleaks.py hook-status
```

Only `setup` can download anything. It installs
[BetterLeaks v1.9.0](https://github.com/betterleaks/betterleaks/releases/tag/v1.9.0),
the latest stable release selected on 2026-10-19; it does not select a moving
`latest` URL. The release's
[published checksums](https://github.com/betterleaks/betterleaks/releases/download/v1.9.0/checksums.txt)
were checked against the GitHub asset digests. The committed
[release manifest](../../scripts/betterleaks/release.json) pins both archive and
executable SHA-256 values for macOS, Linux and Windows, arm64 and x64.

Setup downloads the selected archive over HTTPS, verifies its SHA-256, reads
only the executable member, verifies that member's SHA-256 and installs under
the ignored `.cache/betterleaks/1.9.0/<platform>/` in this checkout. It does not
install packages, run an installer, edit `PATH` or change global Git settings.
Re-running setup verifies an existing installation without downloading. A
conflicting directory or changed executable is refused, never overwritten.
Each scan checks the executable's pinned digest and actual `version` output.
No fallback to another `betterleaks` or `gitleaks` on `PATH` is allowed.

## Commands and coverage

```sh
python3 scripts/betterleaks.py staged
python3 scripts/betterleaks.py tracked
python3 scripts/betterleaks.py history
```

| Command | Input |
|---|---|
| `staged` | Complete index blobs of added, modified and type-changed paths; renames are scanned as additions. Deleted paths contain no new content. Unmerged entries fail. |
| `tracked` | Current on-disk contents of paths in the index, including unstaged changes. It reports absent/deleted/sparse paths. Untracked files are not scanned: add them first. |
| `history` | Locally available commit diffs across **all refs**, including local branches, tags, remote-tracking refs and detached HEAD. Root commits and merge-parent diffs are included. |

The pre-commit hook runs `staged`. It honors `GIT_INDEX_FILE`, including the
temporary prospective index that a real `git commit -a` supplies. It reads
blobs with `git cat-file`; it does not scan the working copy, run clean/smudge
filters, check out files or change the index. Partially staged secrets block
the commit even when the working copy has been cleaned. An unstaged secret
does not block a commit of clean staged content; `tracked` finds it, and
`commit -a` includes it in the hook's input.

Exit statuses: **0** means the selected scan completed without findings,
**1** means findings, **2** means an unavailable/incompatible dependency,
configuration conflict or incomplete scan. Interrupted scans return 130.
Errors are never converted into a clean result. Findings retain file, line,
rule and history commit; values are reported as `REDACTED`. Commit messages,
authors, source lines, captures and match context are omitted. Child error
text is withheld because errors can contain credentials.

Scans and hooks contain no dependency installation, repository fetch or
credential validation path. Scanner subprocesses get a minimal environment
without scanner overrides or credential variables, an isolated home and
explicit `--validation=false`. Git plumbing disallows every transport and
lazy fetching; missing objects fail locally. History uses a private temporary
bare repository borrowing local objects and copying local refs, with no
remote, user configuration, hooks or replacement refs.

## Policy, suppression and findings

The only scanner configuration is the tracked
[local policy](../../scripts/betterleaks/config.toml). It extends the pinned
release's detectors and content heuristics but overrides its broad default
path prefilter. Source, tests, documentation, lockfiles and text in otherwise
excluded filename extensions remain eligible. This is not a directory baseline.

The integration was checked against the actual v1.9.0 CLI and source,
including its `dir`, `git`, `version`, `--report-format=template`,
`--ignore-gitleaks-allow` and `--validation=false` behavior. In particular,
v1.9.0 loads a source-root ignore file **even with an explicit empty
`--gitleaks-ignore-path`**. The wrapper therefore scans isolated snapshots or
the temporary bare history view. Snapshot configuration/ignore files are
ordinary content below `input/files/`, never scanner-root configuration.
Ambient `BETTERLEAKS_*`/`GITLEAKS_*` variables, `.betterleaks.toml`,
`.gitleaks.toml`, `.betterleaksignore`, `.gitleaksignore`, Git ignores,
attributes and inline `gitleaks:allow`/`betterleaks:allow` comments cannot
suppress these scans. Policy or wrapper edits themselves remain trusted code
changes that require review.

Reports are metadata-only templates sent through a captured pipe, with
`--redact=100` also enabled. No raw JSON/SARIF report is written. Input snapshots
contain the selected source bytes inside a private temporary directory; they
are removed on normal completion, errors and keyboard interruption. They are
inputs, not retained reports. Abrupt process termination, OS crash/swap and
privileged access are outside that cleanup guarantee.

Investigate each finding in the named local source location; do not paste
credential values into issues, logs or chat. Remove accidental credentials
from current files and arrange revocation through the credential owner when
appropriate. This tool never tests whether a credential works. Do not add
file-wide ignores, disable a rule or baseline the repository to obtain a pass.

The initial 59 working-file findings were reviewed. The policy's 35 exact
rule/file pairs cover 56 specific values across current and historical paths:

- Synthetic URI userinfo in rejection/redaction tests and hostile Markdown.
- Three explicitly fabricated BPR-1373 account canaries and one punctuation
  credential used by the redaction contract.
- Deterministic idempotency identifiers and publication-journal identity
  digests; they are not authentication keys.
- Source/frozen-candidate SHA-256 records and a documented digest prefix of a
  synthetic registered-device grant in preserved verification evidence.

Each allowlist uses `condition = "AND"`, an anchored exact file and anchored
exact values under only its relevant rule. The first value byte is expressed
as a regex byte escape so policy declarations do not become duplicate token
fixtures. The wrapper replaces `__BPR_PATH_PREFIX__` with exactly the snapshot
prefix or the empty history prefix; a different historical path that happens
to start with `input/files/` receives no exception. No source, test or
historical evidence was edited to evade detection.
The local course tests every exception against a different value of the same
rule in the same file, the same fake value in another file and another rule
in the allowed file. Keep these controls passing when changing policy.

History retains findings for old loopback attended-review session URL tokens
in `docs/issues/_checkpoints/BPR-1575/handoff.json` and
`docs/issues/_checkpoints/BPR-1576/handoff.json`. The current handoffs no longer
contain those URLs. These are not synthetic-fixture exceptions: they remain
visible, with no live validation, baseline or history rewrite. A historical
scan is therefore expected to return 1 for the current locally available refs:
18 historical occurrences at those two file locations remain visible.

## Hooks and removal

Installation refuses an existing different `pre-commit`, symlink or custom
`core.hooksPath`, preserving its contents/configuration. It does not silently
replace or chain a hook. A regular single checkout uses its default Git hooks
directory. When worktree configuration is already enabled, installation uses
that checkout's private Git directory and `git config --worktree core.hooksPath`.
Existing shared hooks prevent that switch because it would hide them. Linked
worktrees without `extensions.worktreeConfig=true`, or multiple worktrees
without isolated configuration, are refused. Setup never enables that shared
extension or changes another checkout's configuration on your behalf.

For an existing hook manager, add this command to its pre-commit script and
propagate a nonzero status, using the **absolute path to this checkout**:

```sh
python3 /absolute/path/to/BetterPullReview/scripts/betterleaks.py staged || exit $?
```

Keep `GIT_INDEX_FILE` inherited. The managed hook records its checkout's
absolute script path; after moving the checkout, remove/reinstall it at the
old location first or remove the stale managed hook manually. The hook needs
`python3` 3.10+ on the Git process's `PATH`, including GUI Git clients.

```sh
python3 scripts/betterleaks.py hook-remove
python3 scripts/betterleaks.py hook-status
```

Removal deletes only an exact matching managed hook and its owned worktree
setting. Modified/conflicting hooks and custom settings are refused. For a
manually integrated hook, remove only the command you added using that hook
manager's normal procedure. The cache can be deleted after hook removal;
leaving an active hook without its scanner intentionally blocks commits.

## Troubleshooting and local verification

- **Missing scanner:** run `setup` explicitly while network access is available.
  Scans never repair the installation automatically.
- **Conflicting/corrupt cache:** inspect and remove only the version/platform
  directory named in the diagnostic, then rerun setup. Do not replace the pin
  with a moving checksum or an unverified binary.
- **Missing Git/Python or permissions:** install the required tools or correct
  local permissions. GUI Git may have a different `PATH` from your terminal.
- **Incomplete Git objects:** use a complete local repository. Any decision to
  obtain missing objects is a separate developer action; scans never fetch.
- **Hook conflict/custom path:** preserve the existing hook manager and use the
  manual integration above. No global configuration change is needed.
- **Scanner error:** raw child output is deliberately withheld. Verify the
  pinned cache with setup, inspect repository readability and the tracked
  tooling configuration, and run the disposable integration course below.

The optional course requires Python **3.11+** (`tomllib`) and an already
installed scanner. It is intentionally outside normal test discovery:

```sh
python3 scripts/betterleaks/check_local.py
```

On 2026-10-19, the real pinned macOS arm64 binary was exercised on macOS 26.6.2
with Python 3.14.7 and Apple Git 2.50.1. The full course runs under
`sandbox-exec` with network access denied and checks that denial with a socket
probe. It creates disposable repositories, installs only a copy of the
verified local executable, runs actual Git commits and removes its fixtures.
Linux/Windows archives have verified hashes; their execution and hooks were
**not tested** here. On other platforms the course reports that OS-level
network denial was not exercised.

Pattern scanning is not proof that a repository has no secrets. Upstream
content/entropy filters remain; unsupported binary formats, archives beyond
eight levels and encodings beyond five decode levels can be missed. Symlink
targets are scanned as link text, never followed. Submodules and LFS backing
objects are not fetched or traversed. History covers commit diffs reachable
from local refs/HEAD, not commit messages, reflog-only/unreachable objects,
unavailable remotes or material beyond a shallow boundary. Shallow history is
reported explicitly. Keep files/index stable during a scan. These tools do
not alter the application's security model or release qualification claims.
