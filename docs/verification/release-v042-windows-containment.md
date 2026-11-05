# 0.4.2 Windows containment registration measurements

[[BPR-1854]] measures seven newly registered courses on the approved native
Windows 11 x64 station, in its Limited Interactive producer context, with Qt
6.12.0 and MSVC 14.51.36231. The source is
`5a86e28ed230462c48e5eba0d159f08eb07010c3`.

The completed native Release test build supplied these images. Its later Debug
interruptions and the archived build intermediates do not alter the retained
Release executables. Each pair executes the actual configured CTest command
first directly, then under the current zero-capability AppContainer launcher.
Arguments, environment modifications, working directory and deadline are retained.
The captured program, application and launcher images have matching pre/post
hashes. This is an observation of the registered courses, not a complete Release
or Debug qualification result.

| Course | Direct exit | Contained exit | First contained observation |
| --- | --- | --- | --- |
| `mimalloc_tool_better_pull_review_gh_avatar_helper` | 0 | 0 | `MIMALLOC_PROBE_PASS C C++ Qt cross-module cross-thread OOM` |
| `sqlite_view_filter_memory_store` | 0 | 3 | `FAILED: a profile with no store reads nothing remembered, writable` |
| `sqlite_pull_request_activity_store` | 0 | 3 | `FAILED: a profile with no store reads a fresh memory, enabled and writable` |
| `application_draft_state_contract` | 0 | 1 | `STARTUP_PROFILE_REFUSAL:registry_unavailable` |
| `application_progressive_open_public_path` | 0 | 1 | `FAILED: private desktop root under the canonical storage policy` |
| `application_refresh_current_file_public_path` | 0 | 1 | `FAILED: private desktop root under the canonical storage policy` |
| `application_home_return_public_path` | 0 | 1 | `FAILED: private desktop root under the canonical storage policy` |

All seven direct courses emit their required pass markers. The avatar helper's
allocator probe also passes contained, so its `WINDOWS_UNCONTAINED` registration
and pending exemption are removed. The other six retain narrow pins naming the
observed failed predicate. A combined store assertion does not identify which
conjunct failed; these observations do not establish a Win32 or ACL cause. No
network capability or filesystem permission is widened.

The external record is
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/windows-containment-r5/`.
Its `receipt.json` SHA-256 is
`461cd13f688fb608a9ae28a3b13ecde2352e6de6f3715d588cc680e9daf30e52`.
It retains the configured manifest, exact commands and image hashes, native
process ownership and exits, and all fourteen stdout/stderr pairs. The paired
native work totals 57.154 seconds; this is not a full-suite duration.

The registration and pin corrections change test metadata only. A successor
release source receives its own build identity and fresh required qualification;
these observations do not transfer a security disposition or accept changed
packages. The earlier interrupted build and measurement-prerequisite attempts
remain failed or unrun in the external release record.


## Updated-main registrations

At `3ccc4369e8970c2371601444d95f4ac48b4215e3`, four further actual
Release registrations were measured in the same approved native Limited
Interactive Windows context. All four direct courses pass (CTest exit 0,
61.40 seconds). Each unchanged command was then prefixed with the maintained
zero-capability AppContainer launcher, preserving configured arguments,
environment, working directory and deadline.

| Course | Direct exit | Contained child exit | First contained observation |
| --- | --- | --- | --- |
| `application_home_own_activity_public_path` | 0 | 1 | private desktop test root under the canonical storage policy |
| `application_home_filter_public_path` | 0 | 1 | private desktop root under the canonical storage policy |
| `application_open_pull_request_activity_public_path` | 0 | 1 | private desktop root under the canonical storage policy |
| `application_draft_delete_contract` | 0 | 1 | CMake `file` failed to set permissions on the owned draft-delete-contract root |

Each contained CTest invocation exits 8 with the actual child exit 1 retained.
The pins name only the observed failed predicate; no inferred Windows error
or broader ACL cause is claimed. No command, assertion, deadline or network
capability changes. The original mandatory manifest failure remains recorded.

The external evidence is
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/main-feedback/measure-main-containment-r1/`.
Its `receipt.json` SHA-256 is `38dd40e56a782cd1562f7ef60bdc993d35603197a8f83dae7a2be46f5d7c3c63`.
It retains the configured rows, generated replay files, native process
identities, raw commands, exits and stdout/stderr. This is finite inventory
measurement, not comprehensive qualification or package approval.
