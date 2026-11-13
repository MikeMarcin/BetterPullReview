# BPR-2131: consistent wrong-volume ownership fixture

## Cause and bounded correction

Frozen Build60 Windows Release reached `agent_workspace_ownership` and failed
at the wrong-volume expectation: `record-malformed` was returned where
`record-moved` with `finding=other-volume` was expected. The non-Mac fixture
changed the immutable Registry volume only in its final phase7 record. The
existing phase-transition validator correctly rejects that inconsistent chain
before it compares live volume identity. Linux uses the same fixture branch.

The correction changes Registry volume consistently in every legacy line for
that case. Its exact moved/other-volume and byte-preservation requirements stay
unchanged. The former final-line-only mutation is retained as a separate
malformed-chain negative, with exact malformed and byte-preservation checks.
The existing macOS wrong-volume construction is unchanged.

Only `tests/scripts/agent_workspace_ownership_tests.py` changes at runtime
(21 insertions, two deletions, only `check_remount_and_record_refusals`). No
production C++, parser, app pin, CMake, registration, profile, native helper,
library or distribution input changes. The separate own issue/plan/proof
metadata does not enter frozen Build60.

## Bound identities

- Mandate: `bpr2131-workspace-fixture-r1`.
- Original and protected native source: `6efba00e1afbf874c88c7e5de2623a33528ff36f`.
- Private correction: `d30c640f1d71507426db225f24d0d5a654027ec0`.
- Main correction: `f9442ef78bb02f0e556ff381713f89eb3962073e`.
- Original Python SHA-256: `ea0a4849a6320af3cda539953218425674e5acebe2599ca922781318819bc583`.
- Corrected Python SHA-256: `99568d9aac3080f0e3a0cb9ca8b45545c80f3400152a24e7061101c180f91b02`.
- Unchanged installed Git adapter SHA-256: `5c7892ada6beb6a96d90d21753fb415f200d303ae51c6cae4ee019975c1c7486`.

The isolated source baseline's fixture bytes were checked equal to the actual
frozen60 fixture, not inferred from its older branch ancestry.

## Native corrective courses

On 2026-11-12, Windows Release and Debug ran serially, followed by Linux
Release and Debug. Each was the full ownership course, using its unchanged
compiled60 helper and actual configured CTest row, including the normal
120-second bound and failure expressions.

| Platform | Configuration | Exit | Bound | Both preservation markers | Migration receipts | Input seals | Remaining owned processes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Windows | Release | 0 | 120 s | present | 17 | exact | empty |
| Windows | Debug | 0 | 120 s | present | 17 | exact | empty |
| Linux | Release | 0 | 120 s | present | 17 | exact | empty |
| Linux | Debug | 0 | 120 s | present | 17 | exact | empty |

Every course emitted `AGENT_WORKSPACE_OWNERSHIP_OK` and platform-specific
`wrong-volume` and `malformed-chain` preservation markers. Each of its 17
existing migration receipts retained the edit and original journal inode.
No course was rerun to replace an observed native failure.

Windows used configured presets `windows-qualification-release` and
`windows-dev`. The real Python 3.14 command ran in the configured
`build/windows-dev/tests/git_contract` directory and launched the original
Release/Debug helper. The normal Windows registration is uncontained; no
sandbox wrapper was invented. Its original environment modifications, Qt PATH
and font path were applied. The fresh task used the approved interactive
nonadmin session1/SID1001 context, had no automatic trigger, and completed
Ready/result0.

Linux used configured presets `qualification-release` and `dev`. Each original
configured `better_pull_review_network_sandbox` wrapper, Python command, working
directory and environment modification was retained under ordinary UID1000.
Windows and Linux were serialized on the shared PC; no native Mac command ran.

The sole command delta was replacing the normal Python script token with an
external runpy loader. That loader verified the corrected script hash and
resolved both support modules from the immutable normal source. Disposable
TMPDIR/TEMP/TMP roots were private, and bytecode writes to normal source were
disabled. No private executable, app, parser or helper replaced a producer input.
The driver checked configured failure expressions, final and per-case markers,
and all 17 migration receipts.

The before/after seals include normal source/script, allocation0060, actual
helper, stock support modules, CMake cache, CTest registration files, sandbox
where applicable, interpreter, maintained process guard and external plumbing.
They match exactly; normal source remains clean at the same 6ef commit.
The maintained native supervisor recorded all processes and completion with
empty remaining owned forests. Compiled helper SHA-256 values:

- Windows Release: `97f2d92bcf29cef3f8aecb83bfe72e8cf4df36d2f7dab34ef362e2b8b42de0b1`.
- Windows Debug: `d46a00814d86d692a4e8f9f9db24a0c8ef70fecbc1095cdc7a4589e9683d060f`.
- Linux Release: `c735234b139badeecfce48910b757a4c2e633d87af61d21e22d817dc649defab`.
- Linux Debug: `99bf6de1ce6602342bd7ce9ad9b12afe1683695425f5b1cff18aedb700d266c8`.

## Evidence and scope

External evidence root:
`/Users/jaeger/.codex/engineering-runs/bpr60-workspace-fixture-r1`.
The two full receipts retain exact configured and executed argv, environment
modifications, working directories, context, input paths/hashes before/after,
raw-log hashes, process identities, markers and migration records:

- `windows-receipt.json`: `1733da275fcfa4f30ce8dc0cdc4ca9e987b98e2cb541f6463c14805507c538c5`.
- `linux-receipt.json`: `6ccefee15868ed84eb917063be1597567ed18f561ddc1f454fd3354c48ea3cce`.

The matching `windows-raw/` and `linux-raw/` directories retain original course
stdout/stderr and receipts. The Windows native state is separately retained
under its fresh `bpr2131-private-r1` root; Linux state under
`/home/mmarcin/bpr-release-diagnostics/bpr2131-r1`. Local collection had two
formatting/parser errors after successful courses (PowerShell JSON decoration
and a compact-summary-plus-pretty-JSON stream); raw receipts were subsequently
copied/decoded without rerunning courses. Neither was a native course failure.

The original frozen60 Windows comprehensive failure and unreached tail remain
recorded in the release run's `candidate60` raw retired course. These bounded
corrective passes do not turn that original run into an all-zero result. The
coordinator explicitly selected the existing test-only correction route: keep
frozen producer source/configuration/binaries and public Mac60, then assess
these separate proofs alongside fresh full native courses. Full release
qualification, adoption and publication remain the release coordinator's work.

Cheap checks: Python syntax, AST changed-definition/source-scope proof,
`git diff --check`, documentation gate and the explicit own plan under pinned
MASK965. Main global plan discovery retains the foreign `release-v041` unknown
`amendments` field refusal; no runtime upgrade or unrelated metadata repair
was performed and no global plan readiness is claimed.
