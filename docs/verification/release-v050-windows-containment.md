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

Twelve direct baselines failed natively, independent of containment. Each
was then fixed at its owner and passes directly in Release and Debug on the
station (the follow-up section of BPR-2215 has the native table):

- Six local-draft desktop courses built `local_draft_desktop` with no Windows
  system root, which the installed-Git adapter needs on Windows; the
  application passes one. The courses now pass the same root the Git
  contracts do.
- `local_draft_publication` expected the push's checkout unquoted; the core
  writes it as a POSIX shell word, so a Windows path is quoted.
- `application_local_draft_author_desktop_public_path` and
  `application_tour_chat_queue_local_draft_public_path` ran `/bin/sh`; they run
  their fixture with Git for Windows' `sh.exe`. The author's Git itself runs
  on POSIX only, so the author course ends at Check Again on Windows.
- In Author, This update showed nothing while it had no file of its own, as on
  Windows always; the viewer now shows the draft's proposed diff.
- `local_draft_desktop` left the search panel's destroyed handler connected
  past its own lifetime: an access violation when the window outlived it.
- `application_agent_chat_setup_list_contract` expected POSIX separators.
- `application_ask_report_public_path`: the report row asked one pixel more
  than its layout gave it; it now answers what its layout does.
- `application_ask_queue_public_path` paused the queue before What changed
  ran when the fixture agent started later; it waits for the run.
- `application_home_next_move_public_path` never left the account setup's
  Ready step; it failed on every platform and now signs in as the other
  courses do.
- `application_local_comments_composition_public_path` passed in every later
  native run without a change.

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

## The rows main added after the first merge

Main registered four more `WINDOWS_UNCONTAINED` rows after the lane's first
merge. Three were measured the same way at
`b9a0915ec87f7a533abc2d5f490ebac519b26d88`, in the same Interactive Limited
context with a freshly built Release graph. The fourth,
`application_commit_asks_public_path`, is registered only off Windows
(`if(APPLE OR UNIX)`), so Windows never configures it; its pin stays
unmeasured, as the report course's does.

| Course | Direct exit | Contained exit | First contained observation |
| --- | --- | --- | --- |
| `application_returning_review_mark_public_path` | 0 | 8 | `private data root refused` |
| `application_review_seen_stays_public_path` | 0 | 8 | `FAILED: private desktop root under the canonical storage policy` |
| `application_review_seen_fails_public_path` | 0 | 8 | `FAILED: private desktop root under the canonical storage policy` |

The direct courses pass in 7.81, 9.08 and 8.59 seconds. The receipt is
`measure-release-3-receipt.json` beside the others (SHA-256
`1d81c5ef0eb09c2c7e328047cd54397c11e4c550a83e280f57a1514858b82235`); the
stdout/stderr pairs and generated CTest files are in `measure-release-3.zip`
(SHA-256
`6a8ec7a06e1addd3190e809c6398918377710874955037f612cb0fa070cf3dd7`).

## Agent setup store courses — `cb200dfa9`

`qt_prompt_macro_chips` and `qt_agent_finding_cards_restart` open the agent
setup store, which the application opens outside any AppContainer and whose
own course, `sqlite_agent_setup_storage`, is already registered
`WINDOWS_UNCONTAINED`. In the full Windows Release suite at `cb200dfa9` they
ran contained. Each one's restart read nothing back: "AC07 the store gives the
name back after a restart" (a direct probe neither wrote nor read), and "the
fresh host reads the one kept card back" (loaded, nothing kept). Both are now
registered `WINDOWS_UNCONTAINED`, pinned
`prompt-turn-name-store-predicate-failed` and
`agent-finding-card-store-predicate-failed`.

`record_rfc0017_anonymous_contracts_recorder` failed contained in both full
suites (`f5343b1e1`, `cb200dfa9`): Git for Windows' `git init --quiet` could
not create `.git/HEAD` in the AppContainer's temporary folder ("cannot lock
ref 'HEAD'"). It was registered `WINDOWS_UNCONTAINED` and pinned
`scratch-git-init-failed`. Uncontained at `5c0a5c902`, once its fixtures wrote
LF, the recorder itself refused: "capture bindings directory cannot be opened
for durable sync". It syncs each capture's directory, which Python cannot open
on Windows, and its contract asserts that sync, so the row is now registered
only off Windows and the pin is gone.

## Rows main added by `392dffc6f` — measured at `fb60e393e`

| Course | Direct exit | Contained exit | First contained observation |
| --- | --- | --- | --- |
| `sqlite_prompt_pack_storage` | 0 | 8 | `FAILED: the sender's prompts are stored` |
| `application_agent_chat_held_turn_public_path` | 0 | 8 | no output (exits as the agent-chat helpers courses do when the private test root is refused) |

Pinned `prompt-pack-store-predicate-failed` and `private-test-root-refused`.
`application_draft_description_public_path` is registered only off Windows
and stays unmeasured, as do the commit-asks and report courses.
`application_window_file_drop_public_path` is pinned unmeasured until the next
Windows measurement.
