# 0.5.0 Windows containment registration measurements

[[BPR-2215]]. The 0.5.0 native Windows risk preflight failed
`test_network_isolation_manifest` in Release and Debug: 47 configured rows
were still pinned `unmeasured (registered on macOS)`, and four rows were
registered `WINDOWS_UNCONTAINED` with no pin at all. This records the native
measurement that replaced those pins.

The station is the approved native Windows 11 x64 producer, in its Limited
Interactive context (scheduled task with Interactive logon, session 1, user
`mikem`), MSVC from Visual Studio 18 and Git for Windows. The source is
`12c1f9250180a8a58fa722f325d2363daee07951`, clean, with its tests-enabled
Release graph freshly built. For each row the configured CTest command ran
first directly (`ctest -C Release -R ^name$`), then unchanged behind the
maintained zero-capability AppContainer launcher
(`better_pull_review_network_sandbox.exe`), from a generated CTest file that
keeps every configured property: arguments, environment, working directory,
pass and fail expressions, and deadline. Program and launcher images hash the
same before and after. The paired native work totals 597 seconds.

Exit 8 is CTest's failed-test exit. The observation is the first line the
contained child printed, or what it did when it printed nothing.

| Course | Direct exit | Contained exit | First contained observation |
| --- | --- | --- | --- |
| `sqlite_discuss_preferences_store` | 0 | 8 | `FAILED: a profile with no store reads nothing chosen, writable` |
| `sqlite_agent_configuration_storage` | 0 | 8 | `FAILED: the sender's agents are stored` |
| `local_draft` | 0 | 8 | `FAILED: the profile root` |
| `local_draft_publication` | 8 | 8 | `FAILED: the profile root` |
| `local_changes` | 8 | 8 | child exit 0, no output: the course is compiled out on Windows |
| `application_agent_chat_helpers_public_path` | 0 | 8 | child exit 1; only the QTemporaryDir cleanup diagnostic |
| `application_agent_chat_helpers_unfinished_public_path` | 0 | 8 | child exit 1; only the QTemporaryDir cleanup diagnostic |
| `application_agent_chat_custom_agent_refused_contract` | 0 | 8 | child exit 3, no output |
| `application_agent_chat_subject_contract` | 0 | 8 | `agent-chat-composition-failure:private-test-root` |
| `application_agent_chat_setup_list_contract` | 8 | 8 | child exit 3, no output |
| `application_account_setup_public_path` | 0 | 8 | `FAILED: application startup succeeds` |
| `application_profile_settings_export_public_path` | 0 | 8 | `FAILED: application startup succeeds` |
| `application_enhance_comment_note_public_path` | 0 | 8 | child exit 1; only the QTemporaryDir cleanup diagnostic |
| `application_review_tour_link_public_path` | 0 | 8 | child exit 1; only the QTemporaryDir cleanup diagnostic |
| `application_review_tour_publication_push` | 0 | 8 | child exit 1; only the QTemporaryDir cleanup diagnostic |
| `application_returning_review_latest_public_path` | 0 | 8 | `private data root refused` |
| `application_returning_review_addressed_public_path` | 0 | 8 | `private data root refused` |
| `application_returning_review_gone_public_path` | 0 | 8 | `private data root refused` |
| `application_explain_file_public_path` | 0 | 8 | `private data root refused` |
| `application_agent_keep_action_public_path` | 0 | 8 | `private data root refused` |
| `application_suggest_change_public_path` | 0 | 8 | `private data root refused` |
| `application_dependable_session_public_path` | 0 | 8 | `FAILED: private desktop root under the canonical storage policy` |
| `application_home_next_move_public_path` | 8 | 8 | `FAILED: private desktop root under the canonical storage policy` |
| `application_home_oversized_public_path` | 0 | 8 | `FAILED: private desktop root under the canonical storage policy` |
| `application_home_approve_again_public_path` | 0 | 8 | `FAILED: private desktop root under the canonical storage policy` |
| `application_discuss_busy_public_path` | 0 | 8 | `FAILED: private composed course root` |
| `application_local_comments_composition_public_path` | 8 | 8 | `FAILED: private composed course root` |
| `application_local_comments_found_public_path` | 0 | 8 | `FAILED: private composed course root` |
| `application_local_draft_desktop_public_path` | 8 | 8 | `FAILED: fixture git command` |
| `application_local_draft_author_desktop_public_path` | 8 | 8 | `FAILED: the author-state fixture is made` |
| `application_local_draft_tour_desktop_public_path` | 8 | 8 | `FAILED: fixture git command` |
| `application_local_draft_publication_desktop_public_path` | 8 | 8 | `FAILED: fixture git command` |
| `application_ask_menu_public_path` | 0 | 8 | `private data root refused` |
| `application_ask_addressed_public_path` | 0 | 8 | `private data root refused` |
| `application_ask_report_public_path` | 8 | 8 | `private data root refused` |
| `application_prompt_macros_public_path` | 0 | 8 | `private data root refused` |
| `application_ask_queue_public_path` | 8 | 8 | `private data root refused` |
| `application_tour_feedback_queue_public_path` | 0 | 8 | `private data root refused` |
| `application_agent_queue_windows_public_path` | 0 | 8 | `private data root refused` |
| `application_agent_queue_second_window_loading_public_path` | 0 | 8 | `private data root refused` |
| `application_agent_queue_close_runs_on_public_path` | 0 | 8 | `private data root refused` |
| `application_agent_queue_restart_public_path` | 0 | 8 | `private data root refused` |
| `application_agent_queue_tour_feedback_crash_public_path` | 0 | 8 | `private data root refused` |
| `application_agent_queue_finish_pauses_public_path` | 0 | 8 | `private data root refused` |
| `application_tour_chat_queue_stop_public_path` | 0 | 8 | `private data root refused` |
| `application_tour_chat_queue_whole_tour_public_path` | 0 | 8 | `private data root refused` |
| `application_tour_chat_queue_local_draft_public_path` | 8 | 8 | `private data root refused` |
| `application_review_seen_public_path` | 0 | 8 | `FAILED: private desktop root under the canonical storage policy` |
| `application_local_draft_check_desktop_public_path` | 8 | 8 | `FAILED: fixture git command` |
| `application_home_open_responsive_public_path` | 0 | 8 | `FAILED: private desktop root under the canonical storage policy` |
| `application_prompt_facts_public_path` | 0 | 8 | `private data root refused` |

No row passes contained, so every registration stays `WINDOWS_UNCONTAINED`,
except `local_changes` below. Each pin in `WINDOWS_UNCONTAINED_TESTS` names
the observed first predicate, without an inferred Win32 or ACL cause. A course
that printed nothing is pinned by what its source does at that point: the
agent-chat helpers and the review-tour courses return `EXIT_FAILURE` silently
when `private_data_root` refuses (`private-test-root-refused`, as the
[[BPR-1797]] tour courses are); the two custom-agent-home courses exit 3
before any output (`child-exit-3-without-output`), with no cause sampled.

`local_changes` compiles its course out on Windows and printed nothing, so
the row failed its pass expression directly as well as contained. It now
prints `local-changes:skipped (the course runs on POSIX only)` on Windows and
is registered contained; it runs no Windows coverage either way.

Twelve direct baselines failed natively, independent of containment, and
remain open Windows functional failures for the release:
`local_draft_publication` (AC01 push naming),
`application_agent_chat_setup_list_contract` (`setup-list-second`),
`application_home_next_move_public_path` (Home lists every discovered pull
request), `application_local_comments_composition_public_path`, the five
local-draft desktop courses (`application_local_draft_desktop_public_path`,
`_author_`, `_tour_`, `_publication_`, `_check_`),
`application_ask_report_public_path`, `application_ask_queue_public_path` and
`application_tour_chat_queue_local_draft_public_path`. Their contained
refusals occur before the direct failure point, except
`application_local_draft_author_desktop_public_path`, which fails directly at
the same predicate; its pin establishes no containment-specific refusal.

The external record is
`/Users/jaeger/.codex/release-runs/v0.5.0-20261116/windows-preflight-fix-r1/`.
`measure-release-receipt.json` (SHA-256
`be0bcf87d3129f81e5424cf894053175f678f32a31e7224dc052a8ac6d4368ea`) holds
the configured commands and properties, image hashes, exits and durations;
`measure-release.zip` (SHA-256
`ebaaac2a64e32e6e2589b7df49c77e6f13c7abf853035e377e3e83ac3a43864d`) holds
every stdout/stderr pair and generated CTest file, and the measurement script
is beside them. This is a finite inventory measurement, not Release or Debug
qualification. No registration's assertions, deadline or network capability
changed.

## Merged-main registration

[[BPR-2245]] registered `application_ask_setup_review_public_path` on main
while this measurement ran. It was measured the same way at
`877f8091b241fd026c797321b24ae5bc653f151a`, the lane with main merged, in
the same Interactive Limited context with a freshly built Release graph.

| Course | Direct exit | Contained exit | First contained observation |
| --- | --- | --- | --- |
| `application_ask_setup_review_public_path` | 0 | 8 | `private data root refused` |

The direct course passes in 14.39 seconds. Its receipt is
`measure-release-2-receipt.json` beside the first one (SHA-256
`8c1a8621839240a13450efeb2d5b81d366d845231febb2dec0b8ba2bea5cf8ab`).
