#!/usr/bin/env python3
"""Verify that every configured automated test crosses the network boundary."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
from dataclasses import dataclass
from typing import Any, Callable


RAW_REGISTRATION = re.compile(r"\badd_test\s*\(", re.IGNORECASE)
ISOLATED_REGISTRATION = re.compile(
    r"\bbpr_add_network_isolated_test\s*\(", re.IGNORECASE
)
LIVE_REGISTRATION = re.compile(
    r"\bbpr_add_designated_live_mutation_test\s*\(", re.IGNORECASE
)
EXACT_LIVE_REGISTRATION = re.compile(
    r"\bbpr_add_designated_live_mutation_test\s*\(\s*\)", re.IGNORECASE
)
ISOLATED_REGISTRATION_ARGUMENTS = re.compile(
    r"\bbpr_add_network_isolated_test\s*\(([^()]*)\)", re.IGNORECASE
)
UNCONTAINED_KEYWORD = re.compile(r"\bWINDOWS_UNCONTAINED\b")
RAW_REGISTRATION_ARGUMENTS = re.compile(r"\badd_test\s*\(([^()]*)\)", re.IGNORECASE)
RAW_REGISTRATION_OWNER = "cmake/BprTestIsolation.cmake"
LIVE_TEST_NAME = "live_review_mutation_designated_target"
LIVE_TEST_LABEL = "bpr_live_mutation"
WINDOWS_BACKEND = "windows-appcontainer"
# [[BPR-1343]]. The label the test wrapper adds, on every platform, to a row
# registered WINDOWS_UNCONTAINED, and the pin a non-Windows author may write
# until the Windows station measures the row: "unmeasured (registered on
# macOS)". Windows refuses that pin until it is replaced by the measured line.
WINDOWS_UNCONTAINED_REGISTRATION_LABEL = "bpr_windows_uncontained_registration"
UNMEASURED_PIN_PREFIX = "unmeasured (registered on "
WINDOWS_UNCONTAINED_LABEL = "bpr_windows_uncontained"
# The Windows boundary is an AppContainer, and three things a lowbox token
# cannot do are needed by the tests below; each was measured on the Windows
# host on 2026-08-22 (BPR-0362), passing uncontained and failing contained at
# exactly the named call. None is a property of the test, and none can be
# granted with a directory ACL, because every object involved is a kernel
# object whose DACL names Everyone, SYSTEM, Administrators and RESTRICTED and
# never ALL APPLICATION PACKAGES.
#
# - `dos-volume-name`: `GetFinalPathNameByHandleW` with VOLUME_NAME_DOS fails
#   while the NT form resolves completely, because translating the volume
#   device into a drive letter needs `\GLOBAL??\C:` and the mount-point
#   manager. The product's retained ancestor chain asks for the DOS form, as do
#   CPython's `realpath` (`Path.resolve(strict=True)`) and Git's `getcwd`.
# - `batch-stub`: `cmd.exe` refuses to run a `.bat` file, so a test whose
#   stand-in tool is a batch script gets no tool at all. CPython's own startup
#   `realpath` warning also lands on stderr, which the format gate compares.
# - `ninja-named-pipe`: `ninja` fails with `CreateNamedPipe: Access is denied`,
#   so a test that configures a CMake fixture stops inside the compiler check.
# - `qprocess-named-pipe`: the same denial reaching `QProcess`, which carries
#   a child's standard streams over a named pipe.
# - `credential-manager`: the Windows Credential Manager refuses a lowbox
#   token; the native vault tests pass uncontained and fail inside.
# - `child-process-denied`: `CreateProcess` is refused outright with WinError 5,
#   before the tool runs at all. Measured on the Windows station on 2026-09-17
#   ([[BPR-0872]]). The image's own ACL is not always the reason: on 2026-09-23
#   `git.exe` under Program Files granted ALL APPLICATION PACKAGES read and
#   execute, while the per-user Python the build resolved granted the container
#   nothing. This is a different obstacle from `dos-volume-name`, which is what
#   Git hits *after* it starts; the pin names the first failure, not the second.
# - `repository-content-outside-checkout`: the row reads content the repository
#   points at but does not contain. `scripts/verify_documentation.py` imports
#   `board_model` from `.agents/skills/implement-rfc/scripts`, which on the
#   Windows station is a symlink out of the checkout into the MASK store under
#   the user profile. The container is denied it, and a Git child lies behind
#   that as well. Measured 2026-09-17 ([[BPR-0873]]). Unlike the others this is
#   not at heart a lowbox limitation -- it is a fact about the checkout that
#   would bite any sandbox -- but it is measured here and it is why these rows
#   cannot be contained.
# - `service-manager-denied`: the Service Control Manager refuses a lowbox
#   token. The row runs the AppContainer launcher itself, and a launcher
#   started inside the container stops at its firewall-service check with
#   `BPR_NETWORK_ISOLATION_UNAVAILABLE:firewall-service:win32=5`, before it
#   reaches the profile. Measured on the Windows station on 2026-09-24
#   ([[BPR-1072]]).
#
# The application tests are `dos-volume-name` through one step: the
# application canonicalises its data root and the catalog walks that root's
# ancestor chain, both of which need the drive letter (BPR-0412, BPR-0436).
#
# The set is exact in both directions: an uncontained test outside it fails
# the manifest gate, and a member found contained fails it too, so the pin has
# to move when the truth does. The value names the cause so the next reader
# does not have to re-measure it.
#
# BPR-0903. `test_network_isolation_external` and `_child` are never pinned
# here: each passes only on the container's own refusal, for itself and for a
# child it starts, and fails uncontained. The reason is beside their
# registration in tests/CMakeLists.txt.
# [[BPR-1525]]. Nine formerly unmeasured registrations were measured on the
# Windows station, Interactive Limited session 1, Git 2.55.0.windows.3, on
# 2026-10-15. All nine pass normally and fail contained at the named boundary.
# The exact commands and normal/contained evidence are recorded in
# docs/verification/BPR-1525-windows-release-gates.md. Three newly measured
# causes are distinct from the historical DOS-name failure above:
# - `ancestor-control-denied`: the store's validate_ancestor_chain opens C:\
#   with FILE_READ_ATTRIBUTES | READ_CONTROL and receives Win32 5, although
#   the retained scratch root's DOS name and own private descriptor work.
# - `owner-private-acl-denied`: QTemporaryDir canonicalization succeeds, but
#   its root refuses the WRITE_DAC needed to narrow it, Win32 5; icacls in the
#   older-theme script reports the same access denial. No broad ancestor ACE
#   is added to make these application courses run inside the container.
# - `git-reference-parent-denied`: Git starts, creates .git and refs, then
#   the exact isolated fixture init command refuses the initial HEAD with
#   "cannot lock ref 'HEAD': unable to create directory for .../.git/HEAD".
#   The probe uses the actual scratch descriptor and fixture git_runner,
#   rather than assuming a process-launch or DOS-name failure. This names
#   Git's measured refusal, without claiming an unobserved Win32 cause.
# No registration, network capability, assertion or deadline is changed.
# [[BPR-1647]]. On 2026-10-19 the complete26-scenario native updater HTTPS
# course passes directly, while its contained Git-for-Windows MSYS OpenSSL
# exits3221225794 at NtCreateDirectoryObject(\BaseNamedObjects\msys-2.0S5-...),
# status0xC0000022. `msys-runtime-object` names that exact external runtime
# namespace refusal, not a child-launch or DOS-name assumption. Only this row
# receives the measured pin; loopback scope and all course assertions remain.
WINDOWS_UNCONTAINED_TESTS: dict[str, str] = {
    "release_update_native_https": "msys-runtime-object",
    "sqlite_device_binding_storage": "dos-volume-name",
    # BPR-0806: measured on JaegAW 2026-09-17. Contained, std::filesystem::canonical
    # on the package temp root reports "Access is denied" before any course step
    # (the registry, restoration and storage contract fixtures), and the profile
    # public paths' make_storage_root_owner_private refuses the same root; all
    # five pass uncontained.
    "sqlite_profile_registry_storage": "dos-volume-name",
    "sqlite_remembered_connection_storage": "ancestor-control-denied",
    # [[BPR-1628]]. Native direct storage course passes on 2026-10-18;
    # the shared ancestor primitive receives Win32 5 for READ_CONTROL on C:\
    # inside the AppContainer. The scratch root and its DOS name succeed.
    "sqlite_insights_storage": "ancestor-control-denied",
    "sqlite_window_review_restoration_storage": "dos-volume-name",
    "application_windows_profile_storage_startup_contract": "dos-volume-name",
    "application_profile_management_public_path": "dos-volume-name",
    "application_profile_startup_public_path": "dos-volume-name",
    # [[BPR-1631]]. Measured Interactive Limited session 3, 2026-10-18:
    # actual direct/contained courses and native admission probes. See
    # docs/verification/release-v030-engineering.md. Private C++/Python
    # roots reach the ancestor refusal; Qt roots refuse WRITE_DAC first.
    "sqlite_profile_data_reset_files": "ancestor-control-denied",
    "application_profile_data_reset_public_path": "owner-private-acl-denied",
    "application_insights_public_path": "owner-private-acl-denied",
    "application_profile_recovery_public_path": "ancestor-control-denied",
    # BPR-0810: measured on JaegAW 2026-09-17. Contained, both fixtures stop at
    # "FAILED: private desktop test root under the canonical storage policy",
    # make_storage_root_owner_private refusing the package temp root exactly as
    # the BPR-0806 profile rows do; both pass uncontained.
    "application_profile_multi_window_public_path": "dos-volume-name",
    # [[BPR-1027]]: measured on JaegAW 2026-09-23. Contained, the course stops
    # at "FAILED: the test root's parent is owner-private", the same
    # make_storage_root_owner_private refusal of the package temp root as the
    # BPR-0810 rows; it passes uncontained.
    "application_profile_second_window_close_public_path": "dos-volume-name",
    # [[BPR-0978]]. The same two-profile registry, private-root and fixture
    # composition as the BPR-0806 profile rows above, so the same refusal;
    # pinned by that shape, not yet measured contained on Windows.
    "application_profile_two_account_isolation_public_path": "dos-volume-name",
    # [[BPR-0990]]. The multi-window row's private root and fixture composition,
    # so the same refusal; pinned by that shape, not yet measured contained.
    "application_personal_discovery_public_path": "dos-volume-name",
    "application_home_waiting_public_path": "dos-volume-name",
    # [[BPR-1357]]. The personal-discovery row's private root and fixture
    # composition. Measured on JaegAW 2026-10-09 at 66c1cf4c8: passes
    # uncontained; contained through the sandbox launcher it exits 3 with
    # "private desktop root under the canonical storage policy" before any
    # course step.
    "application_home_open_feedback_public_path": "dos-volume-name",
    "application_setup_destination_public_path": "dos-volume-name",
    # [[BPR-1089]]. Measured on bpr-main-win 2026-10-07 at e31a3f357, each
    # registered contained for the measurement: the courses stop at "FAILED:
    # private overview-feedback course root" and "FAILED: private
    # overview-labels course root" in under 0.2 s, the same refusal of the
    # private storage root as the setup-destination row whose executable they
    # share; both are registered uncontained and pass there.
    "application_overview_feedback_public_path": "dos-volume-name",
    "application_overview_labels_public_path": "dos-volume-name",
    # RFC-BPR-0084. The markdown-images course of the same setup-destination
    # executable; the actual course and shared Qt-root probe now measure its
    # private-root refusal on the Windows station ([[BPR-1631]]).
    "application_markdown_images_public_path": "owner-private-acl-denied",
    "application_merge_base_public_path": "owner-private-acl-denied",
    # RFC-BPR-0089. The commit-filter course, registered uncontained like the
    # merge-base course; the Windows station has not measured it yet.
    # [[BPR-1694]]. Measured on native Windows, Interactive Limited, 2026-10-21,
    # expanded RFC0089 M3 source a023d1d81. The exact registered application
    # command fails at its private course root; a scoped QTemporaryDir probe
    # confirms CreateFileW(WRITE_DAC) refuses that parent with Win32 5.
    # All three fixture builder variants fail Git init at "cannot lock ref
    # 'HEAD': unable to create directory for .../.git/HEAD". Direct courses
    # pass, including all M3 split/rematch assertions. No capability or ACL
    # is widened. See BPR-1694's native containment measurement record.
    "application_commit_filter_public_path": "owner-private-acl-denied",
    # [[BPR-1576]]. Panes waiting on a slow fixture network.
    "application_pending_states_public_path": "owner-private-acl-denied",
    # [[BPR-1575]]. Refresh made visible on a slow fixture network.
    "application_refresh_feedback_public_path": "owner-private-acl-denied",
    # [[BPR-1589]]. Status attention for the signed-in reviewer.
    "application_status_attention_public_path": "owner-private-acl-denied",
    # [[BPR-1601]]. Durable read state for PR comments and review summaries.
    "application_conversation_read_public_path": "owner-private-acl-denied",
    "merge_base_fixture_contract": "git-reference-parent-denied",
    # RFC-BPR-0089. Builds its fixture with the same tool chain as the
    # merge-base contract; the Windows station has not measured it yet.
    "commit_filter_fixture_contract": "git-reference-parent-denied",
    "sqlite_markdown_image_disk_cache": "ancestor-control-denied",
    # [[BPR-1573]]. Close review's composed course, registered uncontained like
    # application_review_navigation_entry, with its actual Qt-root refusal now
    # measured ([[BPR-1631]]).
    "application_close_review": "owner-private-acl-denied",
    # [[BPR-1355]]. The thread-words course of the same setup-destination
    # executable. Measured on JaegAW 2026-10-09 at 64e145116: passes
    # uncontained; contained through the sandbox launcher it fails "private
    # thread-words course root" before any course step, like its siblings.
    "application_thread_words_public_path": "dos-volume-name",
    # Measured on bpr-main-win 2026-10-08 at 7fc367064, each registered
    # contained for the measurement: overview_summary_public_path.exe stops at
    # "FAILED: private composed course root" (exit 3) in under 0.1 s at every
    # scale, the same refusal of the private storage root as the rows above.
    "application_overview_summary_1": "dos-volume-name",
    "application_overview_summary_1_25": "dos-volume-name",
    "application_overview_summary_2": "dos-volume-name",
    # [[BPR-1326]]. Measured on bpr-main-win 2026-10-08 at e3163bbbe, registered
    # contained for the measurement: the application refuses startup at the
    # registry ("STARTUP_FAILED_CAUSE: startup_failure=registry"), the same
    # refusal the BPR-0834 rows measured.
    "application_published_file_switch_contract": "dos-volume-name",
    # [[BPR-1478]]. The file-switch row's composed application over the same
    # fixture, with no data root of its own; pinned by that shape, not yet
    # measured contained on Windows.
    "application_refresh_pick_contract": "dos-volume-name",
    # [[BPR-1328]]. Measured the same day: change_arrows_public_path.exe stops at
    # "FAILED: private composed course root" (exit 3), as the overview summary
    # rows above do.
    "application_change_arrows_across_files": "dos-volume-name",
    # [[BPR-1337]], [[BPR-1339]]. Measured on bpr-main-win 2026-10-08 at
    # 5964dd6f1, registered contained for the measurement: the courses stop at
    # "FAILED: private discuss-top course root" and "FAILED: private composed
    # course root", the private storage root refused as above.
    "application_discuss_top_public_path": "dos-volume-name",
    "application_accepted_file_in_overview": "dos-volume-name",
    # [[BPR-1348]]. The same private composed course root and fixture
    # composition as application_accepted_file_in_overview. Measured on
    # JaegAW 2026-10-09 at 7672d5a8b: passes uncontained; contained through
    # the sandbox launcher it fails "private composed course root" before any
    # course step.
    "application_overview_double_click": "dos-volume-name",
    # Measured on bpr-main-win 2026-10-08 at 47f6d5882, each registered
    # contained for the measurement, every one refused its private storage
    # before any course step: comment_range_draft_store at "FAILED: the private
    # review store opens", the line-draft contract's application at startup
    # ("STARTUP_PROFILE_REFUSAL:registry_unavailable"), and the diff-zoom and
    # line-details courses at "FAILED: private composed course root".
    # [[BPR-1345]]. The line-draft contract's composed application over a
    # private data root, five courses. Measured on bpr-main-win 2026-10-09 at
    # 3947245ca in windows-dev-tour-cli Debug: contained through the sandbox
    # launcher, the driver cannot narrow its data root before any course
    # ("Failed to set permissions", bpr_narrow_owner_private.cmake:62 via
    # application_start_review_contract.cmake:43). Uncontained it passes in
    # Release; the Debug stall is BPR-1345's finding, not the container's.
    "application_start_review_contract": "dos-volume-name",
    # [[BPR-1374]]. The start-review contract's composed application over a
    # private data root, and the comment-range store test's private review
    # store; pinned by that shape, not yet measured contained on Windows.
    "application_reply_draft_contract": "dos-volume-name",
    "application_reply_draft_survival": "dos-volume-name",
    "application_comment_range_draft_store": "dos-volume-name",
    "application_line_draft_contract": "dos-volume-name",
    "application_diff_zoom_public_path": "dos-volume-name",
    "application_line_details_public_path": "dos-volume-name",
    "application_review_navigation_entry": "dos-volume-name",
    "application_header_source_navigation": "dos-volume-name",
    "application_source_find": "owner-private-acl-denied",
    "application_source_find_scale_1_25": "owner-private-acl-denied",
    "application_source_find_scale_2_0": "owner-private-acl-denied",
    "application_header_source_navigation_large": "dos-volume-name",
    "application_cpp_identifier_appearance": "owner-private-acl-denied",
    # [[BPR-1423]], pinned by [[BPR-1484]]. PR Search's courses compose the
    # shipped application over `e2e::private_data_root`, whose first step
    # application_overview_double_click was measured refusing contained
    # ("private composed course root"); pinned by that shape, not yet measured
    # contained on Windows. The retry row is registered only on UNIX.
    "application_source_pr_find": "dos-volume-name",
    "application_source_pr_find_scale_1_25": "dos-volume-name",
    "application_source_pr_find_scale_2_0": "dos-volume-name",
    "application_source_pr_search_scope": "dos-volume-name",
    "application_source_pr_search_retry": "dos-volume-name",
    "application_source_pr_search_performance": "dos-volume-name",
    # [[BPR-1327]], [[BPR-1340]]. Measured on bpr-main-win 2026-10-08 at
    # 9809b3786, registered contained for the measurement: the prepared-open
    # benchmark stops at "large-review-benchmark-failed: private data root
    # parent", and the Updates-line course's composed application refuses
    # startup at the registry ("STARTUP_PROFILE_REFUSAL:registry_unavailable").
    "application_large_review_prepared_open": "dos-volume-name",
    "application_epoch_update_line_public_path": "dos-volume-name",
    # BPR-0622: measured on JaegAW 2026-09-17. Contained, the canary course exits
    # 3 with no output at std::filesystem::canonical of its root argument, the
    # same lowbox refusal as the BPR-0806 rows; it passes uncontained.
    "agent_parent_death": "dos-volume-name",
    # BPR-0622: measured on JaegAW 2026-09-17. Contained, the journal course stops
    # in CPython realpath (Path.resolve(strict=True)) of its fixture executable with
    # "[WinError 5] Access is denied" before any course step; it passes uncontained.
    "agent_workspace_ownership": "dos-volume-name",
    # BPR-0622: measured on JaegAW 2026-09-17. Contained, the composed
    # application exits `application-startup-failed:failure=registry` before
    # the first checkpoint (its data-root canonicalisation); both pass uncontained.
    "application_agent_crash_recovery": "dos-volume-name",
    "application_agent_materialization_crash_recovery": "dos-volume-name",
    # BPR-0812: measured on JaegAW 2026-09-17, each passing uncontained.
    # Contained, std::filesystem::canonical of the test root (lease) or of the
    # Git executable (sparse cache) throws "Access is denied" and aborts.
    "sqlite_profile_runtime_lease": "dos-volume-name",
    # [[BPR-0979]]. The activation contract now binds a real lease over the same
    # canonical private root before creating its named pipe, and its public course
    # composes the same profile roots as the BPR-0806 rows. Measured on JaegAW
    # 2026-10-01 ([[BPR-1234]]): contained, the contract stops at `canonical: Access
    # is denied.` on its root, and the course's seed refuses the registry; both
    # pass uncontained.
    "sqlite_profile_activation": "dos-volume-name",
    "application_profile_activation_public_path": "dos-volume-name",
    # BPR-0813: measured on JaegAW 2026-09-17. Contained, the contract's scratch
    # root stops at `canonical: Access is denied.` of the temporary directory.
    "sqlite_private_store_open": "dos-volume-name",
    # BPR-0833: measured on JaegAW 2026-09-17. Contained, the scratch root stops at
    # `canonical: Access is denied.` before any course step; it passes uncontained.
    "sqlite_profile_storage_root": "dos-volume-name",
    # BPR-0845: measured on JaegAW 2026-09-17, both passing uncontained once the
    # course root is owner-private. Contained, open_state_directory stops at
    # validate_ancestor_chain with ERROR_ACCESS_DENIED, the retained ancestor
    # chain step described above.
    "sqlite_queue_enrollment_storage": "dos-volume-name",
    "sqlite_workbench_settings_storage": "dos-volume-name",
    # BPR-0833 (integrator decision: pin). Measured on JaegAW 2026-09-17, both passing
    # uncontained. Contained, the automated run's private profile is refused before
    # composition (AUTOMATED_PROFILE_PRIVATE_FAILED): make_storage_root_owner_private
    # cannot open the QTemporaryDir for WRITE_DAC (error 5), because Qt writes it with
    # an owner-only DACL that excludes the container. Creating the root with the
    # container SID instead (option (i), measured, not applied) then stops at the
    # stores' validate_ancestor_chain (error 5): the application step described above.
    "application_composition_capabilities_inside_boundary": "dos-volume-name",
    "application_composition_capabilities_product_run": "dos-volume-name",
    # BPR-0834: measured on JaegAW 2026-09-17, both passing uncontained once the
    # course root and its parent are owner-private. Contained, with both narrowed
    # to the user and the container, the composed application still refuses
    # startup with startup_failure::registry: the application step described
    # above, the same refusal the agent crash courses measured.
    "application_epoch_portable_occurrence_two_catalogues": "dos-volume-name",
    "application_epoch_ordinary_update_recreated": "dos-volume-name",
    "git_sparse_cache_contract": "dos-volume-name",
    # Contained, `git init` in the fixture root fails "unable to get current
    # working directory: Permission denied".
    "profile_window_restoration_fixture_contract": "dos-volume-name",
    # Contained, narrowing the composed course's own data root to the
    # owner-private shape fails before any course step
    # (`agent-*-composition-failure:private-test-root`).
    "application_agent_chat_anchored_continuation_contract": "dos-volume-name",
    "application_agent_finding_dif_context_contract": "dos-volume-name",
    "application_device_binding_restart": "dos-volume-name",
    "sqlite_epoch_catalog_storage_contract": "dos-volume-name",
    "sqlite_epoch_catalog_behavior_contract": "dos-volume-name",
    "sqlite_review_mutation_recovery_storage": "dos-volume-name",
    # Whole-review fixtures reuse open_state_directory and the retained ancestor
    # walk: the measured BPR-0362 DOS-volume cause, not new native evidence.
    "sqlite_review_submission_recovery_storage": "dos-volume-name",
    "live_review_submission_provider": "dos-volume-name",
    "live_review_submission_restart_recovery": "dos-volume-name",
    "sqlite_review_ledger_storage": "dos-volume-name",
    # Measured on JaegAW 2026-09-29, each passing uncontained. Contained, each
    # stops at its first private store open under its scratch root ("open cycle
    # catalog", "open without account, provider or agent", and exit 3 after
    # "each command client edit advances shared history"). Not traced further;
    # the same class as the BPR-0813 and [[BPR-1126]] scratch-root rows.
    "sqlite_review_cycle_storage": "dos-volume-name",
    "sqlite_review_cycle_runtime": "dos-volume-name",
    "sqlite_review_tour_storage": "dos-volume-name",
    # [[BPR-1230]]. Measured on JaegAW 2026-09-29: passes uncontained; contained
    # it stops at "publication SQLite journal opens" under its scratch root.
    "github_review_comment_permalink": "dos-volume-name",
    # [[BPR-0900]]. Measured on JaegAW 2026-10-01: it compares the descriptors
    # that uncontained courses narrow, so it runs where they do. Contained, the
    # lowbox token cannot walk the container temporary directory's ancestors,
    # so the product refuses every directory, including the product's own, and
    # the CMake-narrowed one, which carries no container grant, cannot even be
    # removed.
    "owner_private_descriptor_agreement": "dos-volume-name",
    # [[BPR-1240]]. Measured on JaegAW 2026-09-30: passes uncontained; contained it
    # exits 1 before printing a line. Not traced further; the same composed tour
    # course as application_review_tour_composition_public_path.
    "application_review_tour_writing": "dos-volume-name",
    # [[BPR-1301]]. Measured on bpr-main-win 2026-10-07 at e31a3f357, registered
    # contained for the measurement: like application_review_tour_writing, the
    # same composed tour course, it exits 1 before printing a line (0.09 s).
    # Registered uncontained, it passes.
    "application_review_tour_writing_wrapper": "dos-volume-name",
    # [[BPR-1148]], [[BPR-1149]]. The product opens a chosen root and resolves it
    # with GetFinalPathNameByHandleW(VOLUME_NAME_DOS), the rows keep their stores
    # under a scratch root, and they build fixtures with Git; each is the obstacle
    # named above. Measured on JaegAW 2026-09-29: all five pass uncontained, and
    # contained each fails before its first course completes (workspace_source at
    # "open root", review_cycle_capture at "fixture git command", the other three
    # at "catalog").
    "workspace_source": "dos-volume-name",
    "review_cycle_capture": "dos-volume-name",
    "review_cycle_comparison": "dos-volume-name",
    "review_cycle_scale": "dos-volume-name",
    "review_cycle_recovery": "dos-volume-name",
    # [[BPR-1153]], [[BPR-1155]], [[BPR-1157]], [[BPR-1158]], [[BPR-1159]], [[BPR-1160]], [[BPR-1162]]. The same scratch-root store and chosen-root course as the
    # rows above. Measured on JaegAW 2026-09-29: review_cycle_feedback,
    # review_cycle_suggestion, review_cycle_workbench and the desktop course pass
    # uncontained, and contained each fails before its course completes (the
    # first two at "catalog", the workbench at "opened without an account,
    # provider or agent", the desktop course at "Open Project offered signed
    # out"). review_cycle_round, review_cycle_export, review_cycle_bundle and
    # review_cycle_attach, measured the same day, pass uncontained and fail
    # contained with exit 3 (review_cycle_bundle at "catalog").
    "review_cycle_feedback": "dos-volume-name",
    "review_cycle_suggestion": "dos-volume-name",
    "review_cycle_round": "dos-volume-name",
    "review_cycle_export": "dos-volume-name",
    "review_cycle_bundle": "dos-volume-name",
    "review_cycle_attach": "dos-volume-name",
    # [[BPR-1164]], [[BPR-1165]]. The same scratch-root store course. Measured on
    # JaegAW 2026-09-29: both pass uncontained, and contained each exits 3 at
    # "catalog".
    "review_cycle_response": "dos-volume-name",
    "review_cycle_verification": "dos-volume-name",
    # [[BPR-1172]]. The same scratch-root store course, with the build's Git
    # both making fixtures and serving as the product's installed Git. Measured
    # on JaegAW 2026-09-29: review_cycle_outgoing, _candidate, _publication and
    # _publisher run uncontained, and contained each stops at `canonical: Access
    # is denied.` on the Git executable, the BPR-0812 shape.
    "review_cycle_outgoing": "dos-volume-name",
    # [[BPR-1173]]. The same course shape as review_cycle_outgoing.
    "review_cycle_candidate": "dos-volume-name",
    # [[BPR-1454]]. The Git metadata reader rows: the same scratch-root store
    # course with the build's Git making fixtures (and the publication catalog
    # for the outgoing row).
    "review_cycle_git_readers": "git-reference-parent-denied",
    # [[BPR-1457]]. The scale bench's Git reader and adversarial cases.
    "review_cycle_git_reader_scale": "git-reference-parent-denied",
    # [[BPR-1455]] and [[BPR-1456]]. The Git metadata differential harness and
    # canary course: the same scratch-root store course, with the build's Git
    # making fixtures and answering as the oracle.
    "git_metadata_harness": "git-reference-parent-denied",
    # [[BPR-1174]]. The same course shape.
    "review_cycle_publication": "dos-volume-name",
    # [[BPR-1175]]. The same course shape; the provider is simulated in memory.
    "review_cycle_publisher": "dos-volume-name",
    # [[BPR-1177]]. Measured on JaegAW 2026-10-04 at d8fa5bfc2, contained
    # through the sandbox launcher: it stops at "FAILED: fixture git command",
    # before it reaches review_cycle_publisher's canonical of git.exe. Git
    # starts there, and a probe in the same container shows `git init` under
    # AC\Temp exiting 128 with "fatal: Invalid path 'C:/Users': Permission
    # denied". It passes uncontained.
    "review_cycle_publish_desk": "dos-volume-name",
    # [[BPR-1178]]. The publish desk's course, run as several processes by a
    # Python driver. Measured on JaegAW 2026-10-04 at 91c01540e, contained
    # through the sandbox launcher: its first process (--publish) exits before
    # the first checkpoint with "FAILED: fixture git command", the
    # review_cycle_publish_desk refusal (Git under AC\Temp). It passes
    # uncontained.
    "review_cycle_publication_matrix": "dos-volume-name",
    "review_cycle_workbench": "dos-volume-name",
    "application_review_cycle_desktop_public_path": "dos-volume-name",
    "rfc0016_device_capture_security": "dos-volume-name",
    "test_network_isolation_structure_contract": "dos-volume-name",
    "operator_response_validation_contract": "dos-volume-name",
    "coding_standard_format_gate_contract": "batch-stub",
    "dependency_modernization_contract": "ninja-named-pipe",
    "sqlite_storage_root_contract": "dos-volume-name",
    "application_public_path": "dos-volume-name",
    "application_public_path_ghes_cli_unavailable": "dos-volume-name",
    "application_queue_composition_contract": "dos-volume-name",
    "application_native_notification_attended_contract": "dos-volume-name",
    "application_poller_liveness": "dos-volume-name",
    "application_native_appearance_probe_contract": "dos-volume-name",
    "application_github_public_path_github_com": "dos-volume-name",
    "application_github_public_path_uiforetw_74": "dos-volume-name",
    # [[BPR-1213]]. The same public path over the constructed nested-folders fixture.
    # Measured on JaegAW 2026-10-02: contained, startup refuses at the registry
    # (STARTUP_PROFILE_REFUSAL:registry_unavailable, its ancestor-chain walk);
    # uncontained it passes with PUBLIC_PATH_OK, GITHUB_FIXTURE_OK and RENDER_OK.
    "application_github_public_path_nested_folders_v1": "dos-volume-name",
    "application_uiforetw_selected_diff_contract": "dos-volume-name",
    "application_diff_workbench_public_path": "dos-volume-name",
    "application_workbench_composition_contract": "dos-volume-name",
    "application_review_ledger_composition_contract": "dos-volume-name",
    "application_review_round_trip_composition_contract": "dos-volume-name",
    "application_review_submission_composition_public_path": "dos-volume-name",
    "application_defect_composition_contract": "dos-volume-name",
    "application_overview_since_review_contract": "dos-volume-name",
    "application_review_behaviour_contract": "dos-volume-name",
    "application_saved_server_contract": "dos-volume-name",
    "application_threaded_reply_contract": "dos-volume-name",
    "application_appearance_theme_customization_contract": "dos-volume-name",
    "application_appearance_older_theme_contract": "owner-private-acl-denied",
    "application_fixture_verdict_contract": "dos-volume-name",
    "application_attended_launch_contract": "dos-volume-name",
    "application_diff_workbench_shutdown_public_path": "dos-volume-name",
    "application_automated_run_budget": "dos-volume-name",
    "application_github_public_path_ghes_3_18": "dos-volume-name",
    "application_device_authorization_public_path": "dos-volume-name",
    "application_device_authorization_unavailable": "dos-volume-name",
    "application_device_authorization_registered_course": "dos-volume-name",
    "application_device_authorization_browser_handoff": "dos-volume-name",
    "application_device_authorization_browser_absent": "dos-volume-name",
    "application_device_authorization_panel_course": "dos-volume-name",
    "application_device_authorization_panel_absent": "dos-volume-name",
    "application_device_authorization_ghes_refusal": "dos-volume-name",
    "application_device_authorization_refresh_course": "dos-volume-name",
    "application_device_authorization_orphan_course": "dos-volume-name",
    "application_mcp_context_helper": "dos-volume-name",
    "rfc0017_test_inventory": "dos-volume-name",
    "rfc0016_test_inventory": "dos-volume-name",
    # BPR-0317: the anonymous Git cache and catalog suites walk the same
    # data-root ancestor chain as their siblings above.
    "git_anonymous_public_read": "qprocess-named-pipe",
    "sqlite_anonymous_epoch_authority": "dos-volume-name",
    # BPR-0305: the public-preview composition rows walk the application's
    # data-root ancestor chain like the device-authorization rows above.
    "application_anonymous_public_preview_unavailable": "dos-volume-name",
    "application_anonymous_public_preview_release_contract": "dos-volume-name",
    "application_anonymous_public_preview_ghes_refusal": "dos-volume-name",
    "application_anonymous_public_preview_public_path": "dos-volume-name",
    "application_anonymous_public_preview_enabled_probe": "dos-volume-name",
    "application_anonymous_public_preview_live_candidate_wiring": "dos-volume-name",
    "github_device_authorization_refresh_contract": "dos-volume-name",
    "application_device_authorization_release_contract": "dos-volume-name",
    "application_epoch_maintenance_public_path": "dos-volume-name",
    "application_appearance_gesture_contract": "dos-volume-name",
    "application_appearance_store_fallback_contract": "dos-volume-name",
    "runtime_process_surface": "dos-volume-name",
    "application_render": "dos-volume-name",
    "gh_cli_transport": "dos-volume-name",
    "native_https_transport_fixture": "dos-volume-name",
    # BPR-0452's device-authorization loopback fixture, mirrored onto Windows
    # beside the transport fixture above and uncontained for the same reason.
    "native_https_device_authorization_fixture": "dos-volume-name",
    "epoch_history_fixture_contract": "dos-volume-name",
    # [[BPR-0880]]. Rebuilds the fsck-severity packs from their recipe and asks
    # the installed Git about every message identifier the adapter ships, so it
    # needs a Git child.
    "fsck_severity_fixture_contract": "child-process-denied",
    # [[BPR-0872]]. Eleven tooling rows measured uncontained on the Windows
    # station on 2026-09-17: each launches Git or another interpreter, and the
    # container refuses the child before the tool runs at all.
    "application_agent_finding_protocol": "child-process-denied",
    "attended_review_verdict_contract": "child-process-denied",
    "dif_assets_contract": "child-process-denied",
    "github_recorded_fixture_contract": "child-process-denied",
    "issue_id_allocation_contract": "child-process-denied",
    "large_review_fixture_contract": "child-process-denied",
    "project_source_dump_contract": "child-process-denied",
    "rfc0016_production_identity_security": "child-process-denied",
    "rfc0016_relocated_operator_contract": "child-process-denied",
    "rfc0017_public_preview_decision_contract": "child-process-denied",
    # [[BPR-0873]]. The documentation gate imports a module from outside the
    # checkout, and needs a Git child behind that.
    "documentation_structure": "repository-content-outside-checkout",
    "documentation_issue_graph_contract": "repository-content-outside-checkout",
    # [[BPR-1170]]. Measured on JaegAW 2026-10-05 at 9c717f15a in a tour-cli
    # tree with tests, as registered contained: the station's MASK deployment
    # `.claude/skills/bpr-cycle` is a link into the MASK store under the user
    # profile, which the container reads as an empty tree, so the copy check
    # fails at "differs from the package; import it again". Uncontained the
    # store's copy matches the package byte for byte and the row passes.
    "skill_bpr_cycle_package": "repository-content-outside-checkout",
    # [[BPR-1067]]. Measured on bpr-main-win 2026-10-06 at 1992ee1eb in a
    # tour-cli tree with tests, as registered contained, after `mask repair`
    # deployed `.claude/skills/pr-tour` and `.agents/skills/pr-tour` as links
    # into the MASK store: the copy check fails at ".claude/skills/pr-tour
    # differs from the package; import it again". Uncontained, the store's copy
    # has the package's own tree digest.
    "skill_pr_tour_package": "repository-content-outside-checkout",
    # [[BPR-1302]]. Measured on bpr-main-win 2026-10-07 at 399aade8e, as
    # registered contained: before the course starts,
    # `Path(arguments.binary).resolve(strict=True)` raises "PermissionError:
    # [WinError 5] Access is denied" on the built bpr.exe, CPython's realpath
    # in the container. Uncontained with the row's environment it prints
    # `tour-cli-loopback:never-signed-in-open:pass:0.1s` and
    # `tour-cli-loopback:pass`.
    "tour_cli_loopback_provider": "dos-volume-name",
    # [[BPR-1020]]. Measured on JaegAW 2026-09-23, each registered contained by
    # a reverted trial hook; the first contained failure is named. The release
    # scripts and the modified-endpoint fixture stop at the DOS form: Git starts
    # but cannot read its working directory ("unable to get current working
    # directory: Permission denied", probed directly), or CPython's realpath of
    # the package build directory raises [WinError 5].
    "package_runtime_contract": "dos-volume-name",
    "release_source_assets_contract": "dos-volume-name",
    # Measured on JaegAW 2026-10-10 with the native-release snapshot:
    # AppContainer reaches the test, then package.py's build.resolve(strict=True)
    # raises [WinError 5] on its own build-WIX/DEB/RPM scratch roots. All eleven
    # cases pass uncontained except the declared Bash-only publisher skip.
    "native_release_contract": "dos-volume-name",
    "modified_endpoint_fixture_contract": "dos-volume-name",
    # [[BPR-1108]]. The owner reverted BPR-1020's pins for
    # sqlite_release_maintenance_files, application_attended_recent_replay,
    # application_release_maintenance_public_path and
    # application_release_update_public_path on 2026-09-24. Those rows stay
    # unpinned, as BPR-0812 left its epoch rows, and this gate reports them on
    # Windows until each runs contained.
    #
    # Contained, each composed course stops at its first step, narrowing its
    # temporary root owner-private, as the BPR-0833 application rows do
    # ("native private root before composed startup", "private folder course
    # root").
    "application_logging": "dos-volume-name",
    # [[BPR-0885]]. Measured on JaegAW 2026-09-23 at 664d6e36: contained, the row
    # stops at its own setup, "test owner creates private root before logger
    # admission", where make_storage_root_owner_private narrows a QTemporaryDir
    # under the container's temporary directory. That is the helper and root
    # shape BPR-0833 measured refused; the refusing call here was not traced.
    # The same executable run uncontained passes every assertion, the Windows
    # log's properties among them.
    "quill_logging": "dos-volume-name",
    "application_selected_file_safety_public_path": "dos-volume-name",
    # [[BPR-1213]]. The same composed course executable, nested-folders mode.
    # Measured on JaegAW 2026-10-02: contained, private_data_root's
    # canonicalFilePath is refused ("private nested-folders course root");
    # uncontained the course passes.
    "application_nested_overview_folders_public_path": "dos-volume-name",
    # [[BPR-1031]]. Measured on JaegAW 2026-09-23: contained, the course stops
    # at its first step, "disposable private state". A reverted probe showed the
    # WRITE_DAC open of its QTemporaryDir under AC\Temp refused with error 5,
    # the refusal BPR-0833 measured for the same narrowing. Uncontained it
    # passes.
    "release_update_recovery": "dos-volume-name",
    # [[BPR-0876]]. Measured on JaegAW 2026-09-23 at 470920d5, contained as
    # registered and uncontained through a reverted trial keyword. Contained,
    # the renewal course stops at "private desktop test root under the
    # canonical storage policy", and the restart course's seeding executable
    # exits 3, the contained canonical abort BPR-0833 and BPR-0845 recorded.
    # Uncontained both pass (32.31 s and 16.48 s).
    "application_device_binding_launch_renewal": "dos-volume-name",
    "application_profile_window_restart_public_path": "dos-volume-name",
    # [[BPR-0876]]. The same measurement for three more courses, each of which
    # had a second obstacle behind the container. Contained, the relaunch
    # seed stops at SEED_STARTUP_PROFILE_REFUSAL:registry_unavailable, and the
    # layout and review-restoration seeds exit 3. Uncontained, relaunch and
    # layout pass once their course closes its own store connections
    # ([[BPR-1039]]), and review restoration passes once its copied roots carry
    # the owner-private descriptor its stores require.
    "application_device_binding_relaunch": "dos-volume-name",
    "application_profile_window_layout_public_path": "dos-volume-name",
    "application_profile_window_review_restoration_public_path": "dos-volume-name",
    # [[BPR-0907]], [[BPR-1235]]. Measured on JaegAW 2026-10-01. Contained, the seed
    # refuses the registry (SEED_REGISTRY_REFUSAL:0): the store walks the data
    # root's retained ancestor chain, which asks for the DOS form above the
    # container's AC\Temp. Uncontained, the whole course passes once its
    # Windows-only branches, stale since [[BPR-0979]], are gone.
    "application_profile_owner_admission_public_path": "dos-volume-name",
    # [[BPR-0874]]. Measured on JaegAW 2026-09-23: with the data root in a
    # private temporary parent, as on POSIX, the three large-review courses
    # compose uncontained. Contained they stop at their first step, "private
    # data root parent": narrowing the QTemporaryDir needs WRITE_DAC, the
    # refusal BPR-0833 measured.
    "application_large_review_benchmark": "dos-volume-name",
    "application_unavailable_diff_public_path": "dos-volume-name",
    "application_approximate_diff_public_path": "dos-volume-name",
    "epoch_maintenance_integration": "dos-volume-name",
    # BPR-0846: the same real host, cache and catalog composition as
    # epoch_maintenance_integration, not yet measured contained on Windows.
    "git_haves_trimmed_pack_update": "dos-volume-name",
    "git_haves_trimmed_pack_missing-object": "dos-volume-name",
    "git_haves_trimmed_pack_unconnected": "dos-volume-name",
    "epoch_kill_recovery": "dos-volume-name",
    "epoch_transaction_kill_recovery": "dos-volume-name",
    "git_process_contract": "qprocess-named-pipe",
    "git_cache_contract": "qprocess-named-pipe",
    "live_review_mutation_restart_recovery": "dos-volume-name",
    "application_agent_invocation_composition_contract": "dos-volume-name",
    "application_agent_invocation_construction_cut_contract": "dos-volume-name",
    "application_agent_invocation_launcher_construction_cut_contract": "dos-volume-name",
    "application_agent_invocation_gesture_cut_contract": "dos-volume-name",
    "application_agent_invocation_cancellation_contract": "dos-volume-name",
    # BPR-0514 and BPR-0503's chat and finding composition courses, generated
    # by foreach in tests/end_to_end/CMakeLists.txt and uncontained for the
    # same data-root reason as the invocation courses beside them.
    "application_agent_chat_composition_contract": "dos-volume-name",
    "application_agent_chat_construction_cut_contract": "dos-volume-name",
    "application_agent_chat_continued_contract": "dos-volume-name",
    "application_agent_chat_stale_contract": "dos-volume-name",
    "application_agent_finding_chat_panel_construction_cut_contract": "dos-volume-name",
    "application_agent_finding_composition_contract": "dos-volume-name",
    "application_agent_finding_workshop_press_contract": "dos-volume-name",
    # [[BPR-1091]]. The review-feedback guidance courses, the same composed
    # course and data-root obstacle as the workshop press beside them.
    "application_agent_finding_workshop_guidance_default_contract": "dos-volume-name",
    "application_agent_finding_workshop_guidance_custom_contract": "dos-volume-name",
    "application_agent_finding_workshop_guidance_no_agent_contract": "dos-volume-name",
    # [[BPR-1093]]. The injection course of the same composition. Measured on
    # JaegAW 2026-10-09 at 64e145116: passes uncontained; contained through the
    # sandbox launcher it stops at `agent-finding-composition-failure:private-test-root`,
    # like its siblings above.
    "application_agent_finding_workshop_injection_contract": "dos-volume-name",
    "application_agent_finding_finding_host_construction_cut_contract": "dos-volume-name",
    "application_agent_finding_full_agent_contract": "dos-volume-name",
    "application_agent_finding_mcp_construction_cut_contract": "dos-volume-name",
    "application_agent_finding_queue_construction_cut_contract": "dos-volume-name",
    "application_agent_finding_workshop_construction_cut_contract": "dos-volume-name",
    "application_agent_invocation_staleness_contract": "dos-volume-name",
    "application_agent_invocation_cleanup_retry_contract": "dos-volume-name",
    "application_agent_invocation_workspace_diagnostic_contract": "dos-volume-name",
    "application_epoch_persistence_github-com": "dos-volume-name",
    "application_epoch_persistence_ghes-3_18": "dos-volume-name",
    "application_composition_contract": "ninja-named-pipe",
    "windows_credential_manager_native_setup": "credential-manager",
    "windows_credential_manager_native": "credential-manager",
    "windows_credential_manager_native_cleanup": "credential-manager",
    "github_cli_process_runner": "qprocess-named-pipe",
    # [[BPR-1072]]. Starts the launcher four at a time to prove its profile step
    # holds under concurrency. Registered contained on JaegAW 2026-09-24, every
    # launch stopped at firewall-service:win32=5.
    "test_network_isolation_concurrent_launch": "service-manager-denied",
    # [[BPR-1237]], [[BPR-0967]]. Measured on JaegAW 2026-10-02 at dbe6966ea: each row
    # below run through the sandbox launcher with its own registered environment,
    # and as registered. Every one fails contained at the step its comment
    # names and passes uncontained. The steps are the private data root's
    # canonicalisation or owner-private narrowing, the profile registry's
    # ancestor-chain walk, or a store opened under such a root: the drive-letter
    # translation and WRITE_DAC refusal this file files under dos-volume-name
    # (BPR-0833, BPR-0874, BPR-1031).
    # [[BPR-0967]]. The large-review courses registered uncontained by cef11b982:
    # contained, each stops at "large-review-benchmark-failed: private data root parent".
    "application_large_review_active_work": "dos-volume-name",
    "application_large_review_cache_bytes": "dos-volume-name",
    "application_large_review_cache_entries": "dos-volume-name",
    "application_large_review_history": "dos-volume-name",
    "application_large_review_launch": "dos-volume-name",
    "application_large_review_loaded_close": "dos-volume-name",
    "application_large_review_pathological": "dos-volume-name",
    "application_large_review_provider_cancel": "dos-volume-name",
    "application_large_review_resources": "dos-volume-name",
    "application_large_review_responsiveness": "dos-volume-name",
    # [[BPR-1237]]. The same benchmark's other courses stop at the same step.
    "application_large_review_timing_control": "dos-volume-name",
    "application_first_tab_comparison": "dos-volume-name",
    # [[BPR-1237]]. Composed courses that stop at e2e::private_data_root(), the
    # QTemporaryDir's canonicalFilePath: the tour courses exit there silently, the
    # others say "private data root" ("private data root exists" for the drain).
    "application_review_tour_cold_import_public_path": "dos-volume-name",
    "application_review_tour_composition_public_path": "dos-volume-name",
    "application_review_tour_composition_scale_125": "dos-volume-name",
    "application_review_tour_composition_scale_200": "dos-volume-name",
    "application_review_tour_publication_compat": "dos-volume-name",
    "application_review_tour_publication_conflict": "dos-volume-name",
    "application_review_tour_publication_description": "dos-volume-name",
    "application_review_tour_publication_maintenance": "dos-volume-name",
    "application_review_tour_publication_manual": "dos-volume-name",
    "application_review_tour_publication_native": "dos-volume-name",
    "application_review_tour_publication_reconciliation": "dos-volume-name",
    "application_review_tour_publication_stop": "dos-volume-name",
    "application_review_tour_publication_unsupported": "dos-volume-name",
    "application_diagnostic_controls_public_path": "dos-volume-name",
    "application_review_submission_composition_scale_125": "dos-volume-name",
    "application_review_submission_composition_scale_200": "dos-volume-name",
    "application_profile_window_layout_drain": "dos-volume-name",
    # [[BPR-1404]]. Measured on JaegAW 2026-10-12 at 004a9c8ef. Contained,
    # the real SQLite private store stops at "the course makes its data root
    # owner-private" in 0.17 s. Its retained ancestor chain resolves DOS volume
    # names. The identical binary with the configured environment passes
    # uncontained in 0.341 s, without storage-policy or ACL changes.
    "application_private_draft_restore": "dos-volume-name",
    # [[BPR-1237]]. Courses whose own private root cannot be narrowed contained:
    # "owner-private root", "wire fixture root is owner-private", "owner-private data
    # root", "private application root", "private fixture state root", "native private
    # root before composed startup" (the four logging courses), "private test root",
    # "test owner creates private root before logger admission", and the crash
    # recovery's session_retention child, which stops at its private test root
    # before it reaches "flushed".
    "application_active_changed_scaling": "dos-volume-name",
    "application_attended_recent_replay": "dos-volume-name",
    "application_rate_limit_render_recovery": "dos-volume-name",
    "application_release_maintenance_public_path": "dos-volume-name",
    "application_release_update_public_path": "dos-volume-name",
    "application_logging_cancel_startup": "dos-volume-name",
    "application_logging_capability-failure": "dos-volume-name",
    "application_logging_full": "dos-volume-name",
    "application_logging_unavailable": "dos-volume-name",
    "logging_session_retention": "dos-volume-name",
    "quill_logging_content_policy": "dos-volume-name",
    "logging_crash_recovery": "dos-volume-name",
    # [[BPR-1237]]. QTemporaryDir roots under AC\Temp that the course then writes in:
    # "create_directory: Access is denied" inside qt_temp-*, the refusal BPR-1031
    # measured for the same root.
    "diagnostic_context_age": "dos-volume-name",
    "diagnostic_controls": "dos-volume-name",
    # [[BPR-1237]]. "canonical: Access is denied" on the course's AC\Temp root.
    "git_haves_trimmed_pack_tour-source": "dos-volume-name",
    "sqlite_release_maintenance_files": "dos-volume-name",
    # [[BPR-1237]]. Startup refuses at the profile registry
    # (STARTUP_PROFILE_REFUSAL:registry_unavailable, the private-review seed),
    # "desktop admitted" (the seed's desktop, refused the same way), or the
    # auth seed's exit 3, this station's contained abort for canonical.
    "application_private_review_file": "dos-volume-name",
    "application_private_review_native": "dos-volume-name",
    "application_private_review_retry": "dos-volume-name",
    "application_offline_inventory": "dos-volume-name",
    "application_recent_account_isolation": "dos-volume-name",
    "application_document_tabs_restoration_public_path": "dos-volume-name",
    # [[BPR-1237]]. A store opened under such a root is refused: "production staged
    # journal opens", "accepted overview saves", "public factory opens migrated
    # ledger", and the private-review host's store, an unchecked std::expected.
    "staged_review_session": "dos-volume-name",
    "staged_review_restart": "dos-volume-name",
    "sqlite_cached_review_overview": "dos-volume-name",
    "sqlite_private_review_storage": "dos-volume-name",
    "application_private_review_host": "dos-volume-name",
    # [[BPR-1082]], [[BPR-1083]]. Measured on JaegAW 2026-10-05 at fcd0aebea in
    # windows-dev, contained through the sandbox launcher. Each passes
    # uncontained. The draft store's fixture stops at its first private-review
    # store (feedback_draft_store_tests.cpp:51), an unchecked std::expected
    # ("operator*() called on a std::expected that contains an error"); the
    # label authoring application at "STARTUP_PROFILE_REFUSAL:registry_unavailable".
    # [[BPR-1229]]. Measured on JaegAW 2026-10-05 at f8a29f167 in windows-dev,
    # contained as qt_agent_launch_canary then registered: the durable course's
    # private-review store under its scratch root is refused, "FAILED: open
    # durable child course" (agent_launch_canary_tests.cpp:281). The course is
    # its own row so the canary's launch courses stay contained.
    "qt_agent_launch_canary_durable_course": "dos-volume-name",
    "application_feedback_draft_store": "dos-volume-name",
    "application_label_authoring_contract": "dos-volume-name",
    # [[BPR-1093]]. The label authoring course's composition and private store.
    # Measured on JaegAW 2026-10-09 at 64e145116: passes uncontained; contained
    # through the sandbox launcher it fails "private desktop test root under the
    # canonical storage policy" before any course step.
    "application_label_publication_recovery": "dos-volume-name",
    # [[BPR-1167]], [[BPR-1168]]. Measured on JaegAW 2026-10-02 at 3e3b3fa17 in a
    # BPR_BUILD_TOUR_CLI tree, contained through the sandbox launcher and as
    # registered. Each passes uncontained. Contained, the tour CLI rows and
    # `bpr cycle` stop at "profile_unavailable: The application root is
    # unavailable or is not owner-private" ("public open" in the publication
    # course); the tour handoff's desktop at "owner startup succeeds"; the tour
    # owner channel at "canonical: Access is denied" on its AC\Temp root, before
    # its named pipe. These rows exist only where BPR_BUILD_TOUR_CLI is on.
    "tour_cli_publication": "dos-volume-name",
    "tour_cli_composition": "dos-volume-name",
    "tour_cli_process_commands": "dos-volume-name",
    # [[BPR-1092]]. The tour CLI rows' profile root. Measured on JaegAW
    # 2026-10-09 at 64e145116 in the tour-cli tree: passes uncontained;
    # contained through the sandbox launcher its first `tour open` exits 7 with
    # profile_unavailable, "The application root is unavailable or is not
    # owner-private.", as tour_cli_composition does.
    "tour_cli_labels": "dos-volume-name",
    "cycle_cli_public_path": "dos-volume-name",
    # [[BPR-1185]]. Measured on JaegAW 2026-10-05 at 9c717f15a in a tour-cli
    # tree with tests, contained through the sandbox launcher: its first
    # `bpr cycle open` (prepare, line 331) answers "profile_unavailable: The
    # application root is unavailable or is not owner-private", as
    # cycle_cli_public_path does. It passes uncontained.
    "cycle_cli_documentation": "dos-volume-name",
    "application_profile_cycle_handoff_public_path": "dos-volume-name",
    "application_profile_tour_handoff_public_path": "dos-volume-name",
    "sqlite_tour_owner_channel": "dos-volume-name",
    # [[BPR-1166]]. Measured on JaegAW 2026-10-03 at 544bd5ce0 in a
    # BPR_BUILD_TOUR_CLI tree, contained as registered. The course's first
    # `git init` in its AC\Temp project exits 128 with "fatal: unable to get
    # current working directory: Permission denied" (Git's getcwd) before any
    # desktop step. It passes uncontained.
    "application_review_cycle_two_round_public_path": "dos-volume-name",
    # [[BPR-1171]]. Measured on JaegAW 2026-10-04 at 8fab05419 in a
    # BPR_BUILD_TOUR_CLI tree, contained as registered. The course's first
    # `bpr cycle list` exits 7 with "The application root is unavailable or
    # is not owner-private.", the profile_unavailable refusal
    # cycle_cli_public_path measures. It passes uncontained.
    "application_review_cycle_parity_public_path": "dos-volume-name",
    # [[BPR-1177]]. Measured on JaegAW 2026-10-04 at d8fa5bfc2, contained as
    # registered. It exits 3 with no output in 0.08 s: the course's unchecked
    # std::filesystem::canonical of its scratch root aborts, the BPR-0622
    # agent_parent_death shape. It passes uncontained.
    "application_review_cycle_publish_public_path": "dos-volume-name",
    # [[BPR-1184]]. Measured on JaegAW 2026-10-04 at baef515d8 in the
    # windows-app-release tree with BUILD_TESTING and BPR_BUILD_TOUR_CLI on,
    # contained as registered. After generating its first corpus, the first
    # desktop exits 1 with "FAILED: the desktop starts with a window" (the
    # profile desktop's data root, as the application rows above), before
    # the expected `bpr cycle open`. It is measured uncontained.
    "application_review_cycle_large_project": "dos-volume-name",
    # [[BPR-1179]]. Registered only where BPR_BUILD_LOCAL_AUTOMATION is off.
    # Measured on JaegAW 2026-10-04 at 795d387cf in windows-dev with both
    # switches OFF, contained through the sandbox launcher: the course's
    # first `git init` exits 128 before any desktop step, as the two-round
    # course it shares does (Git's getcwd under AC\Temp). It passes
    # uncontained.
    "application_review_cycle_automation_off_two_round": "dos-volume-name",
    # [[BPR-1180]]. Measured on JaegAW 2026-10-04 at f198d4e09 in windows-dev,
    # contained through the sandbox launcher; each passes uncontained.
    # review_cycle_send exits 3 at its first store step ("FAILED: two
    # notes"), as the other review_cycle_* store rows do. The send course
    # exits 3 at "FAILED: the opened project is the selected cycle": opening
    # the project is refused contained (it had crashed on the unchecked
    # optional before the course checked it).
    "review_cycle_send": "dos-volume-name",
    "application_review_cycle_send_public_path": "dos-volume-name",
    # [[BPR-1181]]. Measured on JaegAW 2026-10-04 at 72ca49f18 in a
    # BPR_BUILD_TOUR_CLI tree, contained as registered: the course's first
    # `git init` (review_cycle_security_qualification.py, line 214) fails
    # before any attack runs, Git's getcwd under AC\Temp as in the two-round
    # course. It is run uncontained.
    "application_review_cycle_security_qualification": "dos-volume-name",
    # [[BPR-1183]]. Measured on JaegAW 2026-10-05 at 1fb3cd77d in a
    # BPR_BUILD_TOUR_CLI tree, contained as registered: the corpus's first
    # `bpr cycle open` exits 7 with "The application root is unavailable or
    # is not owner-private.", the profile_unavailable refusal
    # cycle_cli_public_path measures. It is run uncontained.
    "application_review_cycle_recovery_corpus": "dos-volume-name",
    # [[BPR-1331]]. The published file switch row's fixture and composed
    # application. Measured on JaegAW 2026-10-09 at 64e145116: passes
    # uncontained; contained through the sandbox launcher its CMake driver
    # cannot narrow its own data root ("Failed to set permissions",
    # bpr_narrow_owner_private.cmake:16), before the application starts.
    "application_review_progress_push_contract": "dos-volume-name",
    # [[BPR-1558]]. The syntax delivery course composes the shipped
    # application over `e2e::private_data_root` exactly as
    # application_header_source_navigation does; pinned by that shape, not yet
    # measured contained on Windows.
    "application_syntax_delivery": "dos-volume-name",
}
# BPR-0573 integration audit at 0ff88046 listed source registrations with no
# measured Windows exception, counted only as pending source inventory so they
# could not break another platform's contained manifest, and never allowed by
# configured_failures on Windows. [[BPR-1237]] measured every one on JaegAW on
# 2026-10-02 and pinned it above, or moved it into the container, so none is
# pending. A registration added here grants no executable or network authority.
WINDOWS_PENDING_REGISTRATIONS: frozenset[tuple[str, str]] = frozenset()

# BPR-0802. The macOS rows that cannot run under the sandbox runner, because
# they start a sandbox of their own and macOS refuses nested sandbox
# initialization. Each is registered by the one raw `add_test` its source file
# is allowed, runs its executable directly with no arguments, and still
# carries the seam's boundary marker and pre-boundary scrubs.
#
# Standing outside the sandbox is safe only while the row cannot reach the
# application: no `bpr::app::` symbol means no `github_application`, so no
# system browser opener and no GitHub CLI transport factory. That is asserted
# against the built executable on every run, not assumed from the link line.
#
# The set is exact: a row outside it that skips the runner fails as before, a
# member found under the runner is a stale pin, and the exception exists on no
# other backend.
@dataclass(frozen=True)
class MacosUnsandboxedTest:
    source: str
    target: str
    executable: str
    cause: str


MACOS_UNSANDBOXED_TESTS: dict[str, MacosUnsandboxedTest] = {
    # Every `gh` request the adapter compiles runs through /usr/bin/sandbox-exec
    # (`constrain_gh_writes`), the production write containment this course
    # exercises against a fake `gh`. Under the runner the first launch fails
    # and the course stops at "fixture sandbox version launch"; run directly
    # it passes. Measured on macOS 2026-09-17 (BPR-0802).
    "gh_cli_git_smart_transport": MacosUnsandboxedTest(
        source="tests/github_contract/CMakeLists.txt",
        target="test_better_pull_review_gh_cli_git_smart_transport_tests",
        executable="better_pull_review_gh_cli_git_smart_transport_tests",
        cause="nested-sandbox",
    ),
}
MACOS_UNSANDBOXED_LABEL = "bpr_local_only_fixture"
MACOS_NM = pathlib.Path("/usr/bin/nm")
COMPOSITION_SYMBOL_NAMESPACE = "bpr::app::"
EXIT_VERDICT_TESTS = {
    LIVE_TEST_NAME,
    "architecture_boundaries",
    "architecture_boundary_contract",
    "designated_live_mutation_gate",
    "designated_live_mutation_runner_digest_mismatch",
    "test_network_isolation_structure_contract",
    "test_network_isolation_manifest",
    "test_network_isolation_external",
    "test_network_isolation_loopback",
    "test_network_isolation_unrelated",
    "test_network_isolation_child",
    "test_network_isolation_application",
    "live_mutation_isolation",
    "live_mutation_isolation_contract",
}
FORBIDDEN_EXIT_VERDICT_PROPERTIES = {
    "PASS_REGULAR_EXPRESSION",
    "FAIL_REGULAR_EXPRESSION",
    "SKIP_REGULAR_EXPRESSION",
    "WILL_FAIL",
    "SKIP_RETURN_CODE",
    "DISABLED",
}
# BPR-0552 and BPR-0890. The exact error expressions
# `bpr_add_network_isolated_test` writes on every ordinary row, in the order it
# writes them. `PUBLIC_PATH_FAILED` is the run's own verdict.
# `TEST_RUNNER_CHILD_EXIT_FAILED` is the launcher's statement that the child
# exited non-zero, which is the only verdict left once a row carries a
# `PASS_REGULAR_EXPRESSION`: CTest computes success as `retVal == 0 ||
# !RequiredRegularExpressions.empty()`, so on such a row the exit status is
# ignored outright and an abort after the marker was printed passed.
#
# Held here as a literal rather than parsed out of the CMake that sets it. The
# CMake is the claim; this is the independent record, and it is checked against
# CTest's own manifest because that is where a later row-level
# `set_tests_properties(<row> PROPERTIES FAIL_REGULAR_EXPRESSION ...)` would
# show up. That statement replaces the whole property, runs after the seam has
# returned, and cannot be defended against from inside the seam by any amount
# of APPEND; until this record existed nothing noticed it at all.
RUN_VERDICT_EXPRESSION = "PUBLIC_PATH_FAILED"
CHILD_EXIT_EXPRESSION = "TEST_RUNNER_CHILD_EXIT_FAILED"
# BPR-1050. The application's own line when startup recovery refuses the agent
# workspace neighbourhood's parent, which is what a course rooted directly in a
# bare temporary directory prints on Linux and Windows. It is independent of
# the verdict entries above: a row exempt from those still fails on it, and
# only a row whose subject is the refusal drops it.
WORKSPACE_REFUSAL_EXPRESSION = "AGENT_WORKSPACE_RECOVERY_REFUSED"
INHERITED_FAILURE_EXPRESSIONS = [
    RUN_VERDICT_EXPRESSION,
    CHILD_EXIT_EXPRESSION,
    WORKSPACE_REFUSAL_EXPRESSION,
]
EXPECTS_FAILED_RUN_LABEL = "bpr_expects_failed_run"
EXPECTS_NONZERO_EXIT_LABEL = "bpr_expects_nonzero_exit"
# The narrower exemption: a row whose child exits non-zero because that is the
# answer it asserts, which is not a reason to stop reading the run's own
# verdict. So it keeps `PUBLIC_PATH_FAILED` and drops only the exit entry.
#
# `rfc0017_release_absence_negative_control` runs the shipped-artifact scan
# against the tests-enabled binary, where it must fail naming the test-seam
# factory; the row pins that text with a `PASS_REGULAR_EXPRESSION` and the
# scanner exits 1. Measured green with a child exit of 1 under BPR-0890, which
# is the same green an abort in a shutdown tail produced.
EXPECTS_NONZERO_EXIT_TESTS = {"rfc0017_release_absence_negative_control"}
EXPECTS_NONZERO_EXIT_EXPRESSIONS = [RUN_VERDICT_EXPRESSION, WORKSPACE_REFUSAL_EXPRESSION]
# The rows whose subject is a failed run, so the rows that inherit neither
# verdict entry: the marker they exist to observe must not fail them, and the
# non-zero exit they end on is the thing being asserted rather than a fault.
# Exact in one direction only. A row that takes the exemption without being
# written down here fails the gate; a name here that is not configured does
# not, because a row registered on one platform must not turn the gate red on
# the others.
EXPECTS_FAILED_RUN_TESTS = {"application_automated_run_budget"}
EXPECTS_FAILED_RUN_EXPRESSIONS = [WORKSPACE_REFUSAL_EXPRESSION]
# BPR-1050. The rows whose subject is the workspace recovery refusal itself, so
# the rows that drop `WORKSPACE_REFUSAL_EXPRESSION` and keep everything else.
# `application_profile_management_public_path`'s `shared-parent` section builds
# a group-writable parent on purpose and asserts that recovery refuses it
# (BPR-0817). The section is compiled out on Windows, so the row takes the
# exemption only where the section runs. Exact in one direction only, for that
# reason: a recorded row configured without the label is merely held to the
# stricter rule.
EXPECTS_WORKSPACE_RECOVERY_REFUSAL_LABEL = "bpr_expects_workspace_recovery_refusal"
EXPECTS_WORKSPACE_RECOVERY_REFUSAL_TESTS = {
    "application_profile_management_public_path",
}
REQUIRED_COMMAND_BOUND_TESTS = {
    "architecture_boundaries",
    "architecture_boundary_contract",
}
# The recorded authorization for binding loopback, and how many ports each test
# may take. A test absent from this table is expected to take zero, so the gate
# fails closed on any loopback authority nobody wrote down. Do not derive this
# from the CMake declaration: the declaration is the claim and this is the
# independent record, and a table generated from the thing it checks checks
# nothing.
MACOS_LOOPBACK_AUTHORITIES = {
    "test_network_isolation_loopback": 1,
    "test_network_isolation_unrelated": 1,
    "designated_live_public_observer_fixture": 1,
    "native_https_transport_fixture": 2,
    "native_git_smart_transport_fixture": 1,
    "rfc0017_credential_free_native_https": 2,
    # BPR-0301. The anonymous-read wire suite runs the same
    # verify_native_https_transport.py course as the fixture above: one
    # loopback CONNECT proxy and one loopback TLS origin, nothing outbound.
    "native_https_anonymous_read_fixture": 2,
    # BPR-0452. Both drive verify_native_https_device_authorization.py, which
    # binds one listener at 127.0.0.1, refuses a non-loopback peer, and issues
    # no outbound request; its only github.com string is a verification_uri
    # value inside a response body the fixture itself serves.
    "native_https_device_authorization_fixture": 1,
    "github_device_authorization_refresh_contract": 1,
    # RFC-BPR-0084. verify_native_https_images.py binds its one configured
    # port at 127.0.0.1 as the CONNECT proxy and TLS origin for its fixture
    # image authorities, refuses any other authority at CONNECT, and issues no
    # outbound request; the camo, github.com and S3 hosts are names inside the
    # tunnel only.
    "native_https_image_fixture": 1,
    # BPR-0947, recorded by [[BPR-0854]]. verify_release_update_transport.py
    # binds its one configured port at 127.0.0.1 and is both the CONNECT proxy
    # and the TLS origin, under a fixture authority it generates. The executable
    # is handed that port and connects nowhere else. Nothing leaves the host:
    # the api.github.com and release-assets hosts are only names inside the
    # tunnel.
    "release_update_native_https": 1,
    # [[BPR-1302]]. tour_cli_loopback_tests.py binds its one configured port at
    # 127.0.0.1 as the CONNECT proxy and TLS origin under a fixture authority it
    # generates, refuses a non-loopback peer, and hands the built `bpr` that
    # port through the test-only seam. github.com and api.github.com are only
    # names inside the tunnel.
    "tour_cli_loopback_provider": 1,
}
MACOS_UNRELATED_ENDPOINT_PROBE = "test_network_isolation_unrelated"
PRE_BOUNDARY_INJECTION_VARIABLES = (
    "PYTHONPATH",
    "PYTHONHOME",
    "PYTHONUSERBASE",
    "PYTHONINSPECT",
    "PYTHONSTARTUP",
    "DYLD_INSERT_LIBRARIES",
    "DYLD_LIBRARY_PATH",
    "DYLD_FRAMEWORK_PATH",
    "DYLD_VERSIONED_LIBRARY_PATH",
    "DYLD_VERSIONED_FRAMEWORK_PATH",
    "DYLD_FALLBACK_LIBRARY_PATH",
    "DYLD_FALLBACK_FRAMEWORK_PATH",
    "DYLD_IMAGE_SUFFIX",
    "DYLD_ROOT_PATH",
    "DYLD_SHARED_REGION",
    "LD_PRELOAD",
    "LD_LIBRARY_PATH",
    "LD_AUDIT",
    "LD_DEBUG",
    "LD_PROFILE",
)
LIVE_CREDENTIAL_VARIABLES = (
    "GH_TOKEN",
    "GITHUB_TOKEN",
    "GH_ENTERPRISE_TOKEN",
    "GITHUB_ENTERPRISE_TOKEN",
)
# BPR-0797. The run-time answer to "am I inside the automated test boundary?".
# The composition in `src/app` reads it before it may bind the real system
# browser opener and the real GitHub CLI transport, so a row whose child does
# not see it is a row allowed to open a browser on whoever runs the suite.
# `bpr_add_network_isolated_test` sets it for every row it registers; this
# reads it back out of CTest's own manifest rather than out of the CMake
# source, because the source says what was asked for and the manifest says
# what the row will actually run with.
#
# The marker lives in ENVIRONMENT_MODIFICATION, not ENVIRONMENT, and the check
# follows it there. `set_tests_properties(<row> PROPERTIES ENVIRONMENT ...)`
# replaces ENVIRONMENT outright after the seam returns, which on this tree
# would strip an ENVIRONMENT marker from the hundred-odd rows that go on to
# set their own `QT_QPA_PLATFORM=offscreen` - among them every
# `application_device_authorization_*` course. CTest applies
# ENVIRONMENT_MODIFICATION after ENVIRONMENT, so no row-level write reaches it.
AUTOMATED_TEST_BOUNDARY_VARIABLE = "BPR_AUTOMATED_TEST_BOUNDARY"
AUTOMATED_TEST_BOUNDARY_MODIFICATION = f"{AUTOMATED_TEST_BOUNDARY_VARIABLE}=set:1"
# The one row that stands outside the boundary on purpose. It composes the
# application the way a person's own build does - real opener, real GitHub CLI
# offer - and asserts that identity without beginning a flow, against a closed
# loopback port. It carries the seam's `set:1` like every other row and then
# erases it with its own `unset:`, in that order, so the manifest shows both
# the rule and the one named exception rather than a row the seam missed.
AUTOMATED_TEST_BOUNDARY_PRODUCT_RUN_TEST = (
    "application_composition_capabilities_product_run"
)
AUTOMATED_TEST_BOUNDARY_ERASURE = f"{AUTOMATED_TEST_BOUNDARY_VARIABLE}=unset:"

STAGE_ZERO_PRELOADER_SOURCE = """import hashlib
import os
import stat
import sys

path = sys.argv[1]
expected = sys.argv[2]
descriptor = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
try:
    before = os.fstat(descriptor)
    if not stat.S_ISREG(before.st_mode) or before.st_size <= 0 or before.st_size > 1048576:
        raise SystemExit("designated-live-stage-zero: launcher is not a bounded regular file")
    source = bytearray()
    while len(source) <= 1048576:
        block = os.read(descriptor, min(65536, 1048577 - len(source)))
        if not block:
            break
        source.extend(block)
    after = os.fstat(descriptor)
finally:
    os.close(descriptor)

identity = lambda value: (
    value.st_dev,
    value.st_ino,
    value.st_mode,
    value.st_uid,
    value.st_gid,
    value.st_size,
    value.st_mtime_ns,
    value.st_ctime_ns,
)
captured = bytes(source)
if identity(before) != identity(after) or len(captured) != before.st_size:
    raise SystemExit("designated-live-stage-zero: launcher changed while captured")
if hashlib.sha256(captured).hexdigest() != expected:
    raise SystemExit("designated-live-stage-zero: launcher digest mismatch")
sys.argv = [path, *sys.argv[3:]]
scope = {"__name__": "__main__", "__file__": path}
exec(compile(captured, path, "exec"), scope, scope)
"""
STAGE_ZERO_PRELOADER = f"exec({STAGE_ZERO_PRELOADER_SOURCE!r})"
LIVE_SOURCE_REQUIRED = {
    "src/app/CMakeLists.txt",
    "cmake/BprDesignatedLiveStageZero.cmake",
    "cmake/BprTestIsolation.cmake",
    "scripts/designated_live_macos_capsule.py",
}
MAXIMUM_LIVE_SOURCE_MANIFEST_BYTES = 1024 * 1024


@dataclass(frozen=True)
class LiveImageIdentity:
    source_sha256: str
    stage_zero_sha256: str


def sha256_path(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def cmake_sources(root: pathlib.Path) -> list[pathlib.Path]:
    """Read this checkout's sources without descending into nested repositories.

    A nested checkout has its own registrations, whether its .git marker is a
    directory or a worktree file. Prune those roots and generated directories
    before walking them; exclusions apply only below the requested root.
    """
    result: list[pathlib.Path] = []
    for directory, directories, files in os.walk(root):
        base = pathlib.Path(directory)
        directories[:] = [
            name
            for name in directories
            if name not in {".git", "build", "_build"}
            and not (base / name / ".git").exists()
        ]
        for name in files:
            path = base / name
            if path.is_file() and (path.name == "CMakeLists.txt" or path.suffix == ".cmake"):
                result.append(path)
    return sorted(result)


def registration_inventory(root: pathlib.Path) -> tuple[list[str], int, int, int]:
    raw: list[str] = []
    isolated = 0
    live = 0
    exact_live = 0
    for path in cmake_sources(root):
        text = path.read_text(encoding="utf-8", errors="strict")
        if RAW_REGISTRATION.search(text):
            raw.append(path.relative_to(root).as_posix())
        isolated += len(ISOLATED_REGISTRATION.findall(text))
        live += len(LIVE_REGISTRATION.findall(text))
        exact_live += len(EXACT_LIVE_REGISTRATION.findall(text))
    return raw, isolated, live, exact_live


def raw_registration_failures(root: pathlib.Path, raw: list[str]) -> list[str]:
    """Check that every raw `add_test` is the seam's own or an enumerated row.

    A file named by `MACOS_UNSANDBOXED_TESTS` may hold exactly the raw
    registrations enumerated for it, each spelled `NAME <test> COMMAND
    <target>` with nothing else, so a second row or an argument cannot ride
    in on the exception.
    """
    allowed: dict[str, set[str]] = {}
    for name, exception in MACOS_UNSANDBOXED_TESTS.items():
        allowed.setdefault(exception.source, set()).add(
            f"NAME {name} COMMAND {exception.target}"
        )
    unowned = [
        path for path in raw if path != RAW_REGISTRATION_OWNER and path not in allowed
    ]
    if RAW_REGISTRATION_OWNER not in raw or unowned:
        return [
            f"raw add_test ownership is {raw!r}, expected only "
            f"{RAW_REGISTRATION_OWNER} and the enumerated macOS exceptions "
            f"{sorted(allowed)!r}"
        ]
    failures: list[str] = []
    for path in raw:
        if path not in allowed:
            continue
        text = (root / path).read_text(encoding="utf-8", errors="strict")
        calls = RAW_REGISTRATION.findall(text)
        registrations = [
            " ".join(arguments.split())
            for arguments in RAW_REGISTRATION_ARGUMENTS.findall(text)
        ]
        if len(calls) != len(registrations) or sorted(registrations) != sorted(
            allowed[path]
        ):
            failures.append(
                f"raw add_test registrations in {path} are {registrations!r} "
                f"across {len(calls)} calls, expected exactly "
                f"{sorted(allowed[path])!r}"
            )
    return failures


def composition_symbols(
    executable: pathlib.Path, nm: pathlib.Path = MACOS_NM
) -> list[str] | None:
    """The `bpr::app::` symbols an executable carries, or None if unknowable.

    None when the table cannot be read, and when it holds no `bpr::` symbol at
    all: every enumerated row links the adapters it tests, so an empty or
    stripped table would make "no composition symbol" true of anything.
    """
    try:
        listed = subprocess.run(
            [str(nm), "-C", str(executable)],
            capture_output=True,
            text=True,
            errors="replace",
            check=False,
            timeout=300,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if listed.returncode != 0:
        return None
    lines = listed.stdout.splitlines()
    if not any("bpr::" in line for line in lines):
        return None
    return [line for line in lines if COMPOSITION_SYMBOL_NAMESPACE in line]


def macos_unsandboxed_failures(
    name: str,
    command: Any,
    labels: Any,
    prefix: list[str],
    exception: MacosUnsandboxedTest,
    symbol_probe: Callable[[pathlib.Path], list[str] | None] | None,
) -> list[str]:
    """Check an enumerated macOS row that runs outside the sandbox runner."""
    failures: list[str] = []
    if isinstance(command, list) and command[: len(prefix)] == prefix:
        failures.append(
            f"{name} is pinned as running outside the macOS sandbox but is "
            "registered under it; retire it from the pinned set"
        )
        return failures
    if (
        not isinstance(command, list)
        or len(command) != 1
        or not isinstance(command[0], str)
        or pathlib.PurePosixPath(command[0]).name != exception.executable
    ):
        failures.append(
            f"{name} must run only its own executable {exception.executable} "
            f"with no wrapper or argument: {command!r}"
        )
        return failures
    if not isinstance(labels, list) or set(labels) != {MACOS_UNSANDBOXED_LABEL}:
        failures.append(
            f"{name} labels are {labels!r}, expected only {MACOS_UNSANDBOXED_LABEL}"
        )
    symbols = (
        symbol_probe(pathlib.Path(command[0])) if symbol_probe is not None else None
    )
    if symbols is None:
        failures.append(
            f"{name} runs outside the macOS sandbox and its executable could not "
            f"be inspected for {COMPOSITION_SYMBOL_NAMESPACE} symbols"
        )
    elif symbols:
        failures.append(
            f"{name} runs outside the macOS sandbox but links {len(symbols)} "
            f"{COMPOSITION_SYMBOL_NAMESPACE} symbols, so it can compose the "
            f"application: {symbols[0]}"
        )
    return failures


def uncontained_registration_count(
    root: pathlib.Path, *, exclude_pending: bool = False
) -> int:
    """Count the registrations that opt a test out of Windows containment.

    Only the argument list of a `bpr_add_network_isolated_test(...)` call
    counts; the wrapper's own option declaration and comments do not.
    """
    count = 0
    for path in cmake_sources(root):
        text = path.read_text(encoding="utf-8", errors="strict")
        for arguments in ISOLATED_REGISTRATION_ARGUMENTS.findall(text):
            if UNCONTAINED_KEYWORD.search(arguments):
                name = re.search(r"\bNAME\s+(\S+)", arguments)
                registration = (path.relative_to(root).as_posix(), name[1] if name else "")
                if exclude_pending and registration in WINDOWS_PENDING_REGISTRATIONS:
                    continue
                count += 1
    return count


def pin_registration_failures(manifest: dict[str, Any], *, windows: bool) -> list[str]:
    """[[BPR-1343]]. Every WINDOWS_UNCONTAINED registration has its Windows pin.

    The wrapper labels such a row on every platform, so a macOS or Linux author
    learns at once that the row needs an entry in WINDOWS_UNCONTAINED_TESTS.
    An unmeasured pin is accepted there; on Windows it is refused until the
    row has been measured contained and its measured refusal recorded.
    """
    failures: list[str] = []
    tests = manifest.get("tests")
    if not isinstance(tests, list):
        return failures
    for test in tests:
        name = test.get("name")
        labels = property_value(test, "LABELS")
        if not isinstance(name, str) or not isinstance(labels, list):
            continue
        if WINDOWS_UNCONTAINED_REGISTRATION_LABEL not in labels:
            continue
        pin = WINDOWS_UNCONTAINED_TESTS.get(name)
        if pin is None:
            failures.append(
                f"{name} is registered WINDOWS_UNCONTAINED without an entry in "
                "WINDOWS_UNCONTAINED_TESTS; add one in the same commit, "
                f'"{UNMEASURED_PIN_PREFIX}<platform>)" until Windows measures it'
            )
        elif windows and pin.startswith(UNMEASURED_PIN_PREFIX):
            failures.append(
                f"{name} is pinned {pin!r}; measure it contained on Windows and "
                "record its measured refusal"
            )
    return failures


def property_value(test: dict[str, Any], name: str) -> Any | None:
    for candidate in test.get("properties", []):
        if candidate.get("name") == name:
            return candidate.get("value")
    return None


def exit_verdict_failures(test: dict[str, Any], name: str) -> list[str]:
    # The ordinary registration seam adds these exact failure-only guards.
    # Neither can turn a failed process into a pass; the live lane still
    # accepts no verdict property because its reviewed command has a separate
    # owner. The value is compared whole, so a row that keeps the property name
    # while widening what it matches is not mistaken for the inherited one.
    present = sorted(
        candidate.get("name")
        for candidate in test.get("properties", [])
        if candidate.get("name") in FORBIDDEN_EXIT_VERDICT_PROPERTIES
        and not (
            name != LIVE_TEST_NAME
            and candidate.get("name") == "FAIL_REGULAR_EXPRESSION"
            and candidate.get("value") == INHERITED_FAILURE_EXPRESSIONS
        )
    )
    return (
        [f"{name} must use process exit as its only verdict; found {present!r}"]
        if present
        else []
    )


def inherited_verdict_failures(
    manifest: dict[str, Any],
    macos_unsandboxed: dict[str, MacosUnsandboxedTest] | None = None,
) -> list[str]:
    """Check that every configured row still carries the seam's own verdict.

    `cmake/BprTestIsolation.cmake` claims a row added later inherits the
    fail-closed verdict and that the guarantee cannot drift. Nothing enforced
    that claim: a row-level `set_tests_properties(... FAIL_REGULAR_EXPRESSION
    ...)` replaces the property outright and the manifest gate passed anyway.
    This asserts the property's presence and its exact value on every row,
    which is where such a replacement becomes visible.

    Three kinds of row are exempt, each for a reason that is written down
    rather than inferred: the live lane, which accepts no verdict property at
    all; the rows labelled `bpr_expects_failed_run`, whose subject is a failed
    run and which keep only the workspace refusal entry; and the enumerated
    macOS rows registered outside the seam, which cannot inherit anything and
    are held to the stronger rule instead - no pass expression, so their exit
    status is the only thing that decides them. Independently of those, a row
    labelled `bpr_expects_workspace_recovery_refusal` drops the workspace
    refusal entry and keeps the rest (BPR-1050).
    """
    failures: list[str] = []
    exempt = set(macos_unsandboxed or {})
    tests = manifest.get("tests")
    if not isinstance(tests, list):
        return failures
    for test in tests:
        name = test.get("name", "<unnamed>")
        if name == LIVE_TEST_NAME:
            continue
        if name in exempt:
            if property_value(test, "PASS_REGULAR_EXPRESSION") is not None:
                failures.append(
                    f"{name} runs outside the registration seam and so inherits "
                    "no failure verdict; it must keep process exit as its only "
                    "verdict and carry no PASS_REGULAR_EXPRESSION"
                )
            continue
        value = property_value(test, "FAIL_REGULAR_EXPRESSION")
        labels = property_value(test, "LABELS") or []
        labels = labels if isinstance(labels, list) else []
        refusal_exempt = EXPECTS_WORKSPACE_RECOVERY_REFUSAL_LABEL in labels
        if refusal_exempt and name not in EXPECTS_WORKSPACE_RECOVERY_REFUSAL_TESTS:
            failures.append(
                f"{name} takes the {EXPECTS_WORKSPACE_RECOVERY_REFUSAL_LABEL} "
                "exemption without being recorded in "
                "EXPECTS_WORKSPACE_RECOVERY_REFUSAL_TESTS"
            )

        def expected(expressions: list[str]) -> list[str] | None:
            kept = [
                expression
                for expression in expressions
                if not (refusal_exempt and expression == WORKSPACE_REFUSAL_EXPRESSION)
            ]
            return kept or None

        if EXPECTS_FAILED_RUN_LABEL in labels and EXPECTS_NONZERO_EXIT_LABEL in labels:
            failures.append(
                f"{name} carries both {EXPECTS_FAILED_RUN_LABEL} and "
                f"{EXPECTS_NONZERO_EXIT_LABEL}; the first already covers the second"
            )
            continue
        if EXPECTS_FAILED_RUN_LABEL in labels:
            if name not in EXPECTS_FAILED_RUN_TESTS:
                failures.append(
                    f"{name} takes the {EXPECTS_FAILED_RUN_LABEL} exemption "
                    "without being recorded in EXPECTS_FAILED_RUN_TESTS"
                )
            if value != expected(EXPECTS_FAILED_RUN_EXPRESSIONS):
                failures.append(
                    f"{name} is an expected-failure row and must carry no "
                    "verdict expression, only "
                    f"{expected(EXPECTS_FAILED_RUN_EXPRESSIONS)!r}; found {value!r}"
                )
            continue
        if EXPECTS_NONZERO_EXIT_LABEL in labels:
            if name not in EXPECTS_NONZERO_EXIT_TESTS:
                failures.append(
                    f"{name} takes the {EXPECTS_NONZERO_EXIT_LABEL} exemption "
                    "without being recorded in EXPECTS_NONZERO_EXIT_TESTS"
                )
            if value != expected(EXPECTS_NONZERO_EXIT_EXPRESSIONS):
                failures.append(
                    f"{name} asserts its own non-zero exit and must still carry "
                    f"{expected(EXPECTS_NONZERO_EXIT_EXPRESSIONS)!r}; found {value!r}"
                )
            continue
        if name in EXPECTS_NONZERO_EXIT_TESTS or name in EXPECTS_FAILED_RUN_TESTS:
            failures.append(
                f"{name} is recorded as an exempt row but carries neither "
                f"{EXPECTS_FAILED_RUN_LABEL} nor {EXPECTS_NONZERO_EXIT_LABEL}"
            )
            continue
        if value != expected(INHERITED_FAILURE_EXPRESSIONS):
            failures.append(
                f"{name} must carry the inherited fail-closed verdict "
                f"{expected(INHERITED_FAILURE_EXPRESSIONS)!r}; found {value!r}"
            )
    return failures


def environment_injection_failures(test: dict[str, Any], name: str) -> list[str]:
    failures: list[str] = []
    dangerous = set(PRE_BOUNDARY_INJECTION_VARIABLES)
    environment = property_value(test, "ENVIRONMENT")
    if isinstance(environment, list):
        for entry in environment:
            variable = entry.split("=", 1)[0] if isinstance(entry, str) else ""
            if variable in dangerous:
                failures.append(f"{name} sets pre-boundary variable {variable}")

    modifications = property_value(test, "ENVIRONMENT_MODIFICATION")
    expected = {f"{variable}=unset:" for variable in PRE_BOUNDARY_INJECTION_VARIABLES}
    actual = set(modifications) if isinstance(modifications, list) else set()
    missing = sorted(expected - actual)
    if missing:
        failures.append(f"{name} lacks pre-boundary environment scrubs: {missing!r}")
    for entry in actual:
        variable = entry.split("=", 1)[0]
        if variable in dangerous and entry not in expected:
            failures.append(f"{name} modifies pre-boundary variable: {entry}")
    return failures


def boundary_marker_failures(test: dict[str, Any], name: str) -> list[str]:
    """Check that a row tells its child process it is inside the boundary."""
    failures: list[str] = []
    environment = property_value(test, "ENVIRONMENT")
    if isinstance(environment, list):
        for entry in environment:
            variable = entry.split("=", 1)[0] if isinstance(entry, str) else ""
            if variable == AUTOMATED_TEST_BOUNDARY_VARIABLE:
                failures.append(
                    f"{name} carries the boundary marker in ENVIRONMENT, where "
                    "a row-level write erases it"
                )

    modifications = property_value(test, "ENVIRONMENT_MODIFICATION")
    ordered = [
        entry for entry in modifications if isinstance(entry, str)
    ] if isinstance(modifications, list) else []
    actual = set(ordered)
    if AUTOMATED_TEST_BOUNDARY_MODIFICATION not in actual:
        failures.append(
            f"{name} lacks the automated-test boundary marker "
            f"{AUTOMATED_TEST_BOUNDARY_MODIFICATION}"
        )
    # The product-run row is the one row allowed a second entry, and only the
    # erasure, and only after the seam's own marker: CTest applies the list in
    # order, so `unset:` before `set:1` would leave the row inside after all.
    erasure_allowed = name == AUTOMATED_TEST_BOUNDARY_PRODUCT_RUN_TEST
    if erasure_allowed:
        marker_at = (
            ordered.index(AUTOMATED_TEST_BOUNDARY_MODIFICATION)
            if AUTOMATED_TEST_BOUNDARY_MODIFICATION in actual
            else -1
        )
        erasure_at = (
            ordered.index(AUTOMATED_TEST_BOUNDARY_ERASURE)
            if AUTOMATED_TEST_BOUNDARY_ERASURE in actual
            else -1
        )
        if erasure_at < 0:
            failures.append(
                f"{name} is the product-run row and must erase the boundary "
                f"marker with {AUTOMATED_TEST_BOUNDARY_ERASURE}"
            )
        elif marker_at >= 0 and erasure_at < marker_at:
            failures.append(
                f"{name} erases the boundary marker before the seam sets it, "
                "so the row would run inside the boundary after all"
            )
    for entry in sorted(actual):
        variable = entry.split("=", 1)[0]
        if variable != AUTOMATED_TEST_BOUNDARY_VARIABLE:
            continue
        if entry == AUTOMATED_TEST_BOUNDARY_MODIFICATION:
            continue
        if erasure_allowed and entry == AUTOMATED_TEST_BOUNDARY_ERASURE:
            continue
        failures.append(f"{name} modifies the boundary marker: {entry}")
    return failures


def live_credential_failures(test: dict[str, Any], name: str) -> list[str]:
    failures: list[str] = []
    credentials = set(LIVE_CREDENTIAL_VARIABLES)
    environment = property_value(test, "ENVIRONMENT")
    if isinstance(environment, list):
        for entry in environment:
            variable = entry.split("=", 1)[0] if isinstance(entry, str) else ""
            if variable in credentials:
                failures.append(f"{name} sets ambient credential {variable}")

    modifications = property_value(test, "ENVIRONMENT_MODIFICATION")
    expected = {f"{variable}=unset:" for variable in LIVE_CREDENTIAL_VARIABLES}
    actual = set(modifications) if isinstance(modifications, list) else set()
    missing = sorted(expected - actual)
    if missing:
        failures.append(f"{name} lacks ambient credential scrubs: {missing!r}")
    for entry in actual:
        variable = entry.split("=", 1)[0]
        if variable in credentials and entry not in expected:
            failures.append(f"{name} modifies ambient credential: {entry}")
    return failures


def expected_prefix(
    backend: str,
    profile: pathlib.Path | None,
    linux_launcher: pathlib.Path | None,
    windows_launcher: pathlib.Path | None,
    python: pathlib.Path | None,
    macos_runner: pathlib.Path | None,
) -> list[str]:
    if backend == "macos-sandbox-exec":
        if profile is None or python is None or macos_runner is None:
            raise ValueError("macOS backend requires profile, Python, and runner")
        return [
            str(python.absolute()),
            "-I",
            "-S",
            str(macos_runner.resolve()),
            "--sandbox-exec",
            "/usr/bin/sandbox-exec",
            "--profile-template",
            str(profile.resolve()),
        ]
    if backend == "linux-network-namespace":
        if linux_launcher is None:
            raise ValueError("Linux backend requires a launcher")
        return [str(linux_launcher.resolve())]
    if backend == "windows-appcontainer":
        if windows_launcher is None:
            raise ValueError("Windows backend requires a launcher")
        return [str(windows_launcher.resolve())]
    raise ValueError(f"unknown backend: {backend}")


def manifest_command(
    ctest: pathlib.Path, build_dir: pathlib.Path, build_config: str | None
) -> list[str]:
    """The `ctest --show-only` invocation that yields the configured manifest.

    A multi-configuration generator lists nothing until told which
    configuration, so the registration names the one it runs in.
    """
    command = [str(ctest), "--test-dir", str(build_dir)]
    if build_config:
        command.extend(["--build-config", build_config])
    command.append("--show-only=json-v1")
    return command


def comparable_command(value: Any) -> Any:
    """Compare a configured command the way the host compares paths.

    CTest writes every path in its JSON manifest with forward slashes, while
    `pathlib` resolves one with the platform separator. On Windows that makes
    the very same program compare unequal on separator alone, which reads as a
    substituted command when nothing was substituted. Windows treats the two
    separators as one path, so folding them keeps this comparison exact in
    meaning rather than merely looser, and no other platform is affected.
    """
    if os.name != "nt" or not isinstance(value, list):
        return value
    return [
        item.replace("\\", "/") if isinstance(item, str) else item for item in value
    ]


def configured_failures(
    manifest: dict[str, Any],
    prefix: list[str],
    macos_runner: bool = False,
    live_command: list[str] | None = None,
    required_commands: dict[str, list[str]] | None = None,
    windows_uncontained: frozenset[str] = frozenset(),
    macos_unsandboxed: dict[str, MacosUnsandboxedTest] | None = None,
    symbol_probe: Callable[[pathlib.Path], list[str] | None] | None = None,
) -> list[str]:
    """Check every configured test against the boundary.

    `windows_uncontained` is the exact set of test names permitted to run
    without the launcher prefix; it is non-empty only for the Windows backend.
    `macos_unsandboxed` is the enumerated set permitted to run without the
    sandbox runner, each checked through `symbol_probe`; it is given only for
    the macOS backend.
    """
    macos_unsandboxed = macos_unsandboxed or {}
    failures: list[str] = []
    tests = manifest.get("tests")
    if not isinstance(tests, list) or not tests:
        return ["CTest JSON contains no tests"]
    prefix = comparable_command(prefix)
    live_command = comparable_command(live_command)
    if required_commands is not None:
        required_commands = {
            name: comparable_command(command)
            for name, command in required_commands.items()
        }
    live_named = [test for test in tests if test.get("name") == LIVE_TEST_NAME]
    if live_command is None:
        if live_named:
            failures.append(f"{LIVE_TEST_NAME} is configured while the live lane is disabled")
    elif len(live_named) != 1:
        failures.append(
            f"configured live exception count is {len(live_named)}, expected exactly one"
        )

    if required_commands is not None:
        if set(required_commands) != REQUIRED_COMMAND_BOUND_TESTS:
            failures.append(
                "required command bindings must contain the exact protected pair: "
                f"{sorted(required_commands)!r}"
            )
        for required_name in sorted(REQUIRED_COMMAND_BOUND_TESTS):
            expected_command = required_commands.get(required_name)
            named = [test for test in tests if test.get("name") == required_name]
            if len(named) != 1:
                failures.append(
                    f"protected test {required_name} count is {len(named)}, expected one"
                )
            elif comparable_command(named[0].get("command")) != expected_command:
                failures.append(
                    f"protected test {required_name} does not use its exact command: "
                    f"{named[0].get('command')!r}"
                )
            if expected_command is not None:
                renamed = [
                    test.get("name", "<unnamed>")
                    for test in tests
                    if test.get("name") != required_name
                    and comparable_command(test.get("command")) == expected_command
                ]
                if renamed:
                    failures.append(
                        f"protected command for {required_name} is registered under "
                        f"other names: {renamed!r}"
                    )

    for test in tests:
        name = test.get("name", "<unnamed>")
        failures.extend(environment_injection_failures(test, name))
        if name in EXIT_VERDICT_TESTS:
            failures.extend(exit_verdict_failures(test, name))
        command = comparable_command(test.get("command"))
        labels = property_value(test, "LABELS")
        if name == LIVE_TEST_NAME:
            if live_command is not None and command != live_command:
                failures.append(
                    f"{name} does not use the exact runner/approval command: {command!r}"
                )
            if not isinstance(labels, list) or set(labels) != {LIVE_TEST_LABEL}:
                failures.append(
                    f"{name} labels are {labels!r}, expected only {LIVE_TEST_LABEL}"
                )
            if property_value(test, "TIMEOUT") != 1800.0:
                failures.append(f"{name} must retain its exact 1800-second timeout")
            failures.extend(live_credential_failures(test, name))
            continue
        # Every row past this point runs inside the automated boundary and has
        # to say so. The live lane above is the one row that must not: it drives
        # the shipped application against real GitHub on purpose, from an image
        # this file already requires to be built with BUILD_TESTING=OFF, and a
        # marker there would hand it the recording stand-ins it exists to avoid.
        failures.extend(boundary_marker_failures(test, name))
        if isinstance(labels, list) and LIVE_TEST_LABEL in labels:
            failures.append(
                f"{name} uses {LIVE_TEST_LABEL}; only {LIVE_TEST_NAME} may be live"
            )
        uncontained_labelled = isinstance(labels, list) and WINDOWS_UNCONTAINED_LABEL in labels
        if name in windows_uncontained:
            if isinstance(command, list) and command[: len(prefix)] == prefix:
                failures.append(
                    f"{name} is pinned as {WINDOWS_UNCONTAINED_LABEL} but is registered "
                    "contained; retire it from the pinned set"
                )
            if not uncontained_labelled:
                failures.append(f"{name} lacks the {WINDOWS_UNCONTAINED_LABEL} label")
            if isinstance(labels, list) and "bpr_network_isolated" in labels:
                failures.append(
                    f"{name} carries bpr_network_isolated while running uncontained"
                )
            continue
        if name in macos_unsandboxed:
            failures.extend(
                macos_unsandboxed_failures(
                    name,
                    command,
                    labels,
                    prefix,
                    macos_unsandboxed[name],
                    symbol_probe,
                )
            )
            continue
        if uncontained_labelled:
            failures.append(
                f"{name} uses {WINDOWS_UNCONTAINED_LABEL} but is not in the pinned "
                "Windows set"
            )
        if not isinstance(command, list) or command[: len(prefix)] != prefix:
            failures.append(f"{name} does not start with {prefix!r}: {command!r}")
        elif macos_runner:
            tail = command[len(prefix) :]
            if (
                len(tail) < 4
                or tail[0] != "--loopback-port-count"
                or not tail[1].isdigit()
                or int(tail[1]) > 8
            ):
                failures.append(f"{name} has invalid runner authority: {tail!r}")
            else:
                delimiter = 3 if tail[2:3] == ["--unrelated-endpoint-probe"] else 2
                if tail[delimiter : delimiter + 1] != ["--"] or len(tail) <= delimiter + 1:
                    failures.append(f"{name} has no runner command delimiter: {tail!r}")
        if not isinstance(labels, list) or "bpr_network_isolated" not in labels:
            failures.append(f"{name} lacks the bpr_network_isolated label")
        if macos_runner and isinstance(command, list) and command[: len(prefix)] == prefix:
            tail = command[len(prefix) :]
            if len(tail) >= 2 and tail[0] == "--loopback-port-count" and tail[1].isdigit():
                count = int(tail[1])
                has_fixture_label = isinstance(labels, list) and "bpr_loopback_fixture" in labels
                if (count > 0) != has_fixture_label:
                    failures.append(f"{name} loopback count and label disagree")
                expected_count = MACOS_LOOPBACK_AUTHORITIES.get(name, 0)
                if count != expected_count:
                    failures.append(
                        f"{name} has loopback count {count}, expected {expected_count}"
                    )
                has_unrelated_flag = tail[2:3] == ["--unrelated-endpoint-probe"]
                has_unrelated_label = (
                    isinstance(labels, list)
                    and "bpr_unrelated_endpoint_probe" in labels
                )
                expected_unrelated = name == MACOS_UNRELATED_ENDPOINT_PROBE
                if has_unrelated_flag != expected_unrelated:
                    failures.append(f"{name} unrelated-probe flag is incorrect")
                if has_unrelated_label != expected_unrelated:
                    failures.append(f"{name} unrelated-probe label is incorrect")
    return failures


def inspect_live_image(
    image: pathlib.Path,
    expected_sha256: str,
    mount_parent: pathlib.Path,
    build_dir: pathlib.Path,
    root: pathlib.Path,
    approval: pathlib.Path,
) -> tuple[LiveImageIdentity | None, list[str]]:
    """Validate the tests-disabled producer and exact read-only image inputs."""
    failures: list[str] = []
    if not hasattr(os, "geteuid"):
        # The identity contract below is written in POSIX owner terms, and the
        # designated live lane exists only on macOS. Refuse rather than crash
        # on a platform that cannot express the check.
        return None, [
            "live image identity requires POSIX owner semantics this "
            "platform does not provide"
        ]
    if not all(
        path.is_absolute()
        for path in (image, mount_parent, build_dir, approval)
    ):
        return None, [
            "live image, mount parent, producer build, and approval must be absolute"
        ]
    if len(expected_sha256) != 64 or any(
        character not in "0123456789abcdef" for character in expected_sha256
    ):
        return None, ["live image SHA-256 is not a literal canonical digest"]
    try:
        resolved_image = image.resolve(strict=True)
        resolved_mount_parent = mount_parent.resolve(strict=True)
        resolved_build = build_dir.resolve(strict=True)
        resolved_approval = approval.resolve(strict=True)
    except OSError as error:
        return None, [f"live image producer input is unavailable: {error}"]
    if (
        image != resolved_image
        or mount_parent != resolved_mount_parent
        or build_dir != resolved_build
        or approval != resolved_approval
    ):
        failures.append("live image producer paths must be resolved and immutable")
    if image.is_symlink() or not resolved_image.is_file():
        failures.append("live image is not a non-symlink regular file")
    if mount_parent.is_symlink() or not resolved_mount_parent.is_dir():
        failures.append("live mount parent is not a non-symlink directory")
    else:
        mount_status = resolved_mount_parent.stat()
        if mount_status.st_uid != os.geteuid() or mount_status.st_mode & 0o077:
            failures.append("live mount parent is not owner-private")
    if approval.is_symlink() or not resolved_approval.is_file():
        failures.append("live approval is not a non-symlink regular file")
    else:
        approval_status = resolved_approval.stat()
        parent_status = resolved_approval.parent.stat()
        if (
            approval_status.st_uid != os.geteuid()
            or approval_status.st_mode & 0o077
            or parent_status.st_uid != os.geteuid()
            or parent_status.st_mode & 0o077
        ):
            failures.append("live approval is not external and owner-private")
    try:
        resolved_image.relative_to(resolved_build)
    except ValueError:
        failures.append("live image is outside its explicit producer build directory")
    for private_scope in (resolved_build, resolved_mount_parent):
        try:
            resolved_approval.relative_to(private_scope)
        except ValueError:
            continue
        failures.append("live approval is not external to producer and mount paths")

    if resolved_image.is_file():
        observed_image_sha256 = sha256_path(resolved_image)
        if observed_image_sha256 != expected_sha256:
            failures.append("live image digest differs from its literal expected SHA-256")

    cache = resolved_build / "CMakeCache.txt"
    if not cache.is_file():
        failures.append("live producer build has no CMakeCache.txt")
        return None, failures
    text = cache.read_text(encoding="utf-8", errors="strict")
    required_cache = {
        "BUILD_TESTING:BOOL": "OFF",
        "BPR_BUILD_QT:BOOL": "ON",
        "BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE:BOOL": "ON",
        "CMAKE_BUILD_TYPE:STRING": "Release",
        "CMAKE_HOME_DIRECTORY:INTERNAL": str(root.resolve()),
    }
    for key, expected in required_cache.items():
        match = re.search(rf"^{re.escape(key)}=(.*)$", text, re.MULTILINE)
        if match is None or match.group(1) != expected:
            failures.append(
                f"live producer cache {key} is "
                f"{None if match is None else match.group(1)!r}, expected {expected!r}"
            )

    source_manifest = resolved_image.parent / "source-manifest.txt"
    source_sha256: str | None = None
    stage_zero_sha256: str | None = None
    if source_manifest.is_symlink() or not source_manifest.is_file():
        failures.append("live producer source-manifest.txt is unavailable")
    else:
        with source_manifest.open("rb") as source:
            source_bytes = source.read(MAXIMUM_LIVE_SOURCE_MANIFEST_BYTES + 1)
        if (
            not source_bytes
            or len(source_bytes) > MAXIMUM_LIVE_SOURCE_MANIFEST_BYTES
            or not source_bytes.endswith(b"\n")
            or b"\r" in source_bytes
            or b"\n\n" in source_bytes
        ):
            failures.append(
                "live producer source manifest framing is not bounded canonical ASCII/LF"
            )
        source_sha256 = hashlib.sha256(source_bytes).hexdigest()
        if source_sha256 != resolved_image.parent.name:
            failures.append(
                "live producer source manifest digest differs from source identity"
            )
        seen: set[str] = set()
        try:
            lines = source_bytes.removesuffix(b"\n").decode("ascii").split("\n")
        except UnicodeDecodeError:
            lines = []
            failures.append("live producer source manifest is not ASCII")
        for line in lines:
            match = re.fullmatch(r"([A-Za-z0-9_./+-]+)=([0-9a-f]{64})", line)
            if match is None:
                failures.append("live producer source manifest entry is malformed")
                continue
            relative, digest = match.groups()
            pure = pathlib.PurePosixPath(relative)
            if (
                pure.is_absolute()
                or ".." in pure.parts
                or relative in seen
                or pure.as_posix() != relative
            ):
                failures.append("live producer source manifest entry is unsafe")
                continue
            seen.add(relative)
            current = root / relative
            if current.is_symlink() or not current.is_file():
                failures.append(f"live current source entry is unavailable: {relative}")
                continue
            current_digest = sha256_path(current)
            if current_digest != digest:
                failures.append(f"live current source differs from producer: {relative}")
            if relative == "scripts/designated_live_macos_capsule.py":
                stage_zero_sha256 = digest
        for required in sorted(LIVE_SOURCE_REQUIRED - seen):
            failures.append(f"live producer source manifest lacks {required}")

    source_candidates = [
        path
        for base in (root / "src", root / "cmake")
        if base.is_dir()
        for path in base.rglob("*")
        if path.is_file()
    ]
    source_candidates.extend(
        path
        for path in (root / "CMakeLists.txt", root / "CMakePresets.json")
        if path.is_file()
    )
    if not source_candidates:
        failures.append("live freshness inventory found no product source")
    elif resolved_image.stat().st_mtime_ns < max(
        path.stat().st_mtime_ns for path in source_candidates
    ):
        failures.append("live image predates product source or build configuration")
    identity = (
        LiveImageIdentity(source_sha256, stage_zero_sha256)
        if source_sha256 is not None and stage_zero_sha256 is not None
        else None
    )
    return identity, failures


def live_image_failures(
    image: pathlib.Path,
    expected_sha256: str,
    mount_parent: pathlib.Path,
    build_dir: pathlib.Path,
    root: pathlib.Path,
    approval: pathlib.Path,
) -> list[str]:
    return inspect_live_image(
        image,
        expected_sha256,
        mount_parent,
        build_dir,
        root,
        approval,
    )[1]


def live_stage_zero_command(
    root: pathlib.Path,
    image: pathlib.Path,
    expected_sha256: str,
    mount_parent: pathlib.Path,
    approval: pathlib.Path,
    identity: LiveImageIdentity,
    profile: str | None = None,
) -> list[str]:
    command = [
        "/usr/bin/python3",
        "-I",
        "-S",
        "-c",
        STAGE_ZERO_PRELOADER,
        str((root / "scripts/designated_live_macos_capsule.py").absolute()),
        identity.stage_zero_sha256,
        "stage-zero",
        "--image",
        str(image.absolute()),
        "--expected-image-sha256",
        expected_sha256,
        "--mount-parent",
        str(mount_parent.absolute()),
        "--source-sha256",
        identity.source_sha256,
        "--attended-launch",
        "--approval",
        str(approval.absolute()),
    ]
    if profile is not None:
        if not re.fullmatch(r"[1-9][0-9]*", profile) or int(profile) > (1 << 64) - 1:
            raise ValueError("live profile must be a canonical positive uint64")
        command.extend(["--bpr-profile", profile])
    return command


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=pathlib.Path)
    parser.add_argument("--build-dir", required=True, type=pathlib.Path)
    parser.add_argument("--ctest", required=True, type=pathlib.Path)
    parser.add_argument("--cmake", required=True, type=pathlib.Path)
    parser.add_argument("--backend", required=True)
    # A multi-configuration generator registers its tests per configuration,
    # and `ctest --show-only` lists nothing for such a tree until told which
    # one; without this the gate saw an empty manifest on Windows (BPR-0432).
    parser.add_argument("--build-config")
    parser.add_argument("--profile", type=pathlib.Path)
    parser.add_argument("--python", type=pathlib.Path)
    parser.add_argument("--macos-runner", type=pathlib.Path)
    parser.add_argument("--linux-launcher", type=pathlib.Path)
    parser.add_argument("--windows-launcher", type=pathlib.Path)
    parser.add_argument("--live-image", type=pathlib.Path)
    parser.add_argument("--live-image-sha256")
    parser.add_argument("--live-mount-parent", type=pathlib.Path)
    parser.add_argument("--live-build-dir", type=pathlib.Path)
    parser.add_argument("--live-approval", type=pathlib.Path)
    parser.add_argument("--live-profile")
    arguments = parser.parse_args()

    raw, isolated, live, exact_live = registration_inventory(arguments.root)
    failures: list[str] = raw_registration_failures(arguments.root, raw)
    if isolated < 1:
        failures.append("no isolated test registrations were found")
    if live != 1 or exact_live != 1:
        failures.append(
            "designated live source registration must be exactly one empty fixed call: "
            f"calls={live}, exact={exact_live}"
        )
    # A registration inside a foreach covers several pinned names, so the
    # source count bounds the set from above rather than matching it; the
    # manifest check below pins every name exactly, in both directions.
    uncontained = uncontained_registration_count(arguments.root, exclude_pending=True)
    if uncontained == 0 or uncontained > len(WINDOWS_UNCONTAINED_TESTS):
        failures.append(
            f"WINDOWS_UNCONTAINED has {uncontained} registrations outside the explicit pending inventory, "
            f"expected between one and {len(WINDOWS_UNCONTAINED_TESTS)}, the "
            "size of the pinned set"
        )

    shown = subprocess.run(
        manifest_command(arguments.ctest, arguments.build_dir, arguments.build_config),
        capture_output=True,
        text=True,
        check=False,
    )
    if shown.returncode != 0:
        failures.append(f"ctest manifest failed: {shown.stderr.strip()}")
        manifest: dict[str, Any] = {"tests": []}
    else:
        try:
            manifest = json.loads(shown.stdout)
        except json.JSONDecodeError as error:
            failures.append(f"ctest manifest is not JSON: {error}")
            manifest = {"tests": []}
    failures.extend(
        pin_registration_failures(manifest, windows=arguments.backend == WINDOWS_BACKEND)
    )

    try:
        prefix = expected_prefix(
            arguments.backend,
            arguments.profile,
            arguments.linux_launcher,
            arguments.windows_launcher,
            arguments.python,
            arguments.macos_runner,
        )
    except ValueError as error:
        failures.append(str(error))
        prefix = []
    if prefix:
        live_arguments = (
            arguments.live_image,
            arguments.live_image_sha256,
            arguments.live_mount_parent,
            arguments.live_build_dir,
            arguments.live_approval,
        )
        if any(value is not None for value in live_arguments) and not all(
            value is not None for value in live_arguments
        ):
            failures.append(
                "live image, literal digest, mount parent, producer build, and approval arguments must be supplied together"
            )
            live_command = None
        elif all(value is not None for value in live_arguments):
            identity, live_failures = inspect_live_image(
                arguments.live_image,
                arguments.live_image_sha256,
                arguments.live_mount_parent,
                arguments.live_build_dir,
                arguments.root,
                arguments.live_approval,
            )
            failures.extend(live_failures)
            live_command = (
                live_stage_zero_command(
                    arguments.root,
                    arguments.live_image,
                    arguments.live_image_sha256,
                    arguments.live_mount_parent,
                    arguments.live_approval,
                    identity,
                    arguments.live_profile,
                )
                if identity is not None
                else None
            )
        else:
            live_command = None
        if arguments.python is None:
            failures.append("exact protected command binding requires Python")
            required_commands = None
        else:
            protected_prefix = (
                [*prefix, "--loopback-port-count", "0", "--"]
                if arguments.backend == "macos-sandbox-exec"
                else prefix
            )
            required_commands = {
                "architecture_boundaries": [
                    *protected_prefix,
                    str(arguments.cmake.resolve()),
                    f"-DBPR_ROOT={arguments.root.resolve()}",
                    "-P",
                    str(
                        (
                            arguments.root
                            / "cmake"
                            / "VerifyArchitectureBoundaries.cmake"
                        ).resolve()
                    ),
                ],
                "architecture_boundary_contract": [
                    *protected_prefix,
                    str(arguments.python.absolute()),
                    str(
                        (
                            arguments.root
                            / "tests"
                            / "scripts"
                            / "architecture_boundary_tests.py"
                        ).resolve()
                    ),
                ],
            }
        macos_unsandboxed = (
            MACOS_UNSANDBOXED_TESTS
            if arguments.backend == "macos-sandbox-exec"
            else None
        )
        failures.extend(
            configured_failures(
                manifest,
                prefix,
                macos_runner=arguments.backend == "macos-sandbox-exec",
                live_command=live_command,
                required_commands=required_commands,
                windows_uncontained=(
                    frozenset(WINDOWS_UNCONTAINED_TESTS)
                    if arguments.backend == WINDOWS_BACKEND
                    else frozenset()
                ),
                macos_unsandboxed=macos_unsandboxed,
                symbol_probe=composition_symbols,
            )
        )
        failures.extend(
            inherited_verdict_failures(
                manifest,
                macos_unsandboxed=macos_unsandboxed,
            )
        )

    if failures:
        for failure in failures:
            print(f"TEST_NETWORK_ISOLATION_FAILED:{failure}", flush=True)
        return 1
    print(
        "TEST_NETWORK_ISOLATION_OK:"
        f"configured={len(manifest['tests'])}:registrations={isolated}:"
        f"backend={arguments.backend}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
